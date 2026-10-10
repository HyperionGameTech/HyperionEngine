/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/SWRT/GlimmerSWRTProbeVolume.hpp>
#include <Rendering/Glimmer/GlimmerRelight.hpp>
#include <Rendering/Glimmer/GlimmerBLASCache.hpp>
#include <Rendering/Glimmer/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerFootprintMask.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHVolume.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHOccupancy.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTCVars.hpp>

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

namespace Hyperion {

static EngineStatGpuTimer s_statGlimmerProbes("Rendering/GPU/Glimmer/Probes");
static EngineStatGpuTimer s_statGlimmerProbeClassify("Rendering/GPU/Glimmer/ProbeClassify");
static EngineStatGpuTimer s_statGlimmerProbeAlloc("Rendering/GPU/Glimmer/ProbeAlloc");
static EngineStatGpuTimer s_statGlimmerProbeList("Rendering/GPU/Glimmer/ProbeList");
static EngineStatGpuTimer s_statGlimmerProbeTrace("Rendering/GPU/Glimmer/ProbeTrace");
static EngineStatGpuTimer s_statGlimmerProbeShade("Rendering/GPU/Glimmer/ProbeShade");
static EngineStatGpuTimer s_statGlimmerProbeBlend("Rendering/GPU/Glimmer/ProbeBlend");
static EngineStatGpuTimer s_statGlimmerRelight("Rendering/GPU/Glimmer/Relight");

static EngineStatCounter<uint32> s_statGlimmerRelightTexels("Rendering/Glimmer/RelightTexels");
static EngineStatCounter<uint32> s_statGlimmerProbeWakes("Rendering/Glimmer/ProbeWakes");
static EngineStatCounter<uint32> s_statGlimmerProbeChangedBoxes("Rendering/Glimmer/ProbeChangedBoxes");

static StaticShaderPropertyId s_propAllocModeClassify { ShaderProperty(NAME("MODE"), NAME("CLASSIFY")) };
static StaticShaderPropertyId s_propAllocModeAlloc { ShaderProperty(NAME("MODE"), NAME("ALLOC")) };
static StaticShaderPropertyId s_propAllocModeList { ShaderProperty(NAME("MODE"), NAME("LIST")) };
static StaticShaderPropertyId s_propTraceModeTrace { ShaderProperty(NAME("MODE"), NAME("TRACE")) };
static StaticShaderPropertyId s_propTraceModeShade { ShaderProperty(NAME("MODE"), NAME("SHADE")) };
static StaticShaderPropertyId s_propTraceModeRelight { ShaderProperty(NAME("MODE"), NAME("RELIGHT")) };

static constexpr size_t RayHitSize = 4 * sizeof(Vec4f);

static constexpr uint32 ProbesPerTraceGroup = 2;
static constexpr uint32 RelightGroupSize = 64; // GlimmerSWRTProbeTrace's RAYS_PER_PROBE * PROBES_PER_GROUP
static_assert(GlimmerProbeRays == 32);

static constexpr uint32 ListGroupSize = 64;

static constexpr uint32 NumProbeCounters = 16;

static constexpr uint32 MaxChangedBoxes = 16; // GLIMMER_PROBE_MAX_CHANGED_BOXES

static constexpr uint32 MaxRelocations = 2;

static constexpr float NearFieldReachSpacings = 8.0f;

struct GlimmerProbeAllocConstants
{
    GlimmerProbeVolumeShaderData volume;
    GlimmerGroundShaderData ground;
    GlimmerSHOccupancyShaderData occupancy;
    Vec4u budget; // x = probes traced per frame, y = frames an unwanted block keeps its slot, z = updates between retries of probes inside solids, w = pool slots to use
    Vec4f params; // x = block margin in spacings, y = how far above the ground a solid has to be to want probes around it
    Vec4f viewer; // xyz = viewer position
    Vec4u wake;   // x = 1 when the lighting changed, y = changed boxes, z = longest sleep interval, w = estimates a woken probe's history keeps at most
    Vec4f changedMin[MaxChangedBoxes];
    Vec4f changedMax[MaxChangedBoxes];
};

struct GlimmerProbeTraceConstants
{
    GlimmerProbeVolumeShaderData volume;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
    Vec4u dispatch; // x = probes traced per frame at most
    Vec4f params;   // x = foliage extinction
    GlimmerSkyShaderData sky;
    GlimmerFootprintMaskShaderData mask;
    GlimmerSHVolumeShaderData sh;
    EnvProbeShaderData skyProbe;
    GlimmerRelightShaderData relight;
    Vec4i relightRect; // relight pass: xy = absolute texel of the rect to light, zw = its extent
    Vec4u relightInfo; // relight pass: x = ground level, y = groups along x
};

struct GlimmerProbeBlendConstants
{
    GlimmerProbeVolumeShaderData volume;
    Vec4u dispatch; // x = probes traced per frame at most, y = moves a probe gets to get out of a solid, z = updates the history averages at least while its light holds, w = ...while it changes
    Vec4f params;   // x = seconds the history spans while the light holds, y = while it changes, z = how far past a back face a probe moves (m)
};

static GpuBufferRef CreateProbeBuffer(size_t elementSize, size_t numElements)
{
    GpuBufferRef buffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, elementSize * numElements, alignof(Vec4f));
    Check(buffer->Create());

