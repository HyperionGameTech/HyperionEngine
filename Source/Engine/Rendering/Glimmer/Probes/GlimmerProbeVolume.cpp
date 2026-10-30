/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/Probes/GlimmerProbeVolume.hpp>
#include <Rendering/Glimmer/GlimmerRelight.hpp>
#include <Rendering/Glimmer/Bricks/GlimmerBrickWorld.hpp>
#include <Rendering/Glimmer/Bricks/GlimmerBrickCVars.hpp>
#include <Rendering/Glimmer/GlimmerTriangleCache.hpp>
#include <Rendering/Glimmer/GlimmerSceneInstances.hpp>
#include <Rendering/Glimmer/GlimmerLocalLights.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHVolume.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHOccupancy.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/Probes/GlimmerProbeCVars.hpp>

#include <Rendering/Clouds/CloudPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/RenderProxy.hpp>
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

#include <Framework/EngineStats.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statGlimmerProbes("Rendering/GPU/Glimmer/Probes");
static EngineStatGpuTimer s_statGlimmerProbeClassify("Rendering/GPU/Glimmer/ProbeClassify");
static EngineStatGpuTimer s_statGlimmerProbeAlloc("Rendering/GPU/Glimmer/ProbeAlloc");
static EngineStatGpuTimer s_statGlimmerProbeList("Rendering/GPU/Glimmer/ProbeList");
static EngineStatGpuTimer s_statGlimmerProbeTrace("Rendering/GPU/Glimmer/ProbeTrace");
static EngineStatGpuTimer s_statGlimmerProbeShade("Rendering/GPU/Glimmer/ProbeShade");
static EngineStatGpuTimer s_statGlimmerProbeBlend("Rendering/GPU/Glimmer/ProbeBlend");
static EngineStatGpuTimer s_statGlimmerProbeCompose("Rendering/GPU/Glimmer/ProbeCompose");
static EngineStatGpuTimer s_statGlimmerRelight("Rendering/GPU/Glimmer/Relight");

static EngineStatCounter<uint32> s_statGlimmerRelightTexels("Rendering/Glimmer/RelightTexels");
static EngineStatCounter<uint32> s_statGlimmerProbeWakes("Rendering/Glimmer/ProbeWakes");
static EngineStatCounter<uint32> s_statGlimmerProbeComposes("Rendering/Glimmer/ProbeComposes");
static EngineStatCounter<uint32> s_statGlimmerProbeChangedBoxes("Rendering/Glimmer/ProbeChangedBoxes");

static StaticShaderPropertyId s_propAllocModeClassify { ShaderProperty(NAME("MODE"), NAME("CLASSIFY")) };
static StaticShaderPropertyId s_propAllocModeAlloc { ShaderProperty(NAME("MODE"), NAME("ALLOC")) };
static StaticShaderPropertyId s_propAllocModeList { ShaderProperty(NAME("MODE"), NAME("LIST")) };
static StaticShaderPropertyId s_propTraceModeTrace { ShaderProperty(NAME("MODE"), NAME("TRACE")) };
static StaticShaderPropertyId s_propTraceModeShade { ShaderProperty(NAME("MODE"), NAME("SHADE")) };
static StaticShaderPropertyId s_propTraceModeRelight { ShaderProperty(NAME("MODE"), NAME("RELIGHT")) };
static StaticShaderPropertyId s_propTracerBricks { ShaderProperty(NAME("TRACER"), NAME("BRICKS")) };
static StaticShaderPropertyId s_propTracerBVH { ShaderProperty(NAME("TRACER"), NAME("BVH")) };

static StaticShaderPropertyId s_propBlendModeBlend { ShaderProperty(NAME("MODE"), NAME("BLEND")) };
static StaticShaderPropertyId s_propBlendModeCompose { ShaderProperty(NAME("MODE"), NAME("COMPOSE")) };

// GLIMMER_RAY_UINTS, and GLIMMER_PROBE_RAY_RECORD_UINTS: a hit's position, normal, albedo and transmittance after it until it is shaded
static constexpr size_t RaySize = (GlimmerLightParts * 2 + 2) * sizeof(uint32);
static constexpr size_t RayRecordSize = RaySize + 10 * sizeof(uint32);

static constexpr uint32 ProbeLightEntries = GlimmerLightEntries;

static constexpr uint32 ProbesPerTraceGroup = 2;
static constexpr uint32 RelightGroupSize = 64; // GlimmerProbeTrace's RAYS_PER_PROBE * PROBES_PER_GROUP
static_assert(GlimmerProbeRays == 32);

static constexpr uint32 ListGroupSize = 64;

static constexpr uint32 MinProbesPerFrameCapacity = 256;
static constexpr uint32 MaxBudgetScale = 4;
static constexpr uint32 ComposeGroupSize = 64;

static constexpr uint32 NumProbeCounters = 16; // NOTE must match GLIMMER_PROBE_COUNTERS

static constexpr uint32 MaxInstanceBoxes = 16;
static constexpr uint32 MaxLightBoxes = 4;
static constexpr uint32 MaxChangedBoxes = MaxInstanceBoxes + MaxLightBoxes + 2 * GlimmerGroundLevels; // GLIMMER_PROBE_MAX_CHANGED_BOXES

