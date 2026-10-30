/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/SH/GlimmerSHVolume.hpp>
#include <Rendering/Glimmer/SH/GlimmerSHOccupancy.hpp>

#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerRelight.hpp>
#include <Rendering/Glimmer/GlimmerSceneInstances.hpp>
#include <Rendering/Glimmer/GlimmerLocalLights.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Rendering/Glimmer/Probes/GlimmerProbeVolume.hpp>
#include <Rendering/Glimmer/Probes/GlimmerProbeCVars.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Rendering/Clouds/CloudPass.hpp>
#include <Rendering/RenderProxy.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>

#include <Framework/EngineStats.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statGlimmerSHUpdate("Rendering/GPU/Glimmer/SHUpdate");
static EngineStatGpuTimer s_statGlimmerSHCompose("Rendering/GPU/Glimmer/SHCompose");
static EngineStatCounter<uint32> s_statGlimmerSHVoxelsTraced("Rendering/Glimmer/SHVoxelsTraced");

static StaticShaderPropertyId s_propSHModeUpdate { ShaderProperty(NAME("MODE"), NAME("UPDATE")) };
static StaticShaderPropertyId s_propSHModeCompose { ShaderProperty(NAME("MODE"), NAME("COMPOSE")) };
static StaticShaderPropertyId s_propSHProbes { ShaderProperty(NAME("PROBES")) };

static constexpr int32 VoxelsPerFrame = 512;

// lines the windows up with GlimmerSHOccupancy's, which snap to 8 of its half spaced voxels
static constexpr int32 WindowSnapVoxels = 4;

static constexpr size_t MaxPendingBoxes = 16;

static constexpr int32 SolidChangeMarginVoxels = 4;
static constexpr size_t MaxSolidChangeBoxes = 8;

static constexpr int32 RefreshSliceVoxels = int32(GlimmerSHGridXZ * GlimmerSHGridY) / 2;

static constexpr size_t MaxAccumulatingBoxes = 64;

static constexpr uint32 RadianceSections = 3;

// NOTE must match GLIMMER_SH_LIGHT_UINTS in GlimmerSHCommon.hlsli
static constexpr uint32 LightUints = GlimmerLightFullTerms * 3 + GlimmerLightTransferEntries + (GlimmerLightEntries - GlimmerLightTransferEntries) * 2;
static constexpr uint32 RadianceColumns = 3;
static constexpr uint32 RadianceRows = (RadianceSections + RadianceColumns - 1) / RadianceColumns;

static constexpr uint32 MaxOccupancyWaitFrames = 8;

struct GlimmerSHUpdateConstants
{
    GlimmerSHVolumeShaderData volume;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
    GlimmerSHOccupancyShaderData occupancy;

    /// xyz = voxel pos of the dispatch's first voxel
    /// w = cascade
    Vec4i boxMin;

    /// quaternion applied to this update's ray directions
    Vec4f rayRotation;

    /// x = foliage extinction
    /// y = max ray distance
    /// z = albedo where the ground's isn't known
    /// w = how much of the radiance's history is kept
    Vec4f params;
    GlimmerRelightShaderData relight;
    
    /// zeroed while the probes can't be sampled
    GlimmerProbeVolumeShaderData probes;
    GlimmerSkyShaderData sky;
    EnvProbeShaderData skyProbe;
    GlimmerLightShaderData light;

    /// x = 1 to light hits from the nearest voxel rather than the volume's filtered sample
    Vec4u options;

    GlimmerLocalLightsShaderData localLights;
};

namespace /* Helpers */ {

uint32 GetCascadeGroundLevel(uint32 cascadeIndex)
{
    const float spacing = GlimmerHelpers::GetGlimmerSHCascadeSpacing(cascadeIndex);
    const int32 startLevel = MathUtil::Clamp(int32(std::lround(std::log2(MathUtil::Max(spacing / GetGlimmerGroundTexelSize(0), 1.0f)))), 0, int32(GlimmerGroundLevels) - 1);

    return uint32(MathUtil::Max(startLevel - 1, 0));
}

} // namespace

GlimmerSHVolume::Box GlimmerSHVolume::Box::Intersect(const Box& a, const Box& b)
{
    Box result;
    result.min = MathUtil::Max(a.min, b.min);
    result.max = MathUtil::Min(a.max, b.max);

    if (result.IsEmpty())
    {
        result.max = result.min;
    }

    return result;
}