    return buffer;
}


#pragma region GlimmerSWRTProbeVolume

GlimmerSWRTProbeVolume::GlimmerSWRTProbeVolume()
    : m_startTime(Time::Now()),
      m_frameIndex(0),
      m_seenTLASGeneration(~0u),
      m_shaderData {}
{
}

GlimmerSWRTProbeVolume::~GlimmerSWRTProbeVolume()
{
    for (GpuBufferRef* buffer : { &m_blockTableBuffer, &m_cellsBuffer, &m_slotsBuffer, &m_slotAgesBuffer, &m_blockWakeBuffer, &m_statesBuffer, &m_shBuffer, &m_visibilityBuffer,
             &m_trendBuffer, &m_countersBuffer, &m_updateListBuffer, &m_raysBuffer, &m_rayHitsBuffer, &m_placeholderBuffer })
    {
        EnqueueDeletion(std::move(*buffer));
    }
}

const GpuBufferRef& GlimmerSWRTProbeVolume::GetBufferOrPlaceholder(const GpuBufferRef& buffer) const
{
    if (buffer.IsValid())
    {
        return buffer;
    }

    if (!m_placeholderBuffer.IsValid())
    {
        GlimmerSWRTProbeVolume* self = const_cast<GlimmerSWRTProbeVolume*>(this);

        self->m_placeholderBuffer = CreateProbeBuffer(sizeof(Vec4f), 1);
    }

    return m_placeholderBuffer;
}

const GpuBufferRef& GlimmerSWRTProbeVolume::GetBlockTableBuffer() const
{
    return GetBufferOrPlaceholder(m_blockTableBuffer);
}

const GpuBufferRef& GlimmerSWRTProbeVolume::GetSHBuffer() const
{
    return GetBufferOrPlaceholder(m_shBuffer);
}

const GpuBufferRef& GlimmerSWRTProbeVolume::GetStatesBuffer() const
{
    return GetBufferOrPlaceholder(m_statesBuffer);
}

const GpuBufferRef& GlimmerSWRTProbeVolume::GetVisibilityBuffer() const
{
    return GetBufferOrPlaceholder(m_visibilityBuffer);
}

const GpuBufferRef& GlimmerSWRTProbeVolume::GetSlotsBuffer() const
{
    return GetBufferOrPlaceholder(m_slotsBuffer);
}

