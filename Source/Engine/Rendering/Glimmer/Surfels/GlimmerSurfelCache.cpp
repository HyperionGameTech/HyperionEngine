/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/Surfels/GlimmerSurfelCache.hpp>
#include <Rendering/Glimmer/Surfels/GlimmerSurfelCVars.hpp>
#include <Rendering/Glimmer/GlimmerSceneInstances.hpp>
#include <Rendering/Glimmer/GlimmerTriangleCache.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/GlimmerLocalLights.hpp>
#include <Rendering/Glimmer/GlimmerRelight.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHVolume.hpp>
#include <Rendering/Glimmer/Bricks/GlimmerBrickWorld.hpp>

#include <Rendering/Clouds/CloudPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Scene/EnvProbe.hpp>
#include <Scene/Sky/TimeOfDay.hpp>

#include <Framework/EngineStats.hpp>

#include <Core/HashCode.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statGlimmerSurfels("Rendering/GPU/Glimmer/Surfels");
static EngineStatGpuTimer s_statGlimmerSurfelPlace("Rendering/GPU/Glimmer/SurfelPlace");
static EngineStatGpuTimer s_statGlimmerSurfelTrace("Rendering/GPU/Glimmer/SurfelTrace");
static EngineStatGpuTimer s_statGlimmerSurfelShade("Rendering/GPU/Glimmer/SurfelShade");
static EngineStatGpuTimer s_statGlimmerSurfelBlend("Rendering/GPU/Glimmer/SurfelBlend");
static EngineStatGpuTimer s_statGlimmerSurfelCompose("Rendering/GPU/Glimmer/SurfelCompose");
static EngineStatGpuTimer s_statGlimmerRelight("Rendering/GPU/Glimmer/Relight");

static EngineStatCounter<uint32> s_statGlimmerRelightTexels("Rendering/Glimmer/RelightTexels");
static EngineStatCounter<uint32> s_statGlimmerSurfelPlacements("Rendering/Glimmer/SurfelPlacements");
static EngineStatCounter<uint32> s_statGlimmerSurfelWakes("Rendering/Glimmer/SurfelWakes");

// read back from the GPU counters
static EngineStatCounter<uint32> s_statGlimmerSurfelCount("Rendering/Glimmer/Surfels/Count", false);
static EngineStatCounter<uint32> s_statGlimmerSurfelsDropped("Rendering/Glimmer/Surfels/Dropped", false);
static EngineStatCounter<uint32> s_statGlimmerSurfelsTraced("Rendering/Glimmer/Surfels/Traced", false);
static EngineStatCounter<uint32> s_statGlimmerFoliageVoxelCount("Rendering/Glimmer/Surfels/FoliageVoxels", false);

static StaticShaderPropertyId s_propPlaceModeClear { ShaderProperty(NAME("MODE"), NAME("CLEAR")) };
static StaticShaderPropertyId s_propPlaceModePlace { ShaderProperty(NAME("MODE"), NAME("PLACE")) };
static StaticShaderPropertyId s_propPlaceModeAllocate { ShaderProperty(NAME("MODE"), NAME("ALLOCATE")) };
static StaticShaderPropertyId s_propTraceModeTrace { ShaderProperty(NAME("MODE"), NAME("TRACE")) };
static StaticShaderPropertyId s_propTraceModeShade { ShaderProperty(NAME("MODE"), NAME("SHADE")) };
static StaticShaderPropertyId s_propTraceModeRelight { ShaderProperty(NAME("MODE"), NAME("RELIGHT")) };
static StaticShaderPropertyId s_propTracerBricks { ShaderProperty(NAME("TRACER"), NAME("BRICKS")) };
static StaticShaderPropertyId s_propTracerBVH { ShaderProperty(NAME("TRACER"), NAME("BVH")) };
static StaticShaderPropertyId s_propBlendModeBlend { ShaderProperty(NAME("MODE"), NAME("BLEND")) };
static StaticShaderPropertyId s_propBlendModeCompose { ShaderProperty(NAME("MODE"), NAME("COMPOSE")) };

static constexpr uint32 MaxTransferChanges = 16;
static constexpr uint32 TransferGroupSize = 64;
static constexpr float TransferChangeMargin = 1.0f; // surfels this close (m) to where the scene changed start over

struct GlimmerSurfelTransferConstants
{
    GlimmerSurfelShaderData surfels;
    GlimmerSurfelShaderData previousSurfels;
    Vec4u dispatch; // x = changed boxes
    Vec4f changedMin[MaxTransferChanges];
    Vec4f changedMax[MaxTransferChanges];
};

// GLIMMER_RAY_UINTS, and GLIMMER_SURFEL_RAY_RECORD_UINTS: the ray's sun bounce, then a hit's position, normal, albedo and transmittance after it until it is shaded
static constexpr size_t RaySize = (GlimmerLightParts * 2 + 2) * sizeof(uint32);
static constexpr size_t RayRecordSize = RaySize + (12 + GlimmerSunKeys + 1) * sizeof(uint32); // GLIMMER_SURFEL_RAY_RECORD_UINTS

static constexpr uint32 SurfelsPerTraceGroup = 2;        // GlimmerSurfelTrace's SURFELS_PER_GROUP
static constexpr uint32 RelightGroupSize = GlimmerSurfelRays * SurfelsPerTraceGroup;
static constexpr uint32 ClearGroupSize = 64;
static constexpr uint32 ComposeGroupSize = 64;

static constexpr float CellSizeRadii = 2.0f;     // a hash cell spans this many surfel radii, so the 2x2x2 cells around a point hold every surfel reaching it
static constexpr float RadiusSpacings = 1.25f;   // a surfel's radius in spacings
static constexpr uint32 FeedbackPasses = 1;      // once settled, how many more times the surfels trace their whole history, so bounces through surfels that were still dark catch up
static constexpr float RecenterFraction = 0.5f;  // the viewer moving this much of the radius from where they were placed places them again
static constexpr uint32 MaxSurfelsPerTriangle = 4096;
static constexpr float MaxBudgetScale = 4.0f;