GlimmerSHVolume::Box GlimmerSHVolume::Box::Union(const Box& a, const Box& b)
{
    if (a.IsEmpty())
    {
        return b;
    }

    if (b.IsEmpty())
    {
        return a;
    }

    Box result;
    result.min = MathUtil::Min(a.min, b.min);
    result.max = MathUtil::Max(a.max, b.max);

    return result;
}

#pragma region GlimmerSHVolume

GlimmerSHVolume::GlimmerSHVolume()
    : m_refreshStep(0),
      m_refreshSlices {},
      m_updateIndex(0),
      m_seenInstancesGeneration(~0u),
      m_occupancyWaitFrames(0),
      m_refreshBudget(0),
      m_composedLight {},
      m_hasComposedLight(false),
      m_shaderData {}
{
    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float spacing = GlimmerHelpers::GetGlimmerSHCascadeSpacing(cascadeIndex);

        m_shaderData.cascades[cascadeIndex].params = {
            spacing,
            1.0f / spacing,
            0.0f,
            float(GetCascadeGroundLevel(cascadeIndex))
        };
    }

    m_shaderData.info = Vec4u(GlimmerSHCascades, 0, 0, 0);
}

GlimmerSHVolume::~GlimmerSHVolume()
{
    m_dataTexture.Reset();
    m_stateTexture.Reset();
    m_radianceTexture.Reset();

    m_lightBuffer.Shutdown();
}

void GlimmerSHVolume::CreateResources()
{
    const Vec3u extent = Vec3u(GlimmerSHGridXZ, GlimmerSHGridY * GlimmerSHCascades, GlimmerSHGridXZ);

    m_dataTexture = GlimmerHelpers::CreateGlimmerStorageTexture(
        TextureType::Texture3D,
        TextureFormat::RGBA16F,
        extent,
        1,
        NAME("GlimmerSHData"));

    m_stateTexture = GlimmerHelpers::CreateGlimmerStorageTexture(
        TextureType::Texture3D,
        TextureFormat::RGBA32,
        extent,
        1,
        NAME("GlimmerSHState"));

    m_radianceTexture = GlimmerHelpers::CreateGlimmerStorageTexture(
        TextureType::Texture3D,
        TextureFormat::RGBA16F,
        Vec3u(extent.x * RadianceColumns, extent.y * RadianceRows, extent.z),
        1,
        NAME("GlimmerSHRadiance"));

    m_lightBuffer = GlimmerHelpers::CreateGlimmerRWBuffer(sizeof(uint32), size_t(extent.x) * extent.y * extent.z * LightUints);
}

const GpuImageViewRef& GlimmerSHVolume::GetDataImageView() const
{
    return m_dataTexture.IsValid() ? RI.textureViewCache->GetOrCreate(m_dataTexture) : RI.placeholderData->GetImageView3D1x1x1R8();
}

const GpuImageViewRef& GlimmerSHVolume::GetStateImageView() const
{
    return m_stateTexture.IsValid() ? RI.textureViewCache->GetOrCreate(m_stateTexture) : RI.placeholderData->GetImageView3D1x1x1R8();
}

const GpuImageViewRef& GlimmerSHVolume::GetRadianceImageView() const
{
    return m_radianceTexture.IsValid() ? RI.textureViewCache->GetOrCreate(m_radianceTexture) : RI.placeholderData->GetImageView3D1x1x1R8();
}

GlimmerSHVolume::Box GlimmerSHVolume::GetWindow(const Vec3i& origin)
{
    return Box { origin, origin + Vec3i(int32(GlimmerSHGridXZ), int32(GlimmerSHGridY), int32(GlimmerSHGridXZ)) };
}