void GlimmerSWRTProbeVolume::CreateResources(Frame* frame)
{
    m_blockTableBuffer = CreateProbeBuffer(sizeof(uint32), GlimmerProbeLevels * GlimmerProbeWindowBlocks);
    m_cellsBuffer = CreateProbeBuffer(sizeof(Vec4i), GlimmerProbeLevels * GlimmerProbeWindowBlocks);
    m_slotsBuffer = CreateProbeBuffer(sizeof(Vec4i), GlimmerProbePoolBlocks);
    m_slotAgesBuffer = CreateProbeBuffer(sizeof(uint32), GlimmerProbePoolBlocks);
    m_blockWakeBuffer = CreateProbeBuffer(sizeof(uint32), GlimmerProbePoolBlocks);
    m_statesBuffer = CreateProbeBuffer(sizeof(Vec4u), GlimmerProbePoolProbes);
    m_shBuffer = CreateProbeBuffer(sizeof(Vec4f), GlimmerProbePoolProbes * 3);
    m_visibilityBuffer = CreateProbeBuffer(sizeof(uint32), size_t(GlimmerProbePoolProbes) * GlimmerProbeVisibilityTexels);
    m_trendBuffer = CreateProbeBuffer(sizeof(Vec4f), GlimmerProbePoolProbes);
    m_countersBuffer = CreateProbeBuffer(sizeof(uint32), NumProbeCounters);
    m_updateListBuffer = CreateProbeBuffer(sizeof(uint32), GlimmerMaxProbesPerFrame);
    m_raysBuffer = CreateProbeBuffer(sizeof(Vec4f), size_t(GlimmerMaxProbesPerFrame) * GlimmerProbeRays);
    m_rayHitsBuffer = CreateProbeBuffer(RayHitSize, size_t(GlimmerMaxProbesPerFrame) * GlimmerProbeRays);

    // every slot free, no block with a slot or classified, every counter zero
    Array<uint32> blockTable;
    blockTable.Resize(GlimmerProbeLevels * GlimmerProbeWindowBlocks);

    for (uint32& entry : blockTable)
    {
        entry = ~0u;
    }

    Array<Vec4i> slots;
    slots.Resize(GlimmerProbePoolBlocks);

    for (Vec4i& slot : slots)
    {
        slot = Vec4i(0, 0, 0, -1);
    }

    Array<Vec4u> states;
    states.Resize(GlimmerProbePoolProbes);

    for (Vec4u& state : states)
    {
        state = Vec4u::Zero();
    }

    Array<uint32> zeros;
    zeros.Resize(MathUtil::Max(GlimmerProbePoolBlocks, NumProbeCounters));

    for (uint32& zero : zeros)
    {
        zero = 0;
    }

    CommandRecorder& cr = frame->cr;

    const auto upload = [&cr](const GpuBufferRef& buffer, const void* data, size_t byteSize)
    {
        GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(byteSize);
        Assert(stagingBuffer != nullptr);

        stagingBuffer->Copy(0, byteSize, data);
        stagingBuffer->Flush(0, byteSize);

        cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);
        cr << InsertBarrier(buffer.Get(), ResourceState::CopyDst);
        cr << CopyBuffer(stagingBuffer, buffer.Get(), uint32(byteSize));
    };

    upload(m_blockTableBuffer, blockTable.Data(), blockTable.Size() * sizeof(uint32));

    Array<Vec4i> cells;
    cells.Resize(GlimmerProbeLevels * GlimmerProbeWindowBlocks);

    for (Vec4i& cell : cells)
    {
        cell = Vec4i::Zero();
    }

    upload(m_cellsBuffer, cells.Data(), cells.Size() * sizeof(Vec4i));
    upload(m_slotsBuffer, slots.Data(), slots.Size() * sizeof(Vec4i));
    upload(m_slotAgesBuffer, zeros.Data(), GlimmerProbePoolBlocks * sizeof(uint32));
    upload(m_blockWakeBuffer, zeros.Data(), GlimmerProbePoolBlocks * sizeof(uint32));
    upload(m_statesBuffer, states.Data(), states.Size() * sizeof(Vec4u));
    upload(m_countersBuffer, zeros.Data(), NumProbeCounters * sizeof(uint32));
}

void GlimmerSWRTProbeVolume::UpdateWindows(const Vec3f& viewerPosition)
{
    const int32 halfWindow = int32(GlimmerProbeWindow / 2);

    for (uint32 levelIndex = 0; levelIndex < GlimmerProbeLevels; levelIndex++)
    {
        const float spacing = GetGlimmerProbeLevelSpacing(levelIndex);
        const float blockSize = spacing * float(GlimmerProbeBlock);

        const Vec3i viewerBlock = Vec3i(
            int32(MathUtil::Floor(viewerPosition.x / blockSize)),
            int32(MathUtil::Floor(viewerPosition.y / blockSize)),
            int32(MathUtil::Floor(viewerPosition.z / blockSize)));

        GlimmerProbeLevelShaderData& level = m_shaderData.levels[levelIndex];
        level.windowOrigin = Vec4i(viewerBlock.x - halfWindow, viewerBlock.y - halfWindow, viewerBlock.z - halfWindow, 1);
        level.params = Vec4f(spacing, 1.0f / spacing, 0.0f, 0.0f);
    }
}