static constexpr float SunRetraceCosine = 0.99985f;   // the sun turning further than about a degree traces the sun bounce again
static constexpr float SunJumpCosine = 0.9659258f;    // further than 15 degrees, the sun bounce held is too far off to keep showing
static constexpr float SunOffArcSine = 0.0174524f;    // a sun further than a degree off the time of day's arc takes no keys
static constexpr uint64 SunTurnRetraceUpdates = 8;    // updates a surfel gets when its sun bounce only needs to follow the sun
static constexpr uint64 SunJumpRetraceUpdates = 32;   // and when it starts over (twice GLIMMER_SURFEL_SUN_HISTORY)

static constexpr uint32 SurfelCounterSurfels = 0; // GLIMMER_SURFEL_COUNTER_SURFELS
static constexpr uint32 SurfelCounterDropped = 1; // GLIMMER_SURFEL_COUNTER_DROPPED
static constexpr uint32 SurfelCounterFoliage = 2; // GLIMMER_SURFEL_COUNTER_FOLIAGE
static constexpr uint32 SurfelCounterFoliageDropped = 3; // GLIMMER_SURFEL_COUNTER_FOLIAGE_DROPPED

static constexpr uint64 LogIntervalMs = 1000;

struct GlimmerSurfelPlaceConstants
{
    GlimmerSurfelShaderData surfels;
    Vec4f center;    // xyz = where the surfels are placed around, w = how far from there
    Vec4f placement; // x = spacing, y = most surfels a triangle gets
    Vec4u dispatch;  // x = span instances, y = groups along x, z = seed
};

struct GlimmerSurfelTraceConstants
{
    GlimmerSurfelShaderData surfels;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;

    /// x = how far rays look before taking the sky
    /// y = luminance emissive surfaces and emitters are clamped to
    /// z = 1 when there are bricks or a BVH to trace near
    /// w = albedo where the ground's isn't known
    Vec4f trace;

    Vec4f params; // x = foliage extinction
    GlimmerSkyShaderData sky;
    GlimmerSHVolumeShaderData sh;
    EnvProbeShaderData skyProbe;

    GlimmerRelightShaderData relight;

    /// xy = absolute texel of the rect to light
    /// zw = its extent
    Vec4i relightRect;

    /// x = ground level
    /// y = groups along x
    Vec4u relightInfo;

    GlimmerBrickShaderData bricks;
    Vec4u bvh; // x = traced instances in the TLAS
    GlimmerLocalLightsShaderData localLights;
    GlimmerEmittersShaderData emitters;
    GlimmerLightShaderData light;
};

struct GlimmerSurfelBlendConstants
{
    GlimmerSurfelShaderData surfels;
    Vec4u dispatch; // x = first ray record of the surfels', y = most updates the history averages
    GlimmerLightShaderData light;
};

#pragma region GlimmerSurfelCache

GlimmerSurfelCache::GlimmerSurfelCache()
    : m_shaderData {},
      m_placedCenter(Vec3f::Zero()),
      m_placedSpacing(0.0f),
      m_placedRadius(0.0f),
      m_placedFoliageVoxelSize(0.0f),
      m_seenInstancesGeneration(~0u),
      m_numPlacements(0),
      m_cursor(0),
      m_frameIndex(0),
      m_numTracedSinceWoken(0),
      m_sunDirection(Vec3f::Zero()),
      m_sunEpoch(0),
      m_sunJumpEpoch(0),
      m_isSunTraced(false),
      m_sunArc(Vec4f::Zero()),
      m_sunArcEpoch(0),
      m_hasSunKeys(false),
      m_seenLightsGeneration(~0u),
      m_seenEmittersHash(0),
      m_seenFoliageExtinction(-1.0f),
      m_seenNearReach(-1.0f),
      m_nearTracer(~0u),
      m_numSurfels(0),
      m_numDropped(0),
      m_numFoliageVoxels(0),
      m_numFoliageDropped(0),
      m_startTime(Time::Now()),
      m_lastLogTime(Time(0)),
      m_composedLight {},
      m_hasComposedLight(false),
      m_wasComposedWithClouds(false)
{
}

GlimmerSurfelCache::~GlimmerSurfelCache()
{
    ReleaseResources();

    EnqueueDeletion(std::move(m_placeholderBuffer));
}

const GpuBufferRef& GlimmerSurfelCache::GetPlaceholderBuffer()
{
    if (!m_placeholderBuffer.IsValid())
    {
        m_placeholderBuffer = GlimmerHelpers::CreateGlimmerRWBuffer(sizeof(Vec4f), alignof(Vec4f));
    }

    return m_placeholderBuffer;
}

void GlimmerSurfelCache::CreateResources()
{
    for (GpuBufferRef* surfelsBuffer : { &m_surfelsBuffer, &m_previousSurfelsBuffer })
    {
        *surfelsBuffer = GlimmerHelpers::CreateGlimmerRWBuffer(sizeof(Vec4u) * GlimmerSurfelCapacity, alignof(Vec4f));
    }

    for (GpuBufferRef* hashBuffer : { &m_hashBuffer, &m_previousHashBuffer })
    {
        *hashBuffer = GlimmerHelpers::CreateGlimmerRWBuffer(sizeof(uint32) * GlimmerSurfelHashUints, alignof(Vec4f));
    }

    for (GpuBufferRef* dataBuffer : { &m_dataBuffer, &m_previousDataBuffer })
    {
        *dataBuffer = GlimmerHelpers::CreateGlimmerRWBuffer(sizeof(uint32) * GlimmerSurfelDataUints * GlimmerSurfelCapacity, alignof(Vec4f));
    }

    for (GpuBufferRef* statesBuffer : { &m_statesBuffer, &m_previousStatesBuffer })
    {
        *statesBuffer = GlimmerHelpers::CreateGlimmerRWBuffer(sizeof(Vec2u) * GlimmerSurfelCapacity, alignof(Vec4f));
    }

    m_foliageBuffer = GlimmerHelpers::CreateGlimmerRWBuffer(sizeof(uint32) * GlimmerFoliageBufferUints, alignof(Vec4f));

#ifdef HYP_RHI_DEBUG_NAMES
    m_surfelsBuffer->SetDebugName(NAME("GlimmerSurfels"));
    m_hashBuffer->SetDebugName(NAME("GlimmerSurfelHash"));
    m_dataBuffer->SetDebugName(NAME("GlimmerSurfelData"));
    m_foliageBuffer->SetDebugName(NAME("GlimmerFoliage"));
#endif

    for (GpuBufferRef& readbackBuffer : m_countersReadbackBuffers)
    {
        readbackBuffer = RI.MakeGpuBuffer(GpuBufferType::ReadbackBuffer, GlimmerSurfelCounters * sizeof(uint32));
        readbackBuffer->SetIsCpuAccessible(true);
        Check(readbackBuffer->Create());
    }
}