void GlimmerSHVolume::MoveWindow(uint32 cascadeIndex, const Vec3i& origin)
{
    Cascade& cascade = m_cascades[cascadeIndex];

    if (cascade.origin.HasValue() && *cascade.origin == origin)
    {
        return;
    }

    const Box newWindow = GetWindow(origin);
    const Vec3i size = newWindow.Extent();

    const bool isJump = !cascade.origin.HasValue()
        || MathUtil::Abs(origin.x - cascade.origin->x) >= size.x
        || MathUtil::Abs(origin.y - cascade.origin->y) >= size.y
        || MathUtil::Abs(origin.z - cascade.origin->z) >= size.z;

    if (isJump)
    {
        cascade.pending.Clear();
        cascade.accumulating.Clear();
        cascade.pending.PushBack(newWindow);
    }
    else
    {
        const Box oldWindow = GetWindow(*cascade.origin);

        for (Box& pendingBox : cascade.pending)
        {
            pendingBox = Box::Intersect(pendingBox, newWindow);
        }

        for (auto& elem : cascade.accumulating)
        {
            Box& box = elem.first;
            box = Box::Intersect(box, newWindow);
        }

        // what scrolled in, as up to three slabs.
        // exposed x over the whole window, then exposed y and z over what's left
        const Box kept = Box::Intersect(oldWindow, newWindow);

        Box remaining = newWindow;

        for (int32 axis = 0; axis < 3; axis++)
        {
            Box slab = remaining;

            if (origin[axis] > (*cascade.origin)[axis])
            {
                slab.min[axis] = MathUtil::Max(kept.max[axis], newWindow.min[axis]);
            }
            else
            {
                slab.max[axis] = MathUtil::Min(kept.min[axis], newWindow.max[axis]);
            }

            if (!slab.IsEmpty())
            {
                cascade.pending.PushBack(slab);
            }

            remaining.min[axis] = kept.min[axis];
            remaining.max[axis] = kept.max[axis];
        }
    }

    cascade.origin = origin;

    m_shaderData.cascades[cascadeIndex].origin = Vec4i(origin.x, origin.y, origin.z, 1);
}

void GlimmerSHVolume::AddPending(uint32 cascadeIndex, const Box& box)
{
    Cascade& cascade = m_cascades[cascadeIndex];

    if (!cascade.origin.HasValue())
    {
        return;
    }

    const Box window = GetWindow(*cascade.origin);
    const Box clipped = Box::Intersect(box, window);

    if (clipped.IsEmpty())
    {
        return;
    }

    if (cascade.pending.Size() >= MaxPendingBoxes)
    {
        // joins the pending box it adds the fewest voxels to
        size_t bestIndex = 0;
        int64 bestGrowth = INT64_MAX;

        for (size_t pendingIndex = 0; pendingIndex < cascade.pending.Size(); pendingIndex++)
        {
            const Box& pendingBox = cascade.pending[pendingIndex];
            const int64 growth = Box::Union(pendingBox, clipped).Volume() - pendingBox.Volume();

            if (growth < bestGrowth)
            {
                bestGrowth = growth;
                bestIndex = pendingIndex;
            }
        }

        cascade.pending[bestIndex] = Box::Union(cascade.pending[bestIndex], clipped);

        return;
    }

    cascade.pending.PushBack(clipped);
}

void GlimmerSHVolume::AddPendingWorld(const Vec3f& worldMin, const Vec3f& worldMax, int32 marginVoxels, uint32 numCascades)
{
    for (uint32 cascadeIndex = 0; cascadeIndex < numCascades; cascadeIndex++)
    {
        const float invSpacing = 1.0f / GlimmerHelpers::GetGlimmerSHCascadeSpacing(cascadeIndex);

        AddPending(
            cascadeIndex,
            Box {
                Vec3i(
                    int32(MathUtil::Floor(worldMin.x * invSpacing)) - marginVoxels,
                    int32(MathUtil::Floor(worldMin.y * invSpacing)) - marginVoxels,
                    int32(MathUtil::Floor(worldMin.z * invSpacing)) - marginVoxels),
                Vec3i(
                    int32(MathUtil::Floor(worldMax.x * invSpacing)) + 1 + marginVoxels,
                    int32(MathUtil::Floor(worldMax.y * invSpacing)) + 1 + marginVoxels,
                    int32(MathUtil::Floor(worldMax.z * invSpacing)) + 1 + marginVoxels)
            });
    }
}

void GlimmerSHVolume::QueuePasses(uint32 cascadeIndex, const Box& box)
{
    const int32 passes = g_cvGlimmerSHPasses.Get() - 1;

    if (passes <= 0 || box.IsEmpty())
    {
        return;
    }

    Cascade& cascade = m_cascades[cascadeIndex];

    if (cascade.accumulating.Size() >= MaxAccumulatingBoxes)
    {
        cascade.accumulating.PopFront();
    }

    cascade.accumulating.EmplaceBack(box, passes);
}