void GlimmerSWRTProbeVolume::Update(Frame* frame, const GlimmerSWRTProbeUpdateInputs& inputs)
{
    HYP_SCOPE;

    if (!inputs.surfaceCache || !inputs.spanCache || !inputs.blasCache || !inputs.footprintMask || !inputs.occupancy
        || !inputs.blasCache->IsReady() || !inputs.spanCache->GetSpansBuffer().IsValid())
    {
        return;
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerProbes);

    CommandRecorder& cr = frame->cr;

    if (!m_raysBuffer.IsValid())
    {
        CreateResources(frame);
    }

    m_frameIndex++;

    UpdateWindows(inputs.viewerPosition);

    const bool hasSWRTScene = inputs.tlas && inputs.tlas->IsReady();

    const uint32 probesPerFrame = MathUtil::Clamp(uint32(MathUtil::Max(g_cvGlimmerSWRTProbesRaysPerFrame.Get(), 0)) / GlimmerProbeRays, 1u, GlimmerMaxProbesPerFrame);

    const uint32 sleepMaxInterval = g_cvGlimmerSWRTProbesSleep.Get() ? uint32(MathUtil::Clamp(g_cvGlimmerSWRTProbesSleepMaxInterval.Get(), 0, 3)) : 0u;

    m_shaderData.info = Vec4u(GlimmerProbeLevels, GlimmerProbeRays, m_frameIndex, m_shaderData.info.w);
    m_shaderData.params = Vec4f(
        float(double(Time::Now().ToMilliseconds() - m_startTime.ToMilliseconds()) * 0.001),
        MathUtil::Clamp(g_cvGlimmerVisibility.Get(), 0.0f, 1.0f),
        GlimmerSkyMaxLuminance,
        GlimmerMaxRayDistance);
    m_shaderData.nearField = Vec4f(
        float(MathUtil::Clamp(g_cvGlimmerSWRTNearFieldCascades.Get(), 0, int(GlimmerProbeLevels))),
        NearFieldReachSpacings,
        hasSWRTScene ? float(inputs.tlas->GetNumInstances()) : 0.0f,
        MathUtil::Clamp(g_cvGlimmerGroundAlbedo.Get(), 0.0f, 1.0f));

    const GlimmerGroundShaderData& groundShaderData = inputs.surfaceCache->GetGroundShaderData();
    const GpuImageViewRef& groundImageView = inputs.surfaceCache->GetGroundImageView();

    const GpuBufferRef* const probeStateBuffers[] = { &m_blockTableBuffer, &m_cellsBuffer, &m_slotsBuffer, &m_slotAgesBuffer, &m_blockWakeBuffer, &m_statesBuffer, &m_shBuffer, &m_visibilityBuffer, &m_trendBuffer, &m_countersBuffer, &m_updateListBuffer };

    { // find the blocks around solids, allocate them, then list the probes due for an update
        GlimmerProbeAllocConstants constants {};
        constants.volume = m_shaderData;
        constants.ground = groundShaderData;
        constants.occupancy = *inputs.occupancy;
        constants.budget = Vec4u(
            probesPerFrame,
            uint32(MathUtil::Max(g_cvGlimmerSWRTProbesBlockReleaseFrames.Get(), 0)),
            uint32(MathUtil::Max(g_cvGlimmerSWRTProbesInsideRetryInterval.Get(), 1)),
            uint32(MathUtil::Clamp(g_cvGlimmerSWRTProbesPoolBlocks.Get(), 1, int(GlimmerProbePoolBlocks))));
        constants.params = Vec4f(
            MathUtil::Clamp(g_cvGlimmerSWRTProbesBlockMargin.Get(), 0.0f, 2.0f),
            g_cvGlimmerSWRTProbesMinHeightAboveGround.Get(),
            float(MathUtil::Max(g_cvGlimmerSWRTProbesClassifyPeriod.Get(), 1)),
            0.0f);
        constants.viewer = Vec4f(inputs.viewerPosition.x, inputs.viewerPosition.y, inputs.viewerPosition.z, 0.0f);

        bool wakeEverything = inputs.wakeLighting;
        uint32 numChangedBoxes = 0;

        if (hasSWRTScene && inputs.tlas->GetGeneration() != m_seenTLASGeneration)
        {
            GlimmerSceneChanges changes;

            if (m_seenTLASGeneration == ~0u || !inputs.tlas->GetChangesSince(m_seenTLASGeneration, changes))
            {
                wakeEverything = true;
            }
            else
            {
                if (g_cvGlimmerSkipLodOnlyChanges.Get())
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
                }

                changes.Quantize(MaxChangedBoxes);

                for (const GlimmerSceneChange& change : changes)
                {
                    constants.changedMin[numChangedBoxes] = Vec4f(change.bounds.min, 0.0f);
                    constants.changedMax[numChangedBoxes] = Vec4f(change.bounds.max, 0.0f);

                    numChangedBoxes++;
                }
            }

            m_seenTLASGeneration = inputs.tlas->GetGeneration();
        }

        s_statGlimmerProbeWakes += wakeEverything ? 1u : 0u;
        s_statGlimmerProbeChangedBoxes += numChangedBoxes;

        constants.wake = Vec4u(
            wakeEverything ? 1u : 0u,
            numChangedBoxes,
            sleepMaxInterval,
            uint32(MathUtil::Clamp(g_cvGlimmerSWRTProbesMinHistoryChanging.Get(), 1, 255)));

        for (const GpuBufferRef* buffer : probeStateBuffers)
        {
            cr << InsertBarrier(buffer->Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
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

            cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSWRTProbeAlloc"), shaderProperties));

            uint32 uniformIndex = 0;

            cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
            cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, groundImageView);
            cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyTexture"_sh, inputs.occupancyImageView);
            cr << SetShaderUniform(uniformIndex++, "OutBlockTable"_sh, m_blockTableBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "OutCells"_sh, m_cellsBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4i)));
            cr << SetShaderUniform(uniformIndex++, "OutSlots"_sh, m_slotsBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4i)));
            cr << SetShaderUniform(uniformIndex++, "OutSlotAges"_sh, m_slotAgesBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "OutBlockWake"_sh, m_blockWakeBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "OutStates"_sh, m_statesBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4u)));
            cr << SetShaderUniform(uniformIndex++, "OutSH"_sh, m_shBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4f)));
            cr << SetShaderUniform(uniformIndex++, "OutVisibility"_sh, m_visibilityBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "OutTrend"_sh, m_trendBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4f)));
            cr << SetShaderUniform(uniformIndex++, "OutCounters"_sh, m_countersBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "OutUpdateList"_sh, m_updateListBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));

            cr << DispatchCompute(groups);

            for (const GpuBufferRef* buffer : probeStateBuffers)
            {
                cr << InsertBarrier(buffer->Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
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

            dispatchPass(s_propAllocModeList, 0, Vec3u { GlimmerProbePoolProbes / ListGroupSize, 1, 1 });
        }
    }

    GlimmerSkyShaderData skyData;
    EnvProbeShaderData skyProbeData;
    GetGlimmerSkyShaderData(inputs.skyProbe, skyData, skyProbeData);

    // the far field is sampled once it has been traced; until then its constants stay zeroed and the trace treats it as covering nothing
    const bool hasSHVolume = inputs.shVolume && inputs.shVolume->IsReady();

    const GpuBufferRef& tlasNodes = hasSWRTScene ? inputs.tlas->GetNodesBuffer() : inputs.blasCache->GetNodesBuffer();
    const GpuBufferRef& tlasInstances = hasSWRTScene ? inputs.tlas->GetInstancesBuffer() : inputs.blasCache->GetTrianglesBuffer();

    // the probes the hits sample (last frame's) and the list are read only from here to the blend
    for (const GpuBufferRef* buffer : probeStateBuffers)
    {
        cr << InsertBarrier(buffer->Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    }

    { // trace
        GlimmerProbeTraceConstants constants {};
        constants.volume = m_shaderData;
        constants.ground = groundShaderData;
        constants.spans = inputs.spanCache->GetShaderData();
        constants.dispatch = Vec4u(probesPerFrame, 0, 0, 0);
        constants.params = Vec4f(GetGlimmerFoliageExtinction(), 0.0f, 0.0f, 0.0f);
        constants.sky = skyData;

        // a mask that isn't built yet is left invalid, which has the trace treat all of the SWRT region as occupied
        if (hasSWRTScene && inputs.footprintMask->IsReady())
        {
            constants.mask = inputs.footprintMask->GetShaderData();
        }

        if (hasSHVolume)
        {
            constants.sh = inputs.shVolume->GetShaderData();
        }

        constants.skyProbe = skyProbeData;

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

            cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSWRTProbeTrace"), shaderProperties));

            uint32 uniformIndex = 0;

            cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
            cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
            cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
            cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
            cr << SetShaderUniform(uniformIndex++, "GlimmerTLASNodesBuffer"_sh, tlasNodes.Get(), ShaderDataOffset(0, sizeof(GlimmerBVHNode)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerInstancesBuffer"_sh, tlasInstances.Get(), ShaderDataOffset(0, sizeof(GlimmerInstanceShaderData)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerBLASNodesBuffer"_sh, inputs.blasCache->GetNodesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerBLASNode)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerBLASTrianglesBuffer"_sh, inputs.blasCache->GetTrianglesBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, groundImageView);
            cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, inputs.surfaceCache->GetGroundAlbedoImageView());
            cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, inputs.spanCache->GetSpansBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, inputs.spanCache->GetHeightBoundsBuffer().Get(), ShaderDataOffset(0, sizeof(float)));
            cr << SetShaderUniform(uniformIndex++, "FootprintMaskBuffer"_sh, inputs.footprintMask->GetMaskBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBlockTableBuffer"_sh, m_blockTableBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSHBuffer"_sh, m_shBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4f)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStatesBuffer"_sh, m_statesBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4u)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeVisibilityBuffer"_sh, m_visibilityBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSlotsBuffer"_sh, m_slotsBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4i)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeUpdateListBuffer"_sh, m_updateListBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "GlimmerProbeCountersBuffer"_sh, m_countersBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
            cr << SetShaderUniform(uniformIndex++, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
            cr << SetShaderUniform(uniformIndex++, "CloudWeatherMapTexture"_sh, inputs.cloudPass != nullptr ? inputs.cloudPass->GetWeatherMapView() : RI.placeholderData->GetImageView2D1x1R8Array());
            cr << SetShaderUniform(uniformIndex++, "CloudShadowMapTexture"_sh, inputs.cloudPass != nullptr ? inputs.cloudPass->GetShadowMapView() : RI.placeholderData->GetImageView2D1x1R8());

            // placeholders where the far field isn't ready: its zeroed constants keep the shader from sampling them
            const GpuImageViewRef& placeholderView = RI.placeholderData->GetImageView3D1x1x1R8();

            cr << SetShaderUniform(uniformIndex++, "GlimmerSHDataTexture"_sh, hasSHVolume ? inputs.shVolume->GetDataImageView() : placeholderView);
            cr << SetShaderUniform(uniformIndex++, "GlimmerSHStateTexture"_sh, hasSHVolume ? inputs.shVolume->GetStateImageView() : placeholderView);
            cr << SetShaderUniform(uniformIndex++, "GlimmerSHRadianceTexture"_sh, hasSHVolume ? inputs.shVolume->GetRadianceImageView() : placeholderView);

            cr << SetShaderUniform(uniformIndex++, "OutRays"_sh, m_raysBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4f)));
            cr << SetShaderUniform(uniformIndex++, "RayHits"_sh, m_rayHitsBuffer.Get(), ShaderDataOffset(0, RayHitSize));

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
                const Vec3u groups = helpers::WrapComputeGroupCount((extent.x * extent.y + RelightGroupSize - 1) / RelightGroupSize);

                GlimmerProbeTraceConstants relightConstants = constants;
                relightConstants.relightRect = Vec4i(relightDispatch.rect.min.x, relightDispatch.rect.min.y, int32(extent.x), int32(extent.y));
                relightConstants.relightInfo = Vec4u(relightDispatch.level, groups.x, 0, 0);

                bindTraceResources(s_propTraceModeRelight, relightConstants);

                cr << DispatchCompute(groups);

                s_statGlimmerRelightTexels += extent.x * extent.y;
            }

            cr << InsertBarrier(inputs.relight->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);

            inputs.relight->OnDispatched();
        }

        if (inputs.relight)
        {
            constants.relight = inputs.relight->GetShaderData();
        }

        cr << InsertBarrier(m_raysBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_rayHitsBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        for (const StaticShaderPropertyId* modeProperty : { &s_propTraceModeTrace, &s_propTraceModeShade })
        {
            ENGINE_STAT_GPU_SCOPE(modeProperty == &s_propTraceModeTrace ? &s_statGlimmerProbeTrace : &s_statGlimmerProbeShade);

            bindTraceResources(*modeProperty, constants);

            cr << DispatchCompute(Vec3u { (probesPerFrame + ProbesPerTraceGroup - 1) / ProbesPerTraceGroup, 1, 1 });

            // the shade pass reads the trace pass's hits and adds to its rays
            cr << InsertBarrier(m_raysBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
            cr << InsertBarrier(m_rayHitsBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        }
    }

    { // blend
        ENGINE_STAT_GPU_SCOPE(&s_statGlimmerProbeBlend);

        GlimmerProbeBlendConstants constants {};
        constants.volume = m_shaderData;

        const uint32 minHistory = uint32(MathUtil::Clamp(g_cvGlimmerSWRTProbesMinHistory.Get(), 1, 255));

        constants.dispatch = Vec4u(
            probesPerFrame,
            MaxRelocations,
            minHistory,
            uint32(MathUtil::Clamp(g_cvGlimmerSWRTProbesMinHistoryChanging.Get(), 0, int(minHistory))));

        const float historySeconds = MathUtil::Max(g_cvGlimmerSWRTProbesHistorySeconds.Get(), 0.0f);

        constants.params = Vec4f(
            historySeconds,
            MathUtil::Clamp(g_cvGlimmerSWRTProbesHistorySecondsChanging.Get(), 0.0f, historySeconds),
            MathUtil::Max(g_cvGlimmerSWRTProbesRelocateMargin.Get(), 0.0f),
            float(sleepMaxInterval));

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        cr << InsertBarrier(m_raysBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);

        for (const GpuBufferRef* buffer : { &m_shBuffer, &m_statesBuffer, &m_visibilityBuffer, &m_trendBuffer, &m_blockWakeBuffer })
        {
            cr << InsertBarrier(buffer->Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        }

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSWRTProbeBlend")));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "Rays"_sh, m_raysBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4f)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeUpdateListBuffer"_sh, m_updateListBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeCountersBuffer"_sh, m_countersBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSlotsBuffer"_sh, m_slotsBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4i)));
        cr << SetShaderUniform(uniformIndex++, "OutSH"_sh, m_shBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4f)));
        cr << SetShaderUniform(uniformIndex++, "OutStates"_sh, m_statesBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4u)));
        cr << SetShaderUniform(uniformIndex++, "OutVisibility"_sh, m_visibilityBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "OutTrend"_sh, m_trendBuffer.Get(), ShaderDataOffset(0, sizeof(Vec4f)));
        cr << SetShaderUniform(uniformIndex++, "OutBlockWake"_sh, m_blockWakeBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));

        // group per probe
        cr << DispatchCompute(Vec3u { probesPerFrame, 1, 1 });
    }

    for (const GpuBufferRef* buffer : probeStateBuffers)
    {
        cr << InsertBarrier(buffer->Get(), ResourceState::ShaderResource);
    }

    m_shaderData.info.w = 1;
}

#pragma endregion GlimmerSWRTProbeVolume

} // namespace Hyperion