static constexpr uint32 MaxRelocations = 2;

static constexpr float NearFieldReachSpacings = 8.0f;

static constexpr float BlockMarginSpacings = 1.0f;  // how far around a block solids make it wanted
static constexpr float MinHeightAboveGround = 0.5f; // solids lower than this on the ground (roads, rocks) want no probes
static constexpr uint32 BlockReleaseFrames = 60;    // frames an unwanted block keeps its probes
static constexpr uint32 InsideRetryInterval = 64;   // updates between checks on probes stuck inside solids
static constexpr uint32 ClassifyPeriod = 8;         // frames between looks at the solids around a block
static constexpr float RelocateMargin = 0.3f;       // how far past a back face a probe inside a solid moves (m)

static constexpr float HistorySeconds = 1.0f;
static constexpr float HistorySecondsChanging = 0.25f;
static constexpr uint32 MinHistory = 16;
static constexpr uint32 MinHistoryChanging = 4;

static constexpr uint32 SleepMaxInterval = 3;
static constexpr uint32 SafetyRefreshSeconds = 0;

struct GlimmerProbeAllocConstants
{
    GlimmerProbeVolumeShaderData volume;
    GlimmerGroundShaderData ground;
    GlimmerSHOccupancyShaderData occupancy;

    /// x = probes traced per frame
    /// y = frames an unwanted block keeps its slot
    /// z = updates between retries of probes inside solids
    /// w = pool slots to use
    Vec4u budget;

    /// x = block margin in spacings
    /// y = how far above the ground a solid has to be to want probes around it
    Vec4f params;

    Vec4f viewer; // viewer position in x,y,z

    /// x = 1 when every probe is to wake
    /// y = changed boxes
    /// z = longest sleep interval; a probe that reaches it is settled and only traced again when something wakes it
    /// w = seconds after which a settled probe is traced anyway (0 = never)
    Vec4u wake;

    Vec4f changedMin[MaxChangedBoxes];
    Vec4f changedMax[MaxChangedBoxes];

    Vec4i brickChunkOrigin;          // xyz = absolute chunk of the bricks' window, w = 0 without bricks, 1 with, 2 when none are built yet
    Vec4u brickChunksBuilding[16];   // a bit per chunk of the window, by its place in it, set while it is being built
};

struct GlimmerProbeTraceConstants
{
    GlimmerProbeVolumeShaderData volume;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
    Vec4u dispatch; // x = probes traced per frame at most
    Vec4f params;   // x = foliage extinction
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
};

struct GlimmerProbeBlendConstants
{
    GlimmerProbeVolumeShaderData volume;

    /// x = probes traced per frame at most
    /// y = moves a probe gets to get out of a solid
    /// z = updates the history averages at least while its light holds
    /// w = same but while it changes
    Vec4u dispatch;

    /// x = seconds the history spans while the light holds
    /// y = same but while it changes
    /// z = how far past a back face a probe moves (m)
    Vec4f params;

    GlimmerLightShaderData light;

    GlimmerSHVolumeShaderData sh; // where a probe starting over begins from; zeroed while it can't be sampled
};

static RWStructuredBuffer CreateProbeBuffer(size_t elementSize, size_t numElements)
{
    return GlimmerHelpers::CreateGlimmerRWBuffer(elementSize, numElements);
}


#pragma region GlimmerProbeVolume

GlimmerProbeVolume::GlimmerProbeVolume()
    : m_startTime(Time::Now()),
      m_frameIndex(0),
      m_seenInstancesGeneration(~0u),
      m_poolBlocks(GlimmerProbePoolBlocks),
      m_maxProbesPerFrame(GlimmerMaxProbesPerFrame),
      m_shaderData {},
      m_composedLight {},
      m_hasComposedLight(false)
{
}

GlimmerProbeVolume::~GlimmerProbeVolume()
{
    ReleaseResources();

    for (RWStructuredBuffer& placeholderBuffer : m_placeholderBuffers)
    {
        placeholderBuffer.Shutdown();
    }
}

void GlimmerProbeVolume::ReleaseResources()
{
    for (RWStructuredBuffer* buffer : { &m_blockTableBuffer, &m_cellsBuffer, &m_slotsBuffer, &m_slotTimesBuffer, &m_statesBuffer, &m_shBuffer, &m_lightBuffer, &m_visibilityBuffer,
             &m_trendBuffer, &m_countersBuffer })
    {
        buffer->Shutdown();
    }

    m_seenInstancesGeneration = ~0u;
    m_shaderData.info.w = 0;
    m_hasComposedLight = false;
}

const RWStructuredBuffer& GlimmerProbeVolume::GetPlaceholderBuffer(size_t elementSize) const
{
    // one per stride the shaders declare, 16 bytes either way
    Assert(elementSize == sizeof(uint32) || elementSize == sizeof(Vec4f));

    RWStructuredBuffer& placeholderBuffer = m_placeholderBuffers[elementSize / sizeof(Vec4f)];

    if (!placeholderBuffer.IsInitialized())
    {
        placeholderBuffer = CreateProbeBuffer(elementSize, sizeof(Vec4f) / elementSize);
    }

    return placeholderBuffer;
}