void GlimmerSHVolume::DispatchBox(Frame* frame, uint32 cascadeIndex, const Box& box, float radianceHysteresis, const GlimmerSHVolumeUpdateInputs& inputs, bool& inOutHasBarriers)
{
    CommandRecorder& cr = frame->cr;

    const GlimmerProbeVolume* probeVolume = inputs.probeVolume;
    const bool isSamplingProbes = probeVolume && probeVolume->IsReady() && g_cvGlimmerProbesEnabled.Get();

    if (!inOutHasBarriers)
    {
        inOutHasBarriers = true;

        cr << InsertBarrier(m_dataTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_stateTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_radianceTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_lightBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        ShaderPropertySet shaderProperties;
        shaderProperties.Add(s_propSHModeUpdate);

        if (isSamplingProbes)
        {
            shaderProperties.Add(s_propSHProbes);
        }

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSHUpdate"), shaderProperties));
    }

    GlimmerSHUpdateConstants constants {};
    constants.volume = m_shaderData;
    constants.ground = inputs.surfaceCache->GetGroundShaderData();
    constants.spans = inputs.spanCache->GetShaderData();
    constants.occupancy = *inputs.occupancy;
    constants.boxMin = Vec4i(box.min.x, box.min.y, box.min.z, int32(cascadeIndex));
    constants.rayRotation = GlimmerHelpers::MakeGlimmerRandomRotation(m_updateIndex++);
    constants.params = Vec4f(
        GlimmerHelpers::GetGlimmerFoliageExtinction(),
        GlimmerMaxRayDistance,
        GlimmerDefaultGroundAlbedo,
        radianceHysteresis);

    if (inputs.relight && inputs.relightImageView.IsValid())
    {
        constants.relight = *inputs.relight;
    }

    if (isSamplingProbes)
    {
        constants.probes = probeVolume->GetShaderData();
    }

    GlimmerHelpers::GetGlimmerSkyShaderData(inputs.skyProbe, constants.sky, constants.skyProbe);

    constants.light = inputs.light;
    constants.options = Vec4u(g_cvGlimmerSHNearestHitLight.Get() ? 1u : 0u, 0u, 0u, 0u);

    if (inputs.localLights)
    {
        constants.localLights = inputs.localLights->GetShaderData();
    }

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);

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

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, inputs.surfaceCache->GetGroundImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, inputs.surfaceCache->GetGroundAlbedoImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, inputs.spanCache->GetSpansBuffer());
    cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, inputs.spanCache->GetHeightBoundsBuffer());
    cr << SetShaderUniform(uniformIndex++, "OutData"_sh, RI.textureViewCache->GetOrCreate(m_dataTexture));
    cr << SetShaderUniform(uniformIndex++, "OutState"_sh, RI.textureViewCache->GetOrCreate(m_stateTexture));
    cr << SetShaderUniform(uniformIndex++, "OutRadiance"_sh, RI.textureViewCache->GetOrCreate(m_radianceTexture));
    cr << SetShaderUniform(uniformIndex++, "OutLight"_sh, m_lightBuffer);
    cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(uniformIndex++, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
    cr << SetShaderUniform(uniformIndex++, "CloudWeatherMapTexture"_sh, inputs.cloudPass != nullptr ? inputs.cloudPass->GetWeatherMapView() : RI.placeholderData->GetImageView2D1x1R8Array());
    cr << SetShaderUniform(uniformIndex++, "CloudShadowMapTexture"_sh, inputs.cloudPass != nullptr ? inputs.cloudPass->GetShadowMapView() : RI.placeholderData->GetImageView2D1x1R8());

    if (probeVolume)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBlockTableBuffer"_sh, probeVolume->GetBlockTableBuffer());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeLightBuffer"_sh, probeVolume->GetLightBuffer());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStatesBuffer"_sh, probeVolume->GetStatesBuffer());
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeVisibilityBuffer"_sh, probeVolume->GetVisibilityBuffer());
    }
    else
    {
        // HACK: any structured buffer will do, as the zeroed constants keep the probes from being read
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBlockTableBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeLightBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStatesBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeVisibilityBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    }
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyTexture"_sh, inputs.occupancyImageView);
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyMaskBuffer"_sh, *inputs.occupancyMaskBuffer);

    if (inputs.relightImageView.IsValid())
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerRelightTexture"_sh, inputs.relightImageView);
    }

    const Vec3i extent = box.Extent();

    cr << DispatchCompute(Vec3u { uint32(extent.x), uint32(extent.y), uint32(extent.z) });

    s_statGlimmerSHVoxelsTraced += uint32(extent.x * extent.y * extent.z);
}