void GlimmerSurfelCache::ReleaseResources()
{
    for (GpuBufferRef* buffer : { &m_surfelsBuffer, &m_hashBuffer, &m_dataBuffer, &m_statesBuffer, &m_foliageBuffer,
             &m_previousSurfelsBuffer, &m_previousHashBuffer, &m_previousDataBuffer, &m_previousStatesBuffer })
    {
        EnqueueDeletion(std::move(*buffer));
    }

    m_placeChanges.Clear();
    m_isPlaceFromScratch = true;

    for (uint32 slot = 0; slot < NumFramesInFlight; slot++)
    {
        EnqueueDeletion(std::move(m_countersReadbackBuffers[slot]));

        m_isCountersReadbackPending[slot] = false;
    }

    m_shaderData.info.w = 0;
    m_numPlacements = 0;
    m_seenInstancesGeneration = ~0u;
    m_hasComposedLight = false;
}

void GlimmerSurfelCache::ReadCounters(Frame* frame)
{
    const uint32 slot = frame->GetFrameIndex() % NumFramesInFlight;

    if (!m_isCountersReadbackPending[slot] || !m_countersReadbackBuffers[slot].IsValid())
    {
        return;
    }

    uint32 counters[GlimmerSurfelCounters];

    m_countersReadbackBuffers[slot]->Invalidate();
    m_countersReadbackBuffers[slot]->Read(sizeof(counters), counters);

    m_isCountersReadbackPending[slot] = false;

    m_numSurfels = MathUtil::Min(counters[SurfelCounterSurfels], GlimmerSurfelCapacity);
    m_numDropped = counters[SurfelCounterDropped] + (counters[SurfelCounterSurfels] - m_numSurfels);

    m_numFoliageVoxels = MathUtil::Min(counters[SurfelCounterFoliage], GlimmerFoliageCapacity);
    m_numFoliageDropped = counters[SurfelCounterFoliageDropped];

    s_statGlimmerSurfelCount = m_numSurfels;
    s_statGlimmerSurfelsDropped = m_numDropped;
    s_statGlimmerFoliageVoxelCount = m_numFoliageVoxels;
}

void GlimmerSurfelCache::CopyCounters(Frame* frame)
{
    const uint32 slot = frame->GetFrameIndex() % NumFramesInFlight;
    GpuBufferRef& readbackBuffer = m_countersReadbackBuffers[slot];

    if (!readbackBuffer.IsValid())
    {
        return;
    }

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(m_hashBuffer.Get(), ResourceState::CopySrc, ShaderModuleType::Compute);
    cr << InsertBarrier(readbackBuffer.Get(), ResourceState::CopyDst, ShaderModuleType::Compute);

    cr << CopyBuffer(m_hashBuffer.Get(), readbackBuffer.Get(), uint32(GlimmerSurfelCounters * sizeof(uint32)));

    cr << InsertBarrier(m_hashBuffer.Get(), ResourceState::ShaderResource);

    m_isCountersReadbackPending[slot] = true;
}

bool GlimmerSurfelCache::ShouldPlace(const GlimmerSurfelUpdateInputs& inputs, float spacing, float radius, float foliageVoxelSize)
{
    const bool isLayoutChanged = m_numPlacements == 0
        || spacing != m_placedSpacing
        || foliageVoxelSize != m_placedFoliageVoxelSize;

    if (isLayoutChanged)
    {
        m_isPlaceFromScratch = true;
    }

    bool shouldPlace = isLayoutChanged
        || radius != m_placedRadius
        || inputs.viewerPosition.Distance(m_placedCenter) > radius * RecenterFraction;

    const uint32 generation = inputs.sceneInstances->GetGeneration();

    if (generation == m_seenInstancesGeneration)
    {
        return shouldPlace;
    }

    GlimmerSceneChanges changes;

    if (m_seenInstancesGeneration == ~0u || !inputs.sceneInstances->GetChangesSince(m_seenInstancesGeneration, changes))
    {
        shouldPlace = true;
        m_isPlaceFromScratch = true;
    }
    else
    {
        const BoundingBox placedBounds(m_placedCenter - Vec3f(m_placedRadius), m_placedCenter + Vec3f(m_placedRadius));

        for (const GlimmerSceneChange& change : changes)
        {
            if (!change.isLodOnly && change.bounds.Overlaps(placedBounds))
            {
                shouldPlace = true;

                m_placeChanges.PushBack(change.bounds);
            }
        }
    }

    m_seenInstancesGeneration = generation;

    return shouldPlace;
}

// each new surfel takes the light of the previous one nearest it facing the same way, unless the scene changed around it
void GlimmerSurfelCache::Transfer(Frame* frame, const GlimmerSurfelShaderData& previousShaderData)
{
    CommandRecorder& cr = frame->cr;

    GlimmerSurfelTransferConstants constants {};
    constants.surfels = m_shaderData;
    constants.previousSurfels = previousShaderData;

    uint32 numChanges = 0;

    for (const BoundingBox& change : m_placeChanges)
    {
        const uint32 changeIndex = MathUtil::Min(numChanges, MaxTransferChanges - 1);

        BoundingBox expanded(change.GetMin() - Vec3f(TransferChangeMargin), change.GetMax() + Vec3f(TransferChangeMargin));

        if (numChanges >= MaxTransferChanges)
        {
            expanded = expanded.Union(BoundingBox(constants.changedMin[changeIndex].GetXYZ(), constants.changedMax[changeIndex].GetXYZ()));
        }

        constants.changedMin[changeIndex] = Vec4f(expanded.GetMin(), 0.0f);
        constants.changedMax[changeIndex] = Vec4f(expanded.GetMax(), 0.0f);

        numChanges = MathUtil::Min(numChanges + 1, MaxTransferChanges);
    }

    constants.dispatch = Vec4u(numChanges, 0, 0, 0);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    for (const GpuBufferRef* buffer : { &m_previousSurfelsBuffer, &m_previousHashBuffer, &m_previousDataBuffer, &m_previousStatesBuffer, &m_surfelsBuffer, &m_hashBuffer })
    {
        cr << InsertBarrier(buffer->Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    }

    for (const GpuBufferRef* buffer : { &m_dataBuffer, &m_statesBuffer })
    {
        cr << InsertBarrier(buffer->Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    }

    uint32 uniformIndex = 0;

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSurfelTransfer")));
    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "GlimmerSurfelsBuffer"_sh, m_previousSurfelsBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4u)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerSurfelHashBuffer"_sh, m_previousHashBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerSurfelDataBuffer"_sh, m_previousDataBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerFoliageBuffer"_sh, m_foliageBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "PreviousSurfelStates"_sh, m_previousStatesBuffer.Get(), ShaderDataOffset(0, sizeof(Vec2u)));
    cr << SetShaderUniform(uniformIndex++, "NewSurfels"_sh, m_surfelsBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4u)));
    cr << SetShaderUniform(uniformIndex++, "NewSurfelHash"_sh, m_hashBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "OutSurfelData"_sh, m_dataBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "OutSurfelStates"_sh, m_statesBuffer.Get(), ShaderDataOffset(0, sizeof(Vec2u)));
    cr << DispatchCompute(Vec3u { GlimmerSurfelCapacity / TransferGroupSize, 1, 1 });

    for (const GpuBufferRef* buffer : { &m_dataBuffer, &m_statesBuffer })
    {
        cr << InsertBarrier(buffer->Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    }
}