const RWStructuredBuffer& GlimmerProbeVolume::GetBufferOrPlaceholder(const RWStructuredBuffer& buffer, size_t elementSize) const
{
    if (buffer.IsInitialized())
    {
        return buffer;
    }

    return GetPlaceholderBuffer(elementSize);
}

const RWStructuredBuffer& GlimmerProbeVolume::GetBlockTableBuffer() const
{
    return GetBufferOrPlaceholder(m_blockTableBuffer, sizeof(uint32));
}

const RWStructuredBuffer& GlimmerProbeVolume::GetSHBuffer() const
{
    return GetBufferOrPlaceholder(m_shBuffer, sizeof(Vec4f));
}

const RWStructuredBuffer& GlimmerProbeVolume::GetLightBuffer() const
{
    return GetBufferOrPlaceholder(m_lightBuffer, sizeof(Vec4f));
}

const RWStructuredBuffer& GlimmerProbeVolume::GetStatesBuffer() const
{
    return GetBufferOrPlaceholder(m_statesBuffer, sizeof(Vec4u));
}

const RWStructuredBuffer& GlimmerProbeVolume::GetVisibilityBuffer() const
{
    return GetBufferOrPlaceholder(m_visibilityBuffer, sizeof(uint32));
}

const RWStructuredBuffer& GlimmerProbeVolume::GetSlotsBuffer() const
{
    return GetBufferOrPlaceholder(m_slotsBuffer, sizeof(Vec4i));
}

void GlimmerProbeVolume::CreateResources(Frame* frame)
{
    m_poolBlocks = uint32(MathUtil::Clamp(g_cvGlimmerProbesPoolBlocks.Get(), 1, int(GlimmerProbePoolBlocks)));
    // room for the budget scaled up at low frame rates (GlimmerChannelState::budgetScale)
    m_maxProbesPerFrame = MathUtil::Clamp(uint32(MathUtil::Max(g_cvGlimmerProbesRaysPerFrame.Get(), 0)) / GlimmerProbeRays * MaxBudgetScale, MinProbesPerFrameCapacity, GlimmerMaxProbesPerFrame);

    const size_t poolProbes = size_t(m_poolBlocks) * GlimmerProbesPerBlock;

    m_blockTableBuffer = CreateProbeBuffer(sizeof(uint32), GlimmerProbeLevels * GlimmerProbeWindowBlocks);
    m_cellsBuffer = CreateProbeBuffer(sizeof(Vec4i), GlimmerProbeLevels * GlimmerProbeWindowBlocks);
    m_slotsBuffer = CreateProbeBuffer(sizeof(Vec4i), GlimmerProbePoolBlocks);
    m_slotTimesBuffer = CreateProbeBuffer(sizeof(Vec2u), GlimmerProbePoolBlocks);
    m_statesBuffer = CreateProbeBuffer(sizeof(Vec4u), poolProbes);
    m_shBuffer = CreateProbeBuffer(sizeof(Vec4f), poolProbes * 3);
    m_lightBuffer = CreateProbeBuffer(sizeof(Vec4f), poolProbes * ProbeLightEntries);
    m_visibilityBuffer = CreateProbeBuffer(sizeof(uint32), poolProbes * GlimmerProbeVisibilityTexels);
    m_trendBuffer = CreateProbeBuffer(sizeof(Vec4f), poolProbes);
    m_countersBuffer = CreateProbeBuffer(sizeof(uint32), NumProbeCounters + m_maxProbesPerFrame);

    // every slot free, no block with a slot or classified, every counter zero
    Array<uint32, GlimmerAllocator> blockTable;
    blockTable.Resize(GlimmerProbeLevels * GlimmerProbeWindowBlocks);

    for (uint32& entry : blockTable)
    {
        entry = ~0u;
    }

    Array<Vec4i, GlimmerAllocator> slots;
    slots.Resize(GlimmerProbePoolBlocks);

    for (Vec4i& slot : slots)
    {
        slot = Vec4i(0, 0, 0, -1);
    }

    Array<Vec4u, GlimmerAllocator> states;
    states.Resize(poolProbes);

    for (Vec4u& state : states)
    {
        state = Vec4u::Zero();
    }

    Array<uint32, GlimmerAllocator> zeros;
    zeros.Resize(MathUtil::Max(GlimmerProbePoolBlocks * 2, NumProbeCounters));

    for (uint32& zero : zeros)
    {
        zero = 0;
    }

    CommandRecorder& cr = frame->cr;

    const auto upload = [&cr](const RWStructuredBuffer& buffer, const void* data, size_t byteSize)
    {
        GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(byteSize);
        Assert(stagingBuffer != nullptr);

        stagingBuffer->Copy(0, byteSize, data);
        stagingBuffer->Flush(0, byteSize);

        cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);
        cr << InsertBarrier(buffer, ResourceState::CopyDst);
        cr << CopyBuffer(stagingBuffer, buffer.gpuBuffer, uint32(byteSize));
    };

    upload(m_blockTableBuffer, blockTable.Data(), blockTable.Size() * sizeof(uint32));

    Array<Vec4i, GlimmerAllocator> cells;
    cells.Resize(GlimmerProbeLevels * GlimmerProbeWindowBlocks);

    for (Vec4i& cell : cells)
    {
        cell = Vec4i::Zero();
    }

    upload(m_cellsBuffer, cells.Data(), cells.Size() * sizeof(Vec4i));
    upload(m_slotsBuffer, slots.Data(), slots.Size() * sizeof(Vec4i));
    upload(m_slotTimesBuffer, zeros.Data(), GlimmerProbePoolBlocks * sizeof(Vec2u));
    upload(m_statesBuffer, states.Data(), states.Size() * sizeof(Vec4u));
    upload(m_countersBuffer, zeros.Data(), NumProbeCounters * sizeof(uint32));
}