bool GlimmerSHVolume::DispatchRefreshSlice(
    Frame* frame,
    uint32 cascadeIndex,
    int32& inOutSlice,
    float radianceHysteresis,
    bool& outFinishedCascade,
    const GlimmerSHVolumeUpdateInputs& inputs,
    bool& inOutHasBarriers)
{
    outFinishedCascade = false;

    Cascade& cascade = m_cascades[cascadeIndex];

    if (!cascade.origin.HasValue() || cascade.pending.Any())
    {
        return false;
    }

    const Box window = GetWindow(*cascade.origin);

    const int32 firstSlice = window.min.z * 2;
    const int32 endSlice = window.max.z * 2;

    // slices that scrolled out are skipped; the ones that scrolled in were traced as they did
    const int32 sliceIndex = MathUtil::Max(inOutSlice, firstSlice);

    // the rest of the pass scrolled out
    if (sliceIndex >= endSlice)
    {
        inOutSlice = firstSlice;
        outFinishedCascade = true;

        return false;
    }

    Box slice = window;
    slice.min.z = sliceIndex >> 1;
    slice.max.z = slice.min.z + 1;

    if ((sliceIndex & 1) == 0)
    {
        slice.max.y = slice.min.y + int32(GlimmerSHGridY / 2);
    }
    else
    {
        slice.min.y += int32(GlimmerSHGridY / 2);
    }

    DispatchBox(frame, cascadeIndex, slice, radianceHysteresis, inputs, inOutHasBarriers);

    inOutSlice = sliceIndex + 1;

    if (inOutSlice >= endSlice)
    {
        inOutSlice = firstSlice;
        outFinishedCascade = true;
    }

    return true;
}

