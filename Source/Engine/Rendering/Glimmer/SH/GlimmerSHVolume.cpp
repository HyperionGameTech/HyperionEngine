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
#include <Rendering/Glimmer/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTProbeVolume.hpp>
#include <Rendering/Glimmer/SWRT/GlimmerSWRTCVars.hpp>

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
static EngineStatCounter<uint32> s_statGlimmerSHVoxelsTraced("Rendering/Glimmer/SHVoxelsTraced");

static constexpr int32 VoxelsPerFrame = 512;
static constexpr int32 RefreshVoxelsPerFrame = 256;

// lines the windows up with GlimmerSHOccupancy's, which snap to 8 of its half spaced voxels
static constexpr int32 WindowSnapVoxels = 4;

static constexpr size_t MaxPendingBoxes = 16;

static constexpr int32 SolidChangeMarginVoxels = 4;
static constexpr uint32 MaxOccupancyWaitFrames = 8;

struct GlimmerSHUpdateConstants
{
    GlimmerSHVolumeShaderData volume;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
    GlimmerSHOccupancyShaderData occupancy;
    Vec4i boxMin;      // xyz = absolute voxel of the dispatch's first voxel, w = cascade
    Vec4f rayRotation; // quaternion applied to this update's ray directions
    Vec4f params;      // x = foliage extinction, y = max ray distance, z = albedo where the ground's isn't known
    GlimmerRelightShaderData relight;
    GlimmerProbeVolumeShaderData probes; // zeroed while the probes can't be sampled
    GlimmerSkyShaderData sky;
    EnvProbeShaderData skyProbe;
};

GlimmerSHVolume::Box GlimmerSHVolume::Box::Intersect(const Box& a, const Box& b)
{
    Box result;
    result.min = Vec3i(MathUtil::Max(a.min.x, b.min.x), MathUtil::Max(a.min.y, b.min.y), MathUtil::Max(a.min.z, b.min.z));
    result.max = Vec3i(MathUtil::Min(a.max.x, b.max.x), MathUtil::Min(a.max.y, b.max.y), MathUtil::Min(a.max.z, b.max.z));

    if (result.IsEmpty())
    {
        result.max = result.min;
    }

    return result;
}

#pragma region GlimmerSHVolume

GlimmerSHVolume::GlimmerSHVolume()
    : m_refreshStep(0),
      m_refreshSlices {},
      m_updateIndex(0),
      m_seenTLASGeneration(~0u),
      m_occupancyWaitFrames(0),
      m_shaderData {}
{
    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float spacing = GetGlimmerSHCascadeSpacing(cascadeIndex);

        m_shaderData.cascades[cascadeIndex].params = Vec4f(spacing, 1.0f / spacing, 0.0f, 0.0f);
    }

    m_shaderData.info = Vec4u(GlimmerSHCascades, 0, 0, 0);
}

GlimmerSHVolume::~GlimmerSHVolume()
{
    m_dataTexture = Handle<Texture>();
    m_stateTexture = Handle<Texture>();
    m_radianceTexture = Handle<Texture>();
}