bool GlimmerSurfelCache::HasChangedAround(const GlimmerSurfelUpdateInputs& inputs)
{
    bool hasChanged = false;

    if (inputs.localLights && inputs.localLights->GetGeneration() != m_seenLightsGeneration)
    {
        m_seenLightsGeneration = inputs.localLights->GetGeneration();
        hasChanged = true;
    }

    const GlimmerEmittersShaderData& emitters = inputs.sceneInstances->GetEmittersShaderData();
    const uint64 emittersHash = FNV1::DoHashWords(&emitters, sizeof(emitters), 0);

    if (emittersHash != m_seenEmittersHash)
    {
        m_seenEmittersHash = emittersHash;
        hasChanged = true;
    }

    const float foliageExtinction = GlimmerHelpers::GetGlimmerFoliageExtinction();

    if (foliageExtinction != m_seenFoliageExtinction)
    {
        m_seenFoliageExtinction = foliageExtinction;
        hasChanged = true;
    }

    const float nearReach = g_cvGlimmerSurfelsNearReach.Get();

    if (nearReach != m_seenNearReach)
    {
        m_seenNearReach = nearReach;
        hasChanged = true;
    }

    const Vec2f placedMin = Vec2f(m_placedCenter.x - m_placedRadius, m_placedCenter.z - m_placedRadius);
    const Vec2f placedMax = Vec2f(m_placedCenter.x + m_placedRadius, m_placedCenter.z + m_placedRadius);

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        const float texelSize = GetGlimmerGroundTexelSize(levelIndex);

        for (const GlimmerTexelRect& rect : { inputs.surfaceCache->GetUploadedRect(levelIndex), inputs.spanCache->GetFilledRect(levelIndex) })
        {
            if (rect.IsEmpty())
            {
                continue;
            }

            const bool overlaps = float(rect.max.x) * texelSize > placedMin.x && float(rect.min.x) * texelSize < placedMax.x
                && float(rect.max.y) * texelSize > placedMin.y && float(rect.min.y) * texelSize < placedMax.y;

            hasChanged |= overlaps;
        }
    }

    return hasChanged;
}