void GlimmerSHVolume::Compose(Frame* frame, const GlimmerLightShaderData& light)
{
    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSHCompose);

    CommandRecorder& cr = frame->cr;

    GlimmerSHUpdateConstants constants {};
    constants.light = light;

    static const EffectVolumeShaderData s_noCloudVolume {};
    static const CloudWeatherMapShaderData s_noWeatherMap {};
    static const CloudShadowMapShaderData s_noShadowMap {};

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Write(&s_noCloudVolume);
    RI.cbufferAllocator->Write(&s_noWeatherMap);
    RI.cbufferAllocator->Write(&s_noShadowMap);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    cr << InsertBarrier(m_radianceTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    cr << InsertBarrier(m_lightBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    ShaderPropertySet shaderProperties;
    shaderProperties.Add(s_propSHModeCompose);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSHUpdate"), shaderProperties));

    cr << SetShaderUniform(0, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(1, "OutRadiance"_sh, RI.textureViewCache->GetOrCreate(m_radianceTexture));
    cr << SetShaderUniform(2, "OutLight"_sh, m_lightBuffer);

    // 2 rows of voxels a group
    cr << DispatchCompute(Vec3u { 1, GlimmerSHGridY * GlimmerSHCascades / 2, GlimmerSHGridXZ });

    cr << InsertBarrier(m_radianceTexture->GetGpuImage(), ResourceState::ShaderResource);
    cr << InsertBarrier(m_lightBuffer, ResourceState::ShaderResource);

    m_composedLight = light;
    m_hasComposedLight = true;
}

void GlimmerSHVolume::Update(Frame* frame, const GlimmerSHVolumeUpdateInputs& inputs)
{
    HYP_SCOPE;

    if (!inputs.surfaceCache || !inputs.spanCache || !inputs.spanCache->GetSpansBuffer().IsInitialized() || !inputs.occupancy || !inputs.occupancyMaskBuffer || !inputs.occupancyMaskBuffer->IsInitialized())
    {
        return;
    }

    if (!m_dataTexture.IsValid())
    {
        CreateResources();
    }

    // voxels that aren't traced this frame still hold the light as it was put together before
    if (m_shaderData.info.w != 0 && (!m_hasComposedLight || memcmp(&m_composedLight, &inputs.light, sizeof(GlimmerLightShaderData)) != 0))
    {
        Compose(frame, inputs.light);
    }

    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float spacing = GlimmerHelpers::GetGlimmerSHCascadeSpacing(cascadeIndex);

        m_shaderData.cascades[cascadeIndex].params.z = MathUtil::Clamp(g_cvGlimmerVisibility.Get(), 0.0f, 1.0f);

        // must lean exactly as GlimmerSHOccupancy's windows do
        const Vec3f center = GlimmerHelpers::GetGlimmerWindowCenter(
            inputs.viewerPosition,
            inputs.lookAhead,
            Vec3f(float(GlimmerSHGridXZ), float(GlimmerSHGridY), float(GlimmerSHGridXZ)) * spacing);

        const Vec3i origin = Vec3i(
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(center.x / spacing)) - int32(GlimmerSHGridXZ / 2) + WindowSnapVoxels / 2, WindowSnapVoxels),
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(center.y / spacing)) - int32(GlimmerSHGridY / 2) + WindowSnapVoxels / 2, WindowSnapVoxels),
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(center.z / spacing)) - int32(GlimmerSHGridXZ / 2) + WindowSnapVoxels / 2, WindowSnapVoxels));

        m_shaderData.cascades[cascadeIndex].center = Vec4f(center / spacing, 0.0f);

        MoveWindow(cascadeIndex, origin);
    }

    if (inputs.sceneInstances && inputs.sceneInstances->IsReady() && inputs.sceneInstances->GetGeneration() != m_seenInstancesGeneration)
    {
        GlimmerSceneChanges changes;

        const bool isEverywhere = m_seenInstancesGeneration == ~0u || !inputs.sceneInstances->GetChangesSince(m_seenInstancesGeneration, changes);

        if (isEverywhere)
        {
            for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
            {
                AddPending(cascadeIndex, GetWindow(*m_cascades[cascadeIndex].origin));
            }
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

            changes.Quantize(MaxSolidChangeBoxes);

            for (const GlimmerSceneChange& change : changes)
            {
                AddPendingWorld(change.bounds.min, change.bounds.max, SolidChangeMarginVoxels);
            }
        }

        m_seenInstancesGeneration = inputs.sceneInstances->GetGeneration();
    }

    if (inputs.localLights && inputs.localLights->GetGeneration() != m_seenLightsGeneration)
    {
        GlimmerSceneChanges changes;

        if (inputs.localLights->GetChangesSince(m_seenLightsGeneration, changes))
        {
            changes.Quantize(MaxSolidChangeBoxes);

            for (const GlimmerSceneChange& change : changes)
            {
                AddPendingWorld(change.bounds.min, change.bounds.max, SolidChangeMarginVoxels);
            }
        }
        else
        {
            for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
            {
                AddPending(cascadeIndex, GetWindow(*m_cascades[cascadeIndex].origin));
            }
        }

        m_seenLightsGeneration = inputs.localLights->GetGeneration();
    }

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        const GlimmerTexelRect& uploadedRect = inputs.surfaceCache->GetUploadedRect(levelIndex);

        if (uploadedRect.IsEmpty())
        {
            continue;
        }

        const float texelSize = GetGlimmerGroundTexelSize(levelIndex);

        uint32 numCascades = 0;
        while (numCascades < GlimmerSHCascades && GetCascadeGroundLevel(numCascades) <= levelIndex)
        {
            numCascades++;
        }

        AddPendingWorld(
            Vec3f(float(uploadedRect.min.x) * texelSize, -1e6f, float(uploadedRect.min.y) * texelSize),
            Vec3f(float(uploadedRect.max.x) * texelSize, 1e6f, float(uploadedRect.max.y) * texelSize),
            1,
            numCascades);
    }

    // a viewer outrunning the occupancy's one rebuild a frame can't stall the far field for good
    if (inputs.isOccupancySettled)
    {
        m_occupancyWaitFrames = 0;
    }
    else if (m_occupancyWaitFrames < MaxOccupancyWaitFrames)
    {
        m_occupancyWaitFrames++;

        return;
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSHUpdate);

    // counted per frame, so a low frame rate gets more a frame to keep the same pace
    int32 budget = int32(float(VoxelsPerFrame) * MathUtil::Clamp(inputs.budgetScale, 1.0f, 4.0f));
    bool hasBarriers = false;

    const float hysteresis = MathUtil::Clamp(g_cvGlimmerSHHysteresis.Get(), 0.0f, 0.99f);

    const auto spend = [&](uint32 cascadeIndex, int32 allowance)
    {
        Cascade& cascade = m_cascades[cascadeIndex];

        while (cascade.pending.Any() && allowance > 0 && budget > 0)
        {
            Box& box = cascade.pending.Front();

            if (box.IsEmpty())
            {
                cascade.pending.PopFront();

                continue;
            }

            const Vec3i extent = box.Extent();

            // slice across the thinnest cross section so a small allowance still makes progress without overshooting
            const int32 sliceSizes[3] = { extent.y * extent.z, extent.x * extent.z, extent.x * extent.y };
            const uint32 axis = sliceSizes[0] <= sliceSizes[1] && sliceSizes[0] <= sliceSizes[2] ? 0u : (sliceSizes[1] <= sliceSizes[2] ? 1u : 2u);

            const int32 sliceSize = MathUtil::Max(sliceSizes[axis], 1);
            const int32 slices = MathUtil::Clamp(MathUtil::Min(allowance, budget) / sliceSize, 1, extent[axis]);

            Box chunk = box;
            chunk.max[axis] = chunk.min[axis] + slices;

            DispatchBox(frame, cascadeIndex, chunk, hysteresis, inputs, hasBarriers);
            QueuePasses(cascadeIndex, chunk);

            const int32 spent = sliceSize * slices;

            budget -= spent;
            allowance -= spent;
            box.min[axis] += slices;
        }
    };

    // every cascade gets a share first, so a moving camera that keeps the finest one busy can't starve the far field ahead of it
    const int32 share = budget / int32(GlimmerSHCascades * 2);

    for (uint32 cascadeIndex = 1; cascadeIndex < GlimmerSHCascades && budget > 0; cascadeIndex++)
    {
        spend(cascadeIndex, share);
    }

    // then finest first: it's what the camera sits in
    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades && budget > 0; cascadeIndex++)
    {
        spend(cascadeIndex, budget);
    }

    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades && budget > 0; cascadeIndex++)
    {
        Cascade& cascade = m_cascades[cascadeIndex];

        const size_t numQueued = cascade.accumulating.Size();

        for (size_t queuedIndex = 0; queuedIndex < numQueued && budget > 0; queuedIndex++)
        {
            auto accumulatingBox = cascade.accumulating.PopFront();

            const Box& box = accumulatingBox.first;
            int32& passesRemaining = accumulatingBox.second;

            if (box.IsEmpty())
            {
                continue;
            }

            DispatchBox(frame, cascadeIndex, box, hysteresis, inputs, hasBarriers);

            budget -= int32(box.Volume());

            if (--passesRemaining > 0)
            {
                cascade.accumulating.PushBack(std::move(accumulatingBox));
            }
        }
    }

    const int32 refreshVoxels = MathUtil::Max(g_cvGlimmerSHRefreshVoxelsPerFrame.Get(), 0);

    m_refreshBudget = MathUtil::Min(m_refreshBudget + refreshVoxels, MathUtil::Max(refreshVoxels, RefreshSliceVoxels) * 2);

    for (uint32 attempt = 0; attempt < 2 * GlimmerSHCascades && m_refreshBudget >= RefreshSliceVoxels; attempt++)
    {
        uint32 ruler = ++m_refreshStep;
        uint32 cascadeIndex = 0;

        while ((ruler & 1u) == 0u && cascadeIndex + 1 < GlimmerSHCascades)
        {
            ruler >>= 1;
            cascadeIndex++;
        }

        bool hasFinishedCascade = false;

        if (!DispatchRefreshSlice(frame, cascadeIndex, m_refreshSlices[cascadeIndex], hysteresis, hasFinishedCascade, inputs, hasBarriers))
        {
            continue;
        }

        m_refreshBudget -= RefreshSliceVoxels;

        attempt = 0;
    }

    if (hasBarriers)
    {
        CommandRecorder& cr = frame->cr;

        cr << InsertBarrier(m_dataTexture->GetGpuImage(), ResourceState::ShaderResource);
        cr << InsertBarrier(m_stateTexture->GetGpuImage(), ResourceState::ShaderResource);
        cr << InsertBarrier(m_radianceTexture->GetGpuImage(), ResourceState::ShaderResource);
        cr << InsertBarrier(m_lightBuffer, ResourceState::ShaderResource);

        m_shaderData.info.w = 1;
    }
}

#pragma endregion GlimmerSHVolume

} // namespace Hyperion