void GlimmerProbeVolume::UpdateWindows(const Vec3f& viewerPosition, const Vec3f& lookAhead)
{
    for (uint32 levelIndex = 0; levelIndex < GlimmerProbeLevels; levelIndex++)
    {
        const float spacing = GlimmerHelpers::GetGlimmerProbeLevelSpacing(levelIndex);
        const Vec3i origin = GlimmerHelpers::GetGlimmerProbeWindowOrigin(viewerPosition, lookAhead, levelIndex);
        const Vec3f center = GlimmerHelpers::GetGlimmerProbeWindowCenter(viewerPosition, lookAhead, levelIndex);

        GlimmerProbeLevelShaderData& level = m_shaderData.levels[levelIndex];
        level.windowOrigin = Vec4i(origin.x, origin.y, origin.z, 1);
        level.params = Vec4f(spacing, 1.0f / spacing, 0.0f, 0.0f);
        level.center = Vec4f(center / spacing, 0.0f);
    }
}

void GlimmerProbeVolume::Update(Frame* frame, const GlimmerProbeUpdateInputs& inputs)
{
    HYP_SCOPE;

    if (!inputs.surfaceCache || !inputs.spanCache || !inputs.triangleCache || !inputs.occupancy || !inputs.scratchBuffer
        || !inputs.triangleCache->IsReady() || !inputs.spanCache->GetSpansBuffer().IsInitialized())
    {
        return;
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerProbes);

    CommandRecorder& cr = frame->cr;

    if (m_statesBuffer.IsInitialized())
    {
        const uint32 poolBlocks = uint32(MathUtil::Clamp(g_cvGlimmerProbesPoolBlocks.Get(), 1, int(GlimmerProbePoolBlocks)));
        const uint32 probesPerFrame = MathUtil::Min(uint32(MathUtil::Max(g_cvGlimmerProbesRaysPerFrame.Get(), 0)) / GlimmerProbeRays, GlimmerMaxProbesPerFrame);

        // sized for what the cvars asked for when they were made
        if (poolBlocks > m_poolBlocks || probesPerFrame > m_maxProbesPerFrame)
        {
            ReleaseResources();
        }
    }

    if (!m_statesBuffer.IsInitialized())
    {
        CreateResources(frame);
    }

    GlimmerHelpers::EnsureGlimmerBufferCapacity(*inputs.scratchBuffer, RayRecordSize * m_maxProbesPerFrame * GlimmerProbeRays);

    const RWStructuredBuffer& raysBuffer = *inputs.scratchBuffer;

    m_frameIndex++;

    UpdateWindows(inputs.viewerPosition, inputs.lookAhead);

    const bool useBVH = g_cvGlimmerProbesTracer.Get() != 0;

    const bool hasBVH = useBVH
        && inputs.sceneInstances && inputs.sceneInstances->IsReady() && inputs.sceneInstances->HasTLAS()
        && inputs.triangleCache && inputs.triangleCache->HasBLAS();
    
    const bool hasBricks = !useBVH
        && inputs.sceneInstances && inputs.sceneInstances->IsReady()
        && inputs.brickWorld && inputs.brickWorld->IsReady();
        
    const bool hasNearTracer = hasBVH || hasBricks;
    const uint32 nearTracer = hasBVH ? 2u : (hasBricks ? 1u : 0u);

    const bool isNewTracer = nearTracer != m_nearTracer;

    if (isNewTracer)
    {
        const GlimmerTriangleCacheStats triangleStats = inputs.triangleCache
            ? inputs.triangleCache->GetStats()
            : GlimmerTriangleCacheStats {};

        HYP_LOG(Rendering, Info, "Glimmer: probes trace {} from {} ms; BLAS nodes {} of {}, {} traced instances",
            nearTracer == 2u ? "the BVH" : (nearTracer == 1u ? "the bricks" : "nothing near"),
            Time::Now().ToMilliseconds() - m_startTime.ToMilliseconds(),
            triangleStats.nodesUsed,
            triangleStats.nodesCapacity,
            hasBVH ? inputs.sceneInstances->GetNumTracedInstances() : 0u);
    }

    m_nearTracer = nearTracer;

    // counted per frame, so a low frame rate gets more a frame to keep the same pace
    const float budgetScale = MathUtil::Clamp(inputs.budgetScale, 1.0f, float(MaxBudgetScale));

    const uint32 probesPerFrame = MathUtil::Clamp(
        uint32(float(uint32(MathUtil::Max(g_cvGlimmerProbesRaysPerFrame.Get(), 0)) / GlimmerProbeRays) * budgetScale),
        1u,
        m_maxProbesPerFrame);

    const uint32 sleepMaxInterval = g_cvGlimmerProbesSleep.Get() ? SleepMaxInterval : 0u;

    m_shaderData.info = Vec4u(
        GlimmerProbeLevels,
        GlimmerProbeRays,
        m_frameIndex,
        m_shaderData.info.w);

    m_shaderData.params = Vec4f(
        float(double(Time::Now().ToMilliseconds() - m_startTime.ToMilliseconds()) * 0.001),
        MathUtil::Clamp(g_cvGlimmerVisibility.Get(), 0.0f, 1.0f),
        GlimmerSkyMaxLuminance,
        GlimmerMaxRayDistance);

    m_shaderData.nearField = Vec4f(
        float(MathUtil::Clamp(g_cvGlimmerProbesNearFieldCascades.Get(), 0, int(GlimmerProbeLevels))),
        NearFieldReachSpacings,
        hasNearTracer ? 1.0f : 0.0f,
        GlimmerDefaultGroundAlbedo);

    const GlimmerGroundShaderData& groundShaderData = inputs.surfaceCache->GetGroundShaderData();
    const GpuImageViewRef& groundImageView = inputs.surfaceCache->GetGroundImageView();

    const RWStructuredBuffer* const probeStateBuffers[] = {
        &m_blockTableBuffer,
        &m_cellsBuffer,
        &m_slotsBuffer,
        &m_slotTimesBuffer,
        &m_statesBuffer,
        &m_shBuffer,
        &m_lightBuffer,
        &m_visibilityBuffer,
        &m_trendBuffer,
        &m_countersBuffer
    };

    { // find the blocks around solids, allocate them, then list the probes due for an update
        GlimmerProbeAllocConstants constants {};
        constants.volume = m_shaderData;
        constants.ground = groundShaderData;
        constants.occupancy = *inputs.occupancy;

        constants.budget = Vec4u(
            probesPerFrame,
            BlockReleaseFrames,
            InsideRetryInterval,
            uint32(MathUtil::Clamp(g_cvGlimmerProbesPoolBlocks.Get(), 1, int(m_poolBlocks))));

        constants.params = Vec4f(
            BlockMarginSpacings,
            MinHeightAboveGround,
            float(ClassifyPeriod),
            0.0f);

        // blocks are ranked by distance from where the viewer is headed, so what it flies into gets slots first
        const Vec3f priorityPoint = GlimmerHelpers::GetGlimmerWindowCenter(
            inputs.viewerPosition,
            inputs.lookAhead,
            Vec3f(GlimmerHelpers::GetGlimmerProbeLevelSpacing(0) * float(GlimmerProbeBlock * GlimmerProbeWindow)));

        constants.viewer = Vec4f(priorityPoint.x, priorityPoint.y, priorityPoint.z, 0.0f);

        bool shouldAwaken = isNewTracer;

        uint32 numChangedBoxes = 0;

        if (hasNearTracer && inputs.sceneInstances->GetGeneration() != m_seenInstancesGeneration)
        {
            GlimmerSceneChanges changes;

            if (m_seenInstancesGeneration == ~0u || !inputs.sceneInstances->GetChangesSince(m_seenInstancesGeneration, changes))
            {
                shouldAwaken = true;
            }
            else
            {
                GlimmerSceneChanges worldChanges;

                for (const GlimmerSceneChange& change : changes)
                {
                    if (!change.isLodOnly)
                    {
                        worldChanges.Add(change);
                    }
                }

                changes = std::move(worldChanges);

                changes.Quantize(MaxInstanceBoxes);

                for (const GlimmerSceneChange& change : changes)
                {
                    constants.changedMin[numChangedBoxes] = Vec4f(change.bounds.min, 0.0f);
                    constants.changedMax[numChangedBoxes] = Vec4f(change.bounds.max, 0.0f);

                    numChangedBoxes++;
                }
            }

            m_seenInstancesGeneration = inputs.sceneInstances->GetGeneration();
        }

        if (inputs.localLights && inputs.localLights->GetGeneration() != m_seenLightsGeneration)
        {
            GlimmerSceneChanges changes;

            if (inputs.localLights->GetChangesSince(m_seenLightsGeneration, changes))
            {
                changes.Quantize(MaxLightBoxes);

                for (const GlimmerSceneChange& change : changes)
                {
                    constants.changedMin[numChangedBoxes] = Vec4f(change.bounds.min, 0.0f);
                    constants.changedMax[numChangedBoxes] = Vec4f(change.bounds.max, 0.0f);

                    numChangedBoxes++;
                }
            }
            else
            {
                shouldAwaken = true;
            }

            m_seenLightsGeneration = inputs.localLights->GetGeneration();
        }

        for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
        {
            const float texelSize = GetGlimmerGroundTexelSize(levelIndex);

            for (const GlimmerTexelRect& rect : { inputs.surfaceCache->GetUploadedRect(levelIndex), inputs.spanCache->GetFilledRect(levelIndex) })
            {
                if (rect.IsEmpty())
                {
                    continue;
                }

                constants.changedMin[numChangedBoxes] = Vec4f(float(rect.min.x) * texelSize, -1e6f, float(rect.min.y) * texelSize, 0.0f);
                constants.changedMax[numChangedBoxes] = Vec4f(float(rect.max.x) * texelSize, 1e6f, float(rect.max.y) * texelSize, 0.0f);

                numChangedBoxes++;
            }
        }

        s_statGlimmerProbeWakes += shouldAwaken ? 1u : 0u;
        s_statGlimmerProbeChangedBoxes += numChangedBoxes;

        constants.wake = Vec4u(
            shouldAwaken ? 1u : 0u,
            numChangedBoxes,
            sleepMaxInterval,
            SafetyRefreshSeconds);

        // a probe whose rays could reach bricks still being built keeps what it has until they are
        if (hasBricks)
        {
            inputs.brickWorld->GetBuildingChunks(constants.brickChunkOrigin, constants.brickChunksBuilding);
        }

        for (const RWStructuredBuffer* buffer : probeStateBuffers)
        {
            cr << InsertBarrier(*buffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        }

        auto dispatchPass = [&](const StaticShaderPropertyId& modeProperty, uint32 levelIndex, const Vec3u& groups)
        {
            constants.params.w = float(levelIndex);

            GpuBuffer* cbuffer = nullptr;
            size_t cbufferOffset = 0;
            size_t cbufferSize = 0;

            RI.cbufferAllocator->Write(&constants);
            RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

            ShaderPropertySet shaderProperties;
            shaderProperties.Add(modeProperty);

            cr << SetCurrentShader(ShaderDesc(NAME("GlimmerProbeAlloc"), shaderProperties));

            uint32 uniformIndex = 0;

            cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
            cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, groundImageView);
            cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyTexture"_sh, inputs.occupancyImageView);
            cr << SetShaderUniform(uniformIndex++, "OutBlockTable"_sh, m_blockTableBuffer);
            cr << SetShaderUniform(uniformIndex++, "OutCells"_sh, m_cellsBuffer);
            cr << SetShaderUniform(uniformIndex++, "OutSlots"_sh, m_slotsBuffer);
            cr << SetShaderUniform(uniformIndex++, "OutSlotTimes"_sh, m_slotTimesBuffer);
            cr << SetShaderUniform(uniformIndex++, "OutStates"_sh, m_statesBuffer);
            cr << SetShaderUniform(uniformIndex++, "OutSH"_sh, m_shBuffer);
            cr << SetShaderUniform(uniformIndex++, "OutLight"_sh, m_lightBuffer);
            cr << SetShaderUniform(uniformIndex++, "OutVisibility"_sh, m_visibilityBuffer);
            cr << SetShaderUniform(uniformIndex++, "OutTrend"_sh, m_trendBuffer);
            cr << SetShaderUniform(uniformIndex++, "OutCounters"_sh, m_countersBuffer);

            cr << DispatchCompute(groups);

            for (const RWStructuredBuffer* buffer : probeStateBuffers)
            {
                cr << InsertBarrier(*buffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);
            }
        };

        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerProbeClassify);

            // a group per block, finest level first
            for (uint32 levelIndex = 0; levelIndex < GlimmerProbeLevels; levelIndex++)
            {
                dispatchPass(s_propAllocModeClassify, levelIndex, Vec3u { GlimmerProbeWindowBlocks, 1, 1 });
            }
        }

        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerProbeAlloc);

            dispatchPass(s_propAllocModeAlloc, 0, Vec3u { 1, 1, 1 });
        }

        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerProbeList);

            dispatchPass(s_propAllocModeList, 0, Vec3u { m_poolBlocks * GlimmerProbesPerBlock / ListGroupSize, 1, 1 });
        }
    }

    GlimmerSkyShaderData skyData;
    EnvProbeShaderData skyProbeData;
    GlimmerHelpers::GetGlimmerSkyShaderData(inputs.skyProbe, skyData, skyProbeData);

    // the far field is sampled once it has been traced; until then its constants stay zeroed and the trace treats it as covering nothing
    const bool hasSHVolume = inputs.shVolume && inputs.shVolume->IsReady();


    // the probes the hits sample (last frame's) and the list are read only from here to the blend
    for (const RWStructuredBuffer* buffer : probeStateBuffers)
    {
        cr << InsertBarrier(*buffer, ResourceState::ShaderResource, ShaderModuleType::Compute);
    }

    { // trace
        GlimmerProbeTraceConstants constants {};
        constants.volume = m_shaderData;
        constants.ground = groundShaderData;
        constants.spans = inputs.spanCache->GetShaderData();
        constants.dispatch = Vec4u(probesPerFrame, 0, 0, 0);
        constants.params = Vec4f(GlimmerHelpers::GetGlimmerFoliageExtinction(), 0.0f, 0.0f, 0.0f);
        constants.sky = skyData;

        if (hasSHVolume)
        {
            constants.sh = inputs.shVolume->GetShaderData();
        }

        constants.skyProbe = skyProbeData;

        if (hasBricks)
        {
            constants.bricks = inputs.brickWorld->GetShaderData();
        }

        if (hasBVH)
        {
            constants.bvh = Vec4u(inputs.sceneInstances->GetNumTracedInstances(), 0u, 0u, 0u);
        }

        if (inputs.localLights)
        {
            constants.localLights = inputs.localLights->GetShaderData();
        }

        const GpuImageViewRef relightImageView = inputs.relight ? inputs.relight->GetImageView() : GpuImageViewRef();

        const auto bindTraceResources = [&](const StaticShaderPropertyId& modeProperty, const GlimmerProbeTraceConstants& dispatchConstants)
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

            cr << SetCurrentShader(ShaderDesc(NAME("GlimmerProbeTrace"), shaderProperties));

            uint32 uniformIndex = 0;

            cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
            cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
            cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
            cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());

            if (useBVH && hasBVH)
            {
                cr << SetShaderUniform(uniformIndex++, "GlimmerTLASNodesBuffer"_sh, inputs.sceneInstances->GetTLASNodesBuffer());
                cr << SetShaderUniform(uniformIndex++, "GlimmerTracedInstancesBuffer"_sh, inputs.sceneInstances->GetTracedInstancesBuffer());
                cr << SetShaderUniform(uniformIndex++, "GlimmerBLASNodesBuffer"_sh, inputs.triangleCache->GetNodesBuffer());
                cr << SetShaderUniform(uniformIndex++, "GlimmerTrianglesBuffer"_sh, inputs.triangleCache->GetTrianglesBuffer());
            }
            else if (useBVH)
            {
                const RWStructuredBuffer& placeholder = GetPlaceholderBuffer(sizeof(uint32));

                for (const StringHash name : { "GlimmerTLASNodesBuffer"_sh, "GlimmerTracedInstancesBuffer"_sh, "GlimmerBLASNodesBuffer"_sh, "GlimmerTrianglesBuffer"_sh })
                {
                    cr << SetShaderUniform(uniformIndex++, name, placeholder);
                }
            }
            else if (hasBricks)
            {
                cr << SetShaderUniform(uniformIndex++, "GlimmerBrickTableBuffer"_sh, inputs.brickWorld->GetTableBuffer());
                cr << SetShaderUniform(uniformIndex++, "GlimmerBrickIndexBuffer"_sh, inputs.brickWorld->GetIndexBuffer());
                cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingsBuffer"_sh, inputs.brickWorld->GetCrossingsBuffer());
                cr << SetShaderUniform(uniformIndex++, "GlimmerBrickMaterialBuffer"_sh, inputs.brickWorld->GetMaterialBuffer());
            }
            else
            {
                const RWStructuredBuffer& placeholder = GetPlaceholderBuffer(sizeof(uint32));

                for (const StringHash name : { "GlimmerBrickTableBuffer"_sh, "GlimmerBrickIndexBuffer"_sh, "GlimmerBrickCrossingsBuffer"_sh, "GlimmerBrickMaterialBuffer"_sh })
                {
                    cr << SetShaderUniform(uniformIndex++, name, placeholder);
                }
            }

            cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, groundImageView);
            cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, inputs.surfaceCache->GetGroundAlbedoImageView());
            cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, inputs.spanCache->GetSpansBuffer());
            cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, inputs.spanCache->GetHeightBoundsBuffer());
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBlockTableBuffer"_sh, m_blockTableBuffer);
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeLightBuffer"_sh, m_lightBuffer);
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStatesBuffer"_sh, m_statesBuffer);
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeVisibilityBuffer"_sh, m_visibilityBuffer);
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSlotsBuffer"_sh, m_slotsBuffer);
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeCountersBuffer"_sh, m_countersBuffer);
            cr << SetShaderUniform(uniformIndex++, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
            cr << SetShaderUniform(uniformIndex++, "CloudWeatherMapTexture"_sh, inputs.cloudPass != nullptr ? inputs.cloudPass->GetWeatherMapView() : RI.placeholderData->GetImageView2D1x1R8Array());
            cr << SetShaderUniform(uniformIndex++, "CloudShadowMapTexture"_sh, inputs.cloudPass != nullptr ? inputs.cloudPass->GetShadowMapView() : RI.placeholderData->GetImageView2D1x1R8());

            if (hasSHVolume)
            {
                cr << SetShaderUniform(uniformIndex++, "GlimmerSHDataTexture"_sh, inputs.shVolume->GetDataImageView());
                cr << SetShaderUniform(uniformIndex++, "GlimmerSHStateTexture"_sh, inputs.shVolume->GetStateImageView());
                cr << SetShaderUniform(uniformIndex++, "GlimmerSHRadianceTexture"_sh, inputs.shVolume->GetRadianceImageView());
                cr << SetShaderUniform(uniformIndex++, "GlimmerSHLightBuffer"_sh, inputs.shVolume->GetLightBuffer());
            }
            else
            {
                // placeholders where the far field isn't ready: its zeroed constants keep the shader from sampling them
                const GpuImageViewRef& placeholderView = RI.placeholderData->GetImageView3D1x1x1R8();

                cr << SetShaderUniform(uniformIndex++, "GlimmerSHDataTexture"_sh, placeholderView);
                cr << SetShaderUniform(uniformIndex++, "GlimmerSHStateTexture"_sh, placeholderView);
                cr << SetShaderUniform(uniformIndex++, "GlimmerSHRadianceTexture"_sh, placeholderView);
                cr << SetShaderUniform(uniformIndex++, "GlimmerSHLightBuffer"_sh, GetPlaceholderBuffer(sizeof(uint32)));
            }

            cr << SetShaderUniform(uniformIndex++, "OutRays"_sh, raysBuffer);

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

                GlimmerProbeTraceConstants relightConstants = constants;
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

        cr << InsertBarrier(raysBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        for (const StaticShaderPropertyId* modeProperty : { &s_propTraceModeTrace, &s_propTraceModeShade })
        {
            ENGINE_STAT_GPU_SCOPE(modeProperty == &s_propTraceModeTrace ? &s_statGlimmerProbeTrace : &s_statGlimmerProbeShade);

            bindTraceResources(*modeProperty, constants);

            cr << DispatchCompute(Vec3u { (probesPerFrame + ProbesPerTraceGroup - 1) / ProbesPerTraceGroup, 1, 1 });

            // the shade pass reads the trace pass's hits and adds to its rays
            cr << InsertBarrier(raysBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        }
    }

    { // blend
        ENGINE_STAT_GPU_SCOPE(&s_statGlimmerProbeBlend);

        GlimmerProbeBlendConstants constants {};
        constants.volume = m_shaderData;

        constants.dispatch = Vec4u(
            probesPerFrame,
            MaxRelocations,
            MinHistory,
            MinHistoryChanging);

        constants.params = Vec4f(
            HistorySeconds,
            HistorySecondsChanging,
            RelocateMargin,
            float(sleepMaxInterval));
        constants.light = inputs.light;

        if (hasSHVolume)
        {
            constants.sh = inputs.shVolume->GetShaderData();
        }

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        cr << InsertBarrier(raysBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);

        for (const RWStructuredBuffer* buffer : { &m_shBuffer, &m_lightBuffer, &m_statesBuffer, &m_visibilityBuffer, &m_trendBuffer, &m_slotTimesBuffer })
        {
            cr << InsertBarrier(*buffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        }

        ShaderPropertySet blendProperties;
        blendProperties.Add(s_propBlendModeBlend);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerProbeBlend"), blendProperties));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "Rays"_sh, raysBuffer);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeCountersBuffer"_sh, m_countersBuffer);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSlotsBuffer"_sh, m_slotsBuffer);
        cr << SetShaderUniform(uniformIndex++, "OutSH"_sh, m_shBuffer);
        cr << SetShaderUniform(uniformIndex++, "OutLight"_sh, m_lightBuffer);
        cr << SetShaderUniform(uniformIndex++, "OutStates"_sh, m_statesBuffer);
        cr << SetShaderUniform(uniformIndex++, "OutVisibility"_sh, m_visibilityBuffer);
        cr << SetShaderUniform(uniformIndex++, "OutTrend"_sh, m_trendBuffer);
        cr << SetShaderUniform(uniformIndex++, "OutSlotTimes"_sh, m_slotTimesBuffer);

        if (hasSHVolume)
        {
            cr << SetShaderUniform(uniformIndex++, "GlimmerSHStateTexture"_sh, inputs.shVolume->GetStateImageView());
            cr << SetShaderUniform(uniformIndex++, "GlimmerSHLightBuffer"_sh, inputs.shVolume->GetLightBuffer());
        }
        else
        {
            cr << SetShaderUniform(uniformIndex++, "GlimmerSHStateTexture"_sh, RI.placeholderData->GetImageView3D1x1x1R8());
            cr << SetShaderUniform(uniformIndex++, "GlimmerSHLightBuffer"_sh, GetPlaceholderBuffer(sizeof(uint32)));
        }

        // group per probe
        cr << DispatchCompute(Vec3u { probesPerFrame, 1, 1 });

        if (!m_hasComposedLight || memcmp(&m_composedLight, &inputs.light, sizeof(GlimmerLightShaderData)) != 0)
        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerProbeCompose);

            cr << InsertBarrier(m_shBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);
            cr << InsertBarrier(m_lightBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);

            ShaderPropertySet composeProperties;
            composeProperties.Add(s_propBlendModeCompose);

            cr << SetCurrentShader(ShaderDesc(NAME("GlimmerProbeBlend"), composeProperties));

            uniformIndex = 0;

            cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
            cr << SetShaderUniform(uniformIndex++, "OutSH"_sh, m_shBuffer);
            cr << SetShaderUniform(uniformIndex++, "OutLight"_sh, m_lightBuffer);

            cr << DispatchCompute(Vec3u { m_poolBlocks * GlimmerProbesPerBlock / ComposeGroupSize, 1, 1 });

            m_composedLight = inputs.light;
            m_hasComposedLight = true;

            s_statGlimmerProbeComposes++;
        }
    }

    for (const RWStructuredBuffer* buffer : probeStateBuffers)
    {
        cr << InsertBarrier(*buffer, ResourceState::ShaderResource);
    }

    m_shaderData.info.w = 1;
}

#pragma endregion GlimmerProbeVolume

} // namespace Hyperion