void GlimmerSurfelCache::Place(Frame* frame, const GlimmerSurfelUpdateInputs& inputs, float spacing, float radius, float foliageVoxelSize)
{
    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSurfelPlace);

    CommandRecorder& cr = frame->cr;

    // the new surfels go where the previous set's oldest copy was, so the previous ones can still be read
    const bool canTransfer = !m_isPlaceFromScratch && m_numPlacements != 0;
    const GlimmerSurfelShaderData previousShaderData = m_shaderData;

    if (canTransfer)
    {
        std::swap(m_surfelsBuffer, m_previousSurfelsBuffer);
        std::swap(m_hashBuffer, m_previousHashBuffer);
        std::swap(m_dataBuffer, m_previousDataBuffer);
        std::swap(m_statesBuffer, m_previousStatesBuffer);
    }

    const uint32 numSpanInstances = inputs.sceneInstances->GetNumSpanInstances();
    const Vec3u placeGroups = RenderHelpers::WrapComputeGroupCount(MathUtil::Max(numSpanInstances, 1u));

    GlimmerSurfelPlaceConstants constants {};
    constants.surfels = m_shaderData;
    constants.center = Vec4f(inputs.viewerPosition, radius);
    constants.placement = Vec4f(
        spacing,
        float(MaxSurfelsPerTriangle),
        MathUtil::Max(g_cvGlimmerSurfelsFoliageMinDensity.Get(), 0.0f) * foliageVoxelSize * foliageVoxelSize * foliageVoxelSize,
        0.0f);
    constants.dispatch = Vec4u(numSpanInstances, placeGroups.x, m_numPlacements * 0x9E3779B9u, 0);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    const GpuBufferRef* const placedBuffers[] = {
        &m_surfelsBuffer,
        &m_hashBuffer,
        &m_dataBuffer,
        &m_statesBuffer,
        &m_foliageBuffer
    };

    for (const GpuBufferRef* buffer : placedBuffers)
    {
        cr << InsertBarrier(buffer->Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    }

    const auto bindHashes = [&](const StaticShaderPropertyId& modeProperty)
    {
        ShaderPropertySet shaderProperties;
        shaderProperties.Add(modeProperty);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSurfelPlace"), shaderProperties));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "OutSurfelHash"_sh, m_hashBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "OutFoliage"_sh, m_foliageBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));

        return uniformIndex;
    };

    bindHashes(s_propPlaceModeClear);

    cr << DispatchCompute(Vec3u { GlimmerSurfelHashEntries / ClearGroupSize, 1, 1 });

    cr << InsertBarrier(m_hashBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    cr << InsertBarrier(m_foliageBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    {
        uint32 uniformIndex = bindHashes(s_propPlaceModePlace);

        cr << SetShaderUniform(uniformIndex++, "SpanInstancesBuffer"_sh, inputs.sceneInstances->GetSpanInstancesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerSpanInstanceShaderData)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerTrianglesBuffer"_sh, inputs.triangleCache->GetTrianglesBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "OutSurfels"_sh, m_surfelsBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4u)));
        cr << SetShaderUniform(uniformIndex++, "OutSurfelData"_sh, m_dataBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "OutSurfelStates"_sh, m_statesBuffer.Get(), ShaderDataOffset(0, sizeof(Vec2u)));

        if (numSpanInstances != 0)
        {
            cr << DispatchCompute(placeGroups);
        }
    }

    cr << InsertBarrier(m_hashBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    cr << InsertBarrier(m_foliageBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    bindHashes(s_propPlaceModeAllocate);

    cr << DispatchCompute(Vec3u { GlimmerFoliageHashEntries / ClearGroupSize, 1, 1 });

    for (const GpuBufferRef* buffer : placedBuffers)
    {
        cr << InsertBarrier(buffer->Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    }

    if (canTransfer)
    {
        Transfer(frame, previousShaderData);

        if (m_placeChanges.Any())
        {
            m_wakeEpoch = m_wakeEpoch % 0xFFFFu + 1u;
        }
    }

    m_placeChanges.Clear();
    m_isPlaceFromScratch = false;

    m_placedCenter = inputs.viewerPosition;
    m_placedSpacing = spacing;
    m_placedRadius = radius;
    m_placedFoliageVoxelSize = foliageVoxelSize;
    m_numPlacements++;
    m_cursor = 0;
    m_numTracedSinceWoken = 0;
    m_feedbackPassesLeft = FeedbackPasses;
    m_hasComposedLight = false;

    if (inputs.relight)
    {
        inputs.relight->AddChangedArea(
            Vec2f(m_placedCenter.x - radius, m_placedCenter.z - radius),
            Vec2f(m_placedCenter.x + radius, m_placedCenter.z + radius));
    }

    s_statGlimmerSurfelPlacements++;

    HYP_LOG(Rendering, Info, "Glimmer: placing surfels {} m apart within {} m of ({}, {}, {}) on {} span instances",
        spacing,
        radius,
        inputs.viewerPosition.x,
        inputs.viewerPosition.y,
        inputs.viewerPosition.z,
        numSpanInstances);
}

void GlimmerSurfelCache::Update(Frame* frame, const GlimmerSurfelUpdateInputs& inputs)
{
    HYP_SCOPE;

    if (!g_cvGlimmerSurfelsEnabled.Get())
    {
        if (m_surfelsBuffer.IsValid())
        {
            ReleaseResources();
        }

        return;
    }

    if (!inputs.surfaceCache || !inputs.spanCache || !inputs.triangleCache || !inputs.sceneInstances || !inputs.scratchBuffer
        || !inputs.triangleCache->IsReady() || !inputs.sceneInstances->IsReady() || !inputs.spanCache->GetSpansBuffer().IsValid())
    {
        return;
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSurfels);

    if (!m_surfelsBuffer.IsValid())
    {
        CreateResources();
    }

    ReadCounters(frame);

    if (g_cvGlimmerSurfelsLogStats.Get() && Time::Now().ToMilliseconds() - m_lastLogTime.ToMilliseconds() >= LogIntervalMs)
    {
        m_lastLogTime = Time::Now();

        HYP_LOG(Rendering, Info, "Glimmer: {} surfels ({} dropped), {} foliage voxels ({} dropped), {} placements, tracing {} a frame",
            m_numSurfels,
            m_numDropped,
            m_numFoliageVoxels,
            m_numFoliageDropped,
            m_numPlacements,
            m_shaderData.info.x);
    }

    const bool useBVH = g_cvGlimmerTracer.Get() != 0;

    const bool hasBVH = useBVH
        && inputs.sceneInstances->HasTLAS()
        && inputs.triangleCache->HasBLAS();

    const bool hasBricks = !useBVH
        && inputs.brickWorld && inputs.brickWorld->IsReady();

    const uint32 nearTracer = hasBVH ? 2u : (hasBricks ? 1u : 0u);

    bool shouldWake = HasChangedAround(inputs);

    if (nearTracer != m_nearTracer)
    {
        HYP_LOG(Rendering, Info, "Glimmer: surfels trace {} from {} ms",
            nearTracer == 2u ? "the BVH" : (nearTracer == 1u ? "the bricks" : "nothing near"),
            Time::Now().ToMilliseconds() - m_startTime.ToMilliseconds());

        m_nearTracer = nearTracer;
        shouldWake = true;
    }

    const float spacing = MathUtil::Max(g_cvGlimmerSurfelsSpacing.Get(), 0.05f);
    const float radius = MathUtil::Max(g_cvGlimmerSurfelsRadius.Get(), 1.0f);
    const float surfelRadius = spacing * RadiusSpacings;
    const float cellSize = surfelRadius * CellSizeRadii;
    const float foliageVoxelSize = MathUtil::Max(g_cvGlimmerSurfelsFoliageVoxelSize.Get(), 0.0f);

    m_frameIndex++;

    m_shaderData.params = Vec4f(
        cellSize,
        1.0f / cellSize,
        surfelRadius,
        MathUtil::Max(g_cvGlimmerSurfelsNearReach.Get(), 0.0f));

    const bool isSunTraced = g_cvGlimmerSurfelsTracedSun.Get();

    m_shaderData.foliage = Vec4f(
        foliageVoxelSize,
        foliageVoxelSize > 0.0f ? 1.0f / foliageVoxelSize : 0.0f,
        0.0f,
        isSunTraced ? 1.0f : 0.0f);

    const Vec3f sunDirection = GlimmerHelpers::GetGlimmerSunDirection();
    const float sunCosine = sunDirection.Dot(m_sunDirection);

    // a sun bounce left over from before tracing it was switched off is out of date
    const bool hasStartedTracingSun = isSunTraced && !m_isSunTraced;

    if (isSunTraced != m_isSunTraced)
    {
        m_hasComposedLight = false;
        m_isSunTraced = isSunTraced;
    }

    const bool hasSunTurned = m_sunEpoch == 0 || sunCosine < SunRetraceCosine || hasStartedTracingSun;
    const bool hasSunJumped = m_sunEpoch == 0 || sunCosine < SunJumpCosine || hasStartedTracingSun;

    if (hasSunTurned)
    {
        m_sunDirection = sunDirection;
        m_sunEpoch = m_sunEpoch % 0xFFFFu + 1u;
    }

    if (hasSunJumped)
    {
        m_sunJumpEpoch = m_sunEpoch;
    }

    m_shaderData.sun = Vec4f(m_sunDirection, float(m_sunEpoch));
    m_shaderData.foliage.z = float(m_sunJumpEpoch);

    // with a time of day the sun bounce is baked at keys along the sun's arc, so the sun moving along it needs no rays
    const WorldShaderData* worldData = GetWorldBufferData();
    const bool hasSunKeys = worldData != nullptr && (worldData->environmentFlags & uint32(WorldEnvironmentFlags::SunArc)) != 0;

    bool isSunOnKeys = false;

    if (hasSunKeys)
    {
        const Vec4f& sunArc = worldData->sunArc;

        if (!m_hasSunKeys || sunArc != m_sunArc)
        {
            m_sunArc = sunArc;
            m_sunArcEpoch = m_sunArcEpoch % 0xFFFFu + 1u;
            shouldWake = true;
        }

        const float keyHours = (sunArc.y - sunArc.x) / float(GlimmerSunKeys - 1);

        for (uint32 key = 0; key < GlimmerSunKeys; key++)
        {
            m_shaderData.sunKeys[key] = Vec4f(GetSunArcDirection(sunArc, sunArc.x + keyHours * float(key)), 0.0f);
        }

        float offArc = 0.0f;
        const float keyAt = (GetSunArcHour(sunArc, sunDirection, offArc) - sunArc.x) / keyHours;

        isSunOnKeys = offArc < SunOffArcSine && keyAt >= -1e-3f && keyAt <= float(GlimmerSunKeys - 1) + 1e-3f;

        m_shaderData.sunArc = Vec4f(isSunOnKeys ? MathUtil::Clamp(keyAt, 0.0f, float(GlimmerSunKeys - 1)) : -1.0f, 1.0f, float(m_sunArcEpoch), 0.0f);
    }
    else
    {
        m_shaderData.sunArc = Vec4f(-1.0f, 0.0f, float(m_sunArcEpoch), 0.0f);
    }

    if (hasSunKeys != m_hasSunKeys)
    {
        m_hasComposedLight = false;
        m_hasSunKeys = hasSunKeys;
    }

    if (ShouldPlace(inputs, spacing, radius, foliageVoxelSize))
    {
        Place(frame, inputs, spacing, radius, foliageVoxelSize);
    }
    else if (shouldWake)
    {
        m_numTracedSinceWoken = 0;
        m_wakeEpoch = m_wakeEpoch % 0xFFFFu + 1u;
        m_feedbackPassesLeft = FeedbackPasses;

        s_statGlimmerSurfelWakes++;
    }

    // counted per frame, so a low frame rate gets more a frame to keep the same pace
    const float budgetScale = MathUtil::Clamp(inputs.budgetScale, 1.0f, MaxBudgetScale);

    uint32 surfelsPerFrame = MathUtil::Clamp(
        uint32(float(uint32(MathUtil::Max(g_cvGlimmerSurfelsRaysPerFrame.Get(), 0)) / GlimmerSurfelRays) * budgetScale),
        1u,
        GlimmerMaxSurfelsPerFrame);

    const uint64 historyUpdates = uint64(MathUtil::Max(g_cvGlimmerSurfelsHistory.Get(), 1));

    const uint64 numTraced = uint64(m_numSurfels) + uint64(m_numFoliageVoxels);

    if (hasSunTurned && isSunTraced && !hasSunKeys)
    {
        // only the sun bounce is out of date, which needs a few updates and not the whole history
        uint64 sunRetraceUpdates = SunTurnRetraceUpdates;

        if (hasSunJumped)
        {
            sunRetraceUpdates = SunJumpRetraceUpdates;
        }

        m_numTracedSinceWoken = MathUtil::Min(m_numTracedSinceWoken, numTraced * (historyUpdates - MathUtil::Min(historyUpdates, sunRetraceUpdates)));
    }

    if (numTraced != 0 && m_numTracedSinceWoken >= numTraced * historyUpdates)
    {
        if (m_feedbackPassesLeft > 0)
        {
            m_feedbackPassesLeft--;
            m_numTracedSinceWoken = 0;
            m_wakeEpoch = m_wakeEpoch % 0xFFFFu + 1u;
        }
        else
        {
            surfelsPerFrame = 0;
        }
    }

    m_shaderData.info = Vec4u(surfelsPerFrame, m_cursor, m_frameIndex, 1u);

    s_statGlimmerSurfelsTraced = surfelsPerFrame;

    Check(inputs.scratchBuffer->EnsureCapacity(
        RayRecordSize * size_t(MathUtil::Max(surfelsPerFrame, 1u)) * GlimmerSurfelRays,
        alignof(Vec4f)));

    Trace(frame, inputs, hasBVH, hasBricks);
    Blend(frame, inputs.scratchBuffer, inputs.light, inputs.cloudPass);

    m_cursor += surfelsPerFrame;
    m_numTracedSinceWoken += surfelsPerFrame;
}

void GlimmerSurfelCache::Trace(Frame* frame, const GlimmerSurfelUpdateInputs& inputs, bool hasBVH, bool hasBricks)
{
    CommandRecorder& cr = frame->cr;

    const bool useBVH = g_cvGlimmerTracer.Get() != 0;
    const uint32 surfelsPerFrame = m_shaderData.info.x;

    GpuBuffer* const raysBuffer = inputs.scratchBuffer;

    GlimmerSkyShaderData skyData;
    EnvProbeShaderData skyProbeData;
    GlimmerHelpers::GetGlimmerSkyShaderData(inputs.skyProbe, skyData, skyProbeData);

    // the far field is sampled once it has been traced; until then its constants stay zeroed and the trace treats it as covering nothing
    const bool hasSHVolume = inputs.shVolume && inputs.shVolume->IsReady();

    const GpuImageViewRef& groundImageView = inputs.surfaceCache->GetGroundImageView();

    GlimmerSurfelTraceConstants constants {};
    constants.surfels = m_shaderData;
    constants.ground = inputs.surfaceCache->GetGroundShaderData();
    constants.spans = inputs.spanCache->GetShaderData();
    constants.trace = Vec4f(GlimmerMaxRayDistance, GlimmerSkyMaxLuminance, (hasBVH || hasBricks) ? 1.0f : 0.0f, GlimmerDefaultGroundAlbedo);
    constants.params = Vec4f(GlimmerHelpers::GetGlimmerFoliageExtinction(), 0.0f, 0.0f, 0.0f);
    constants.sky = skyData;
    constants.skyProbe = skyProbeData;
    constants.light = inputs.light;

    if (hasSHVolume)
    {
        constants.sh = inputs.shVolume->GetShaderData();
    }

    if (hasBricks)
    {
        constants.bricks = inputs.brickWorld->GetShaderData();
    }

    if (hasBVH)
    {
        constants.bvh = Vec4u(inputs.sceneInstances->GetNumTracedInstances(), 0u, 0u, 0u);
        constants.emitters = inputs.sceneInstances->GetEmittersShaderData();
    }

    if (inputs.localLights)
    {
        constants.localLights = inputs.localLights->GetShaderData();
    }

    const GpuImageViewRef relightImageView = inputs.relight ? inputs.relight->GetImageView() : GpuImageViewRef();

    const auto bindTraceResources = [&](const StaticShaderPropertyId& modeProperty, const GlimmerSurfelTraceConstants& dispatchConstants)
    {
        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        RI.cbufferAllocator->Write(&dispatchConstants);

        if (inputs.cloudPass != nullptr)
        {
            inputs.cloudPass->WriteShaderData(*RI.cbufferAllocator);
        }
        else
        {
            static const EffectVolumeShaderData s_noCloudVolume {};
            static const CloudWeatherMapShaderData s_noWeatherMap {};
            static const CloudShadowMapShaderData s_noShadowMap {};

            RI.cbufferAllocator->Write(&s_noCloudVolume);
            RI.cbufferAllocator->Write(&s_noWeatherMap);
            RI.cbufferAllocator->Write(&s_noShadowMap);
        }

        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        ShaderPropertySet shaderProperties;
        shaderProperties.Add(modeProperty);
        shaderProperties.Add(useBVH ? s_propTracerBVH : s_propTracerBricks);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSurfelTrace"), shaderProperties));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
        cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());

        if (useBVH && hasBVH)
        {
            cr << SetShaderUniform(uniformIndex++, "GlimmerTLASNodesBuffer"_sh, inputs.sceneInstances->GetTLASNodesBuffer().Get(), ShaderDataOffset(0, sizeof(SAHBVHNode)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerTracedInstancesBuffer"_sh, inputs.sceneInstances->GetTracedInstancesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerTracedInstanceShaderData)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerBLASNodesBuffer"_sh, inputs.triangleCache->GetNodesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerBLASNode)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerTrianglesBuffer"_sh, inputs.triangleCache->GetTrianglesBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        }
        else if (useBVH)
        {
            const GpuBufferRef& placeholder = GetPlaceholderBuffer();

            for (const StringHash name : { "GlimmerTLASNodesBuffer"_sh, "GlimmerTracedInstancesBuffer"_sh, "GlimmerBLASNodesBuffer"_sh, "GlimmerTrianglesBuffer"_sh })
            {
                cr << SetShaderUniform(uniformIndex++, name, placeholder.Get(), ShaderDataOffset(0, sizeof(uint32)));
            }
        }
        else if (hasBricks)
        {
            cr << SetShaderUniform(uniformIndex++, "GlimmerBrickTableBuffer"_sh, inputs.brickWorld->GetTableBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerBrickIndexBuffer"_sh, inputs.brickWorld->GetIndexBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingsBuffer"_sh, inputs.brickWorld->GetCrossingsBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerBrickMaterialBuffer"_sh, inputs.brickWorld->GetMaterialBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        }
        else
        {
            const GpuBufferRef& placeholder = GetPlaceholderBuffer();

            for (const StringHash name : { "GlimmerBrickTableBuffer"_sh, "GlimmerBrickIndexBuffer"_sh, "GlimmerBrickCrossingsBuffer"_sh, "GlimmerBrickMaterialBuffer"_sh })
            {
                cr << SetShaderUniform(uniformIndex++, name, placeholder.Get(), ShaderDataOffset(0, sizeof(uint32)));
            }
        }

        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, groundImageView);
        cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, inputs.surfaceCache->GetGroundAlbedoImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, inputs.spanCache->GetSpansBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, inputs.spanCache->GetHeightBoundsBuffer().Get(), ShaderDataOffset(0, sizeof(float)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerSurfelsBuffer"_sh, m_surfelsBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4u)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerSurfelHashBuffer"_sh, m_hashBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerSurfelDataBuffer"_sh, m_dataBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerFoliageBuffer"_sh, m_foliageBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
        cr << SetShaderUniform(uniformIndex++, "CloudWeatherMapTexture"_sh, inputs.cloudPass != nullptr ? inputs.cloudPass->GetWeatherMapView() : RI.placeholderData->GetImageView2D1x1R8Array());
        cr << SetShaderUniform(uniformIndex++, "CloudShadowMapTexture"_sh, inputs.cloudPass != nullptr ? inputs.cloudPass->GetShadowMapView() : RI.placeholderData->GetImageView2D1x1R8());

        // placeholders where the far field isn't ready: its zeroed constants keep the shader from sampling them
        const GpuImageViewRef& placeholderView = RI.placeholderData->GetImageView3D1x1x1R8();

        cr << SetShaderUniform(uniformIndex++, "GlimmerSHDataTexture"_sh, hasSHVolume ? inputs.shVolume->GetDataImageView() : placeholderView);
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHStateTexture"_sh, hasSHVolume ? inputs.shVolume->GetStateImageView() : placeholderView);
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHRadianceTexture"_sh, hasSHVolume ? inputs.shVolume->GetRadianceImageView() : placeholderView);
        cr << SetShaderUniform(uniformIndex++, "GlimmerSHLightBuffer"_sh, (hasSHVolume ? inputs.shVolume->GetLightBuffer() : GetPlaceholderBuffer()).Get(), ShaderDataOffset(0, sizeof(uint32)));

        cr << SetShaderUniform(uniformIndex++, "OutRays"_sh, raysBuffer, ShaderDataOffset(0, sizeof(uint32)));

        if (relightImageView.IsValid())
        {
            cr << SetShaderUniform(uniformIndex++, &modeProperty == &s_propTraceModeRelight ? "OutRelight"_sh : "GlimmerRelightTexture"_sh, relightImageView);
        }
    };

    if (inputs.relight && inputs.relight->GetDispatches().Any())
    {
        ENGINE_STAT_GPU_SCOPE(&s_statGlimmerRelight);

        cr << InsertBarrier(inputs.relight->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        for (const GlimmerRelightDispatch& relightDispatch : inputs.relight->GetDispatches())
        {
            const Vec2u extent = Vec2u(uint32(relightDispatch.rect.max.x - relightDispatch.rect.min.x), uint32(relightDispatch.rect.max.y - relightDispatch.rect.min.y));
            const Vec3u groups = RenderHelpers::WrapComputeGroupCount((extent.x * extent.y + RelightGroupSize - 1) / RelightGroupSize);

            GlimmerSurfelTraceConstants relightConstants = constants;
            relightConstants.relightRect = Vec4i(relightDispatch.rect.min.x, relightDispatch.rect.min.y, int32(extent.x), int32(extent.y));
            relightConstants.relightInfo = Vec4u(relightDispatch.level, groups.x, 0, 0);

            bindTraceResources(s_propTraceModeRelight, relightConstants);

            cr << DispatchCompute(groups);

            s_statGlimmerRelightTexels += extent.x * extent.y;
        }

        cr << InsertBarrier(inputs.relight->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    }

    if (inputs.relight)
    {
        inputs.relight->OnDispatched();

        constants.relight = inputs.relight->GetShaderData();
    }

    if (surfelsPerFrame == 0)
    {
        return;
    }

    cr << InsertBarrier(raysBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    for (const StaticShaderPropertyId* modeProperty : { &s_propTraceModeTrace, &s_propTraceModeShade })
    {
        ENGINE_STAT_GPU_SCOPE(modeProperty == &s_propTraceModeTrace ? &s_statGlimmerSurfelTrace : &s_statGlimmerSurfelShade);

        bindTraceResources(*modeProperty, constants);

        cr << DispatchCompute(Vec3u { (surfelsPerFrame + SurfelsPerTraceGroup - 1) / SurfelsPerTraceGroup, 1, 1 });

        // the shade pass reads the trace pass's hits and adds to its rays
        cr << InsertBarrier(raysBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    }
}

void GlimmerSurfelCache::Blend(Frame* frame, GpuBuffer* raysBuffer, const GlimmerLightShaderData& light, const CloudPass* cloudPass)
{
    HYP_SCOPE;

    CommandRecorder& cr = frame->cr;

    // moving clouds change the sun bounce every frame, and once more when they go
    const bool hasClouds = cloudPass != nullptr && cloudPass->IsActive();

    const bool isComposing = !m_hasComposedLight
        || memcmp(&m_composedLight, &light, sizeof(GlimmerLightShaderData)) != 0
        || hasClouds
        || m_wasComposedWithClouds;

    if (m_shaderData.info.x == 0 && !isComposing)
    {
        return;
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSurfelBlend);

    GlimmerSurfelBlendConstants constants {};
    constants.surfels = m_shaderData;
    constants.dispatch = Vec4u(0, uint32(MathUtil::Max(g_cvGlimmerSurfelsHistory.Get(), 1)), m_wakeEpoch, 0);
    constants.light = light;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);

    if (hasClouds)
    {
        cloudPass->WriteShaderData(*RI.cbufferAllocator);
    }
    else
    {
        static const EffectVolumeShaderData s_noCloudVolume {};
        static const CloudWeatherMapShaderData s_noWeatherMap {};
        static const CloudShadowMapShaderData s_noShadowMap {};

        RI.cbufferAllocator->Write(&s_noCloudVolume);
        RI.cbufferAllocator->Write(&s_noWeatherMap);
        RI.cbufferAllocator->Write(&s_noShadowMap);
    }

    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    cr << InsertBarrier(raysBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);

    for (const GpuBufferRef* buffer : { &m_dataBuffer, &m_statesBuffer, &m_foliageBuffer })
    {
        cr << InsertBarrier(buffer->Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    }

    const auto bindBlendResources = [&](const StaticShaderPropertyId& modeProperty)
    {
        ShaderPropertySet shaderProperties;
        shaderProperties.Add(modeProperty);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSurfelBlend"), shaderProperties));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "GlimmerSurfelHashBuffer"_sh, m_hashBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "OutSurfelData"_sh, m_dataBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "OutSurfelStates"_sh, m_statesBuffer.Get(), ShaderDataOffset(0, sizeof(Vec2u)));
        cr << SetShaderUniform(uniformIndex++, "OutFoliage"_sh, m_foliageBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerSurfelsBuffer"_sh, m_surfelsBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4u)));
        cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
        cr << SetShaderUniform(uniformIndex++, "CloudWeatherMapTexture"_sh, hasClouds ? cloudPass->GetWeatherMapView() : RI.placeholderData->GetImageView2D1x1R8Array());
        cr << SetShaderUniform(uniformIndex++, "CloudShadowMapTexture"_sh, hasClouds ? cloudPass->GetShadowMapView() : RI.placeholderData->GetImageView2D1x1R8());

        if (&modeProperty == &s_propBlendModeBlend)
        {
            cr << SetShaderUniform(uniformIndex++, "Rays"_sh, raysBuffer, ShaderDataOffset(0, sizeof(uint32)));
        }
    };

    if (m_shaderData.info.x != 0)
    {
        bindBlendResources(s_propBlendModeBlend);

        // group per surfel
        cr << DispatchCompute(Vec3u { m_shaderData.info.x, 1, 1 });
    }

    if (isComposing)
    {
        ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSurfelCompose);

        for (const GpuBufferRef* buffer : { &m_dataBuffer, &m_statesBuffer, &m_foliageBuffer })
        {
            cr << InsertBarrier(buffer->Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        }

        bindBlendResources(s_propBlendModeCompose);

        // a thread per surfel, then per foliage voxel
        cr << DispatchCompute(Vec3u { (GlimmerSurfelCapacity + GlimmerFoliageCapacity) / ComposeGroupSize, 1, 1 });

        m_composedLight = light;
        m_hasComposedLight = true;
        m_wasComposedWithClouds = hasClouds;
    }

    for (const GpuBufferRef* buffer : { &m_dataBuffer, &m_statesBuffer, &m_foliageBuffer })
    {
        cr << InsertBarrier(buffer->Get(), ResourceState::ShaderResource);
    }

    CopyCounters(frame);
}

#pragma endregion GlimmerSurfelCache

} // namespace Hyperion