void GlimmerSHVolume::CreateResources()
{
    const Vec3u extent = Vec3u(GlimmerSHGridXZ, GlimmerSHGridY * GlimmerSHCascades, GlimmerSHGridXZ);

    m_dataTexture = CreateGlimmerStorageTexture(TextureType::Texture3D, TextureFormat::RGBA16F, extent, 1, NAME("GlimmerSHData"));
    m_stateTexture = CreateGlimmerStorageTexture(TextureType::Texture3D, TextureFormat::RGBA32, extent, 1, NAME("GlimmerSHState"));
    m_radianceTexture = CreateGlimmerStorageTexture(TextureType::Texture3D, TextureFormat::RGBA16F, Vec3u(extent.x, extent.y * 3, extent.z), 1, NAME("GlimmerSHRadiance"));
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

    if (cascade.hasOrigin && cascade.origin == origin)
    {
        return;
    }

    const Box newWindow = GetWindow(origin);
    const Vec3i size = newWindow.Extent();

    const bool isJump = !cascade.hasOrigin
        || MathUtil::Abs(origin.x - cascade.origin.x) >= size.x
        || MathUtil::Abs(origin.y - cascade.origin.y) >= size.y
        || MathUtil::Abs(origin.z - cascade.origin.z) >= size.z;

    if (isJump)
    {
        cascade.pending.Clear();
        cascade.pending.PushBack(newWindow);
    }
    else
    {
        const Box oldWindow = GetWindow(cascade.origin);

        for (Box& pendingBox : cascade.pending)
        {
            pendingBox = Box::Intersect(pendingBox, newWindow);
        }

        // what scrolled in, as up to three slabs: exposed x over the whole window, then exposed y and z over what's left
        const Box kept = Box::Intersect(oldWindow, newWindow);

        Box remaining = newWindow;

        for (int32 axis = 0; axis < 3; axis++)
        {
            Box slab = remaining;

            if (origin[axis] > cascade.origin[axis])
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
    cascade.hasOrigin = true;

    m_shaderData.cascades[cascadeIndex].origin = Vec4i(origin.x, origin.y, origin.z, 1);
}

void GlimmerSHVolume::AddPending(uint32 cascadeIndex, const Box& box)
{
    Cascade& cascade = m_cascades[cascadeIndex];

    if (!cascade.hasOrigin)
    {
        return;
    }

    const Box window = GetWindow(cascade.origin);
    const Box clipped = Box::Intersect(box, window);

    if (clipped.IsEmpty())
    {
        return;
    }

    if (cascade.pending.Size() >= MaxPendingBoxes)
    {
        cascade.pending.Clear();
        cascade.pending.PushBack(window);

        return;
    }

    cascade.pending.PushBack(clipped);
}

void GlimmerSHVolume::AddPendingWorld(const Vec3f& worldMin, const Vec3f& worldMax, int32 marginVoxels)
{
    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float invSpacing = 1.0f / GetGlimmerSHCascadeSpacing(cascadeIndex);

        AddPending(cascadeIndex, Box {
            Vec3i(
                int32(MathUtil::Floor(worldMin.x * invSpacing)) - marginVoxels,
                int32(MathUtil::Floor(worldMin.y * invSpacing)) - marginVoxels,
                int32(MathUtil::Floor(worldMin.z * invSpacing)) - marginVoxels),
            Vec3i(
                int32(MathUtil::Floor(worldMax.x * invSpacing)) + 1 + marginVoxels,
                int32(MathUtil::Floor(worldMax.y * invSpacing)) + 1 + marginVoxels,
                int32(MathUtil::Floor(worldMax.z * invSpacing)) + 1 + marginVoxels) });
    }
}

void GlimmerSHVolume::DispatchBox(Frame* frame, uint32 cascadeIndex, const Box& box, const GlimmerSHVolumeUpdateInputs& inputs, bool& inOutHasBarriers)
{
    CommandRecorder& cr = frame->cr;

    if (!inOutHasBarriers)
    {
        inOutHasBarriers = true;

        cr << InsertBarrier(m_dataTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_stateTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_radianceTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSHUpdate")));
    }

    GlimmerSHUpdateConstants constants {};
    constants.volume = m_shaderData;
    constants.ground = inputs.surfaceCache->GetGroundShaderData();
    constants.spans = inputs.spanCache->GetShaderData();
    constants.occupancy = *inputs.occupancy;
    constants.boxMin = Vec4i(box.min.x, box.min.y, box.min.z, int32(cascadeIndex));
    constants.rayRotation = MakeGlimmerRandomRotation(m_updateIndex++);
    constants.params = Vec4f(
        GetGlimmerFoliageExtinction(),
        GlimmerMaxRayDistance,
        MathUtil::Clamp(g_cvGlimmerGroundAlbedo.Get(), 0.0f, 1.0f),
        0.0f);

    if (inputs.relight && inputs.relightImageView.IsValid())
    {
        constants.relight = *inputs.relight;
    }

    const GlimmerSWRTProbeVolume* probeVolume = inputs.probeVolume;

    if (probeVolume && probeVolume->IsReady() && g_cvGlimmerSWRTProbesEnabled.Get())
    {
        constants.probes = probeVolume->GetShaderData();
    }

    GetGlimmerSkyShaderData(inputs.skyProbe, constants.sky, constants.skyProbe);

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
    cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, inputs.spanCache->GetSpansBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "GlimmerHeightBoundsBuffer"_sh, inputs.spanCache->GetHeightBoundsBuffer().Get(), ShaderDataOffset(0, sizeof(float)));
    cr << SetShaderUniform(uniformIndex++, "OutData"_sh, RI.textureViewCache->GetOrCreate(m_dataTexture));
    cr << SetShaderUniform(uniformIndex++, "OutState"_sh, RI.textureViewCache->GetOrCreate(m_stateTexture));
    cr << SetShaderUniform(uniformIndex++, "OutRadiance"_sh, RI.textureViewCache->GetOrCreate(m_radianceTexture));
    cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(uniformIndex++, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
    cr << SetShaderUniform(uniformIndex++, "CloudWeatherMapTexture"_sh, inputs.cloudPass != nullptr ? inputs.cloudPass->GetWeatherMapView() : RI.placeholderData->GetImageView2D1x1R8Array());
    cr << SetShaderUniform(uniformIndex++, "CloudShadowMapTexture"_sh, inputs.cloudPass != nullptr ? inputs.cloudPass->GetShadowMapView() : RI.placeholderData->GetImageView2D1x1R8());

    if (probeVolume)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBlockTableBuffer"_sh, probeVolume->GetBlockTableBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSHBuffer"_sh, probeVolume->GetSHBuffer().Get(), ShaderDataOffset(0, sizeof(Vec4f)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStatesBuffer"_sh, probeVolume->GetStatesBuffer().Get(), ShaderDataOffset(0, sizeof(Vec4u)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeVisibilityBuffer"_sh, probeVolume->GetVisibilityBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    }
    else
    {
        // HACK: any structured buffer will do, as the zeroed constants keep the probes from being read
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeBlockTableBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeSHBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeStatesBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "GlimmerProbeVisibilityBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    }
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyTexture"_sh, inputs.occupancyImageView);

    if (inputs.relightImageView.IsValid())
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerRelightTexture"_sh, inputs.relightImageView);
    }

    const Vec3i extent = box.Extent();

    cr << DispatchCompute(Vec3u { uint32(extent.x), uint32(extent.y), uint32(extent.z) });

    s_statGlimmerSHVoxelsTraced += uint32(extent.x * extent.y * extent.z);
}

void GlimmerSHVolume::Update(Frame* frame, const GlimmerSHVolumeUpdateInputs& inputs)
{
    HYP_SCOPE;

    if (!inputs.surfaceCache || !inputs.spanCache || !inputs.spanCache->GetSpansBuffer().IsValid() || !inputs.occupancy)
    {
        return;
    }

    if (!m_dataTexture.IsValid())
    {
        CreateResources();
    }

    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float spacing = GetGlimmerSHCascadeSpacing(cascadeIndex);

        m_shaderData.cascades[cascadeIndex].params.z = MathUtil::Clamp(g_cvGlimmerVisibility.Get(), 0.0f, 1.0f);

        const Vec3i origin = Vec3i(
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(inputs.viewerPosition.x / spacing)) - int32(GlimmerSHGridXZ / 2), WindowSnapVoxels),
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(inputs.viewerPosition.y / spacing)) - int32(GlimmerSHGridY / 2), WindowSnapVoxels),
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(inputs.viewerPosition.z / spacing)) - int32(GlimmerSHGridXZ / 2), WindowSnapVoxels));

        MoveWindow(cascadeIndex, origin);
    }

    if (inputs.tlas && inputs.tlas->IsReady() && inputs.tlas->GetGeneration() != m_seenTLASGeneration)
    {
        const bool isEverywhere = m_seenTLASGeneration == ~0u
            || inputs.tlas->GetGeneration() != m_seenTLASGeneration + 1
            || inputs.tlas->IsSpanFullyDirty();

        if (isEverywhere)
        {
            for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
            {
                AddPending(cascadeIndex, GetWindow(m_cascades[cascadeIndex].origin));
            }
        }
        else
        {
            for (const BoundingBox& bounds : inputs.tlas->GetSpanDirtyBounds())
            {
                AddPendingWorld(bounds.min, bounds.max, SolidChangeMarginVoxels);
            }
        }

        m_seenTLASGeneration = inputs.tlas->GetGeneration();
    }

    for (uint32 levelIndex = 0; levelIndex < GlimmerGroundLevels; levelIndex++)
    {
        const GlimmerTexelRect& uploadedRect = inputs.surfaceCache->GetUploadedRect(levelIndex);

        if (uploadedRect.IsEmpty())
        {
            continue;
        }

        const float texelSize = GetGlimmerGroundTexelSize(levelIndex);

        AddPendingWorld(
            Vec3f(float(uploadedRect.min.x) * texelSize, -1e6f, float(uploadedRect.min.y) * texelSize),
            Vec3f(float(uploadedRect.max.x) * texelSize, 1e6f, float(uploadedRect.max.y) * texelSize),
            1);
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

    int32 budget = VoxelsPerFrame;
    bool hasBarriers = false;

    // finest first: it's what the camera sits in
    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades && budget > 0; cascadeIndex++)
    {
        Cascade& cascade = m_cascades[cascadeIndex];

        while (cascade.pending.Any() && budget > 0)
        {
            Box& box = cascade.pending.Front();

            if (box.IsEmpty())
            {
                cascade.pending.PopFront();

                continue;
            }

            const Vec3i extent = box.Extent();
            const int32 slices = MathUtil::Clamp(budget / MathUtil::Max(extent.x * extent.y, 1), 1, extent.z);

            Box chunk = box;
            chunk.max.z = chunk.min.z + slices;

            DispatchBox(frame, cascadeIndex, chunk, inputs, hasBarriers);

            budget -= extent.x * extent.y * slices;
            box.min.z += slices;
        }
    }

    const int32 sliceVoxels = int32(GlimmerSHGridXZ * GlimmerSHGridY) / 2;

    budget = MathUtil::Min(budget, RefreshVoxelsPerFrame);

    for (uint32 attempt = 0; attempt < 2 * GlimmerSHCascades && budget >= sliceVoxels; attempt++)
    {
        uint32 ruler = ++m_refreshStep;
        uint32 cascadeIndex = 0;

        while ((ruler & 1u) == 0u && cascadeIndex + 1 < GlimmerSHCascades)
        {
            ruler >>= 1;
            cascadeIndex++;
        }

        Cascade& cascade = m_cascades[cascadeIndex];

        if (!cascade.hasOrigin || cascade.pending.Any())
        {
            continue;
        }

        int32& refreshSlice = m_refreshSlices[cascadeIndex];

        Box slice = GetWindow(cascade.origin);
        slice.min.z += refreshSlice / 2;
        slice.max.z = slice.min.z + 1;

        if (refreshSlice % 2 == 0)
        {
            slice.max.y = slice.min.y + int32(GlimmerSHGridY / 2);
        }
        else
        {
            slice.min.y += int32(GlimmerSHGridY / 2);
        }

        DispatchBox(frame, cascadeIndex, slice, inputs, hasBarriers);

        budget -= sliceVoxels;

        refreshSlice = (refreshSlice + 1) % int32(GlimmerSHGridXZ * 2);

        attempt = 0;
    }

    if (hasBarriers)
    {
        CommandRecorder& cr = frame->cr;

        cr << InsertBarrier(m_dataTexture->GetGpuImage(), ResourceState::ShaderResource);
        cr << InsertBarrier(m_stateTexture->GetGpuImage(), ResourceState::ShaderResource);
        cr << InsertBarrier(m_radianceTexture->GetGpuImage(), ResourceState::ShaderResource);

        m_shaderData.info.w = 1;
    }
}

#pragma endregion GlimmerSHVolume

} // namespace Hyperion
