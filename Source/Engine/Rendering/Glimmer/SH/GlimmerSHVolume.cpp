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
#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/Glimmer/GlimmerMath.hpp>

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

// voxels traced per frame; scrolled in voxels come first, the rest of the budget refreshes a slice at a time
static constexpr int32 VoxelsPerFrame = 1024;

// rays past this are taken to have escaped, which only the coarsest ground levels could stop anyway
static constexpr float MaxRayDistance = 1024.0f;

// physical extinction for randomly oriented leaves, less the gaps between crowns (see Rendering.Glimmer.SWRT.FoliageClumping)
static constexpr float FoliageExtinction = 0.7f;

// Must match GlimmerSHUpdateConstants in Shaders/Glimmer/SH/GlimmerSHUpdate.hlsl
struct GlimmerSHUpdateConstants
{
    GlimmerSHVolumeShaderData volume;
    GlimmerGroundShaderData ground;
    GlimmerSpanShaderData spans;
    GlimmerSHOccupancyShaderData occupancy;
    Vec4i boxMin;      // xyz = absolute voxel of the dispatch's first voxel, w = cascade
    Vec4f rayRotation; // quaternion applied to this update's ray directions
    Vec4f params;      // x = foliage extinction, y = max ray distance, z = albedo where the ground's isn't known
};

static float GetCascadeSpacing(uint32 cascadeIndex)
{
    return GlimmerSHSpacing * float(1u << cascadeIndex);
}

static Handle<Texture> CreateVolumeTexture(TextureFormat format, Name name)
{
    Handle<Texture> texture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture3D,
        format,
        Vec3u(GlimmerSHGridXZ, GlimmerSHGridY * GlimmerSHCascades, GlimmerSHGridXZ),
        TextureFilterMode::Nearest,
        TextureFilterMode::Nearest,
        TextureWrapMode::Repeat,
        1,
        ImageUsage::Storage | ImageUsage::Sampled });

    texture->SetIsTransient(true);
    texture->SetName(name);
    Check(texture->Create());

    return texture;
}

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

GlimmerSHVolume::GlimmerSHVolume()
    : m_refreshCascade(0),
      m_refreshSlice(0),
      m_updateIndex(0),
      m_shaderData {}
{
    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float spacing = GetCascadeSpacing(cascadeIndex);

        m_shaderData.cascades[cascadeIndex].params = Vec4f(spacing, 1.0f / spacing, 0.0f, 0.0f);
    }

    m_shaderData.info = Vec4u(GlimmerSHCascades, 0, 0, 0);
}

GlimmerSHVolume::~GlimmerSHVolume()
{
    m_visibilityTexture = Handle<Texture>();
    m_bounceTexture = Handle<Texture>();
    m_stateTexture = Handle<Texture>();

    for (Handle<Texture>& depthTexture : m_depthTextures)
    {
        depthTexture = Handle<Texture>();
    }
}

void GlimmerSHVolume::CreateResources()
{
    m_visibilityTexture = CreateVolumeTexture(TextureFormat::RGBA16F, NAME("GlimmerSHVisibility"));
    m_bounceTexture = CreateVolumeTexture(TextureFormat::RGBA16F, NAME("GlimmerSHBounce"));
    m_stateTexture = CreateVolumeTexture(TextureFormat::RG32, NAME("GlimmerSHState"));
    m_depthTextures[0] = CreateVolumeTexture(TextureFormat::RGBA16F, NAME("GlimmerSHDepthX"));
    m_depthTextures[1] = CreateVolumeTexture(TextureFormat::RGBA16F, NAME("GlimmerSHDepthY"));
    m_depthTextures[2] = CreateVolumeTexture(TextureFormat::RGBA16F, NAME("GlimmerSHDepthZ"));
}

const GpuImageViewRef& GlimmerSHVolume::GetVisibilityImageView() const
{
    return m_visibilityTexture.IsValid() ? RI.textureViewCache->GetOrCreate(m_visibilityTexture) : RI.placeholderData->GetImageView3D1x1x1R8();
}

const GpuImageViewRef& GlimmerSHVolume::GetBounceImageView() const
{
    return m_bounceTexture.IsValid() ? RI.textureViewCache->GetOrCreate(m_bounceTexture) : RI.placeholderData->GetImageView3D1x1x1R8();
}

const GpuImageViewRef& GlimmerSHVolume::GetStateImageView() const
{
    return m_stateTexture.IsValid() ? RI.textureViewCache->GetOrCreate(m_stateTexture) : RI.placeholderData->GetImageView3D1x1x1R8();
}

const GpuImageViewRef& GlimmerSHVolume::GetDepthImageView(uint32 axis) const
{
    return m_depthTextures[axis].IsValid() ? RI.textureViewCache->GetOrCreate(m_depthTextures[axis]) : RI.placeholderData->GetImageView3D1x1x1R8();
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

void GlimmerSHVolume::DispatchBox(Frame* frame, uint32 cascadeIndex, const Box& box, const GlimmerSHVolumeUpdateInputs& inputs, bool& inOutHasBarriers)
{
    CommandRecorder& cr = frame->cr;

    if (!inOutHasBarriers)
    {
        inOutHasBarriers = true;

        cr << InsertBarrier(m_visibilityTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_bounceTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_stateTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        for (const Handle<Texture>& depthTexture : m_depthTextures)
        {
            cr << InsertBarrier(depthTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        }

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
        FoliageExtinction,
        MaxRayDistance,
        MathUtil::Clamp(g_cvGlimmerGroundAlbedo.Get(), 0.0f, 1.0f),
        0.0f);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundTexture"_sh, inputs.surfaceCache->GetGroundImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerGroundAlbedoTexture"_sh, inputs.surfaceCache->GetGroundAlbedoImageView());
    cr << SetShaderUniform(uniformIndex++, "GlimmerSpansBuffer"_sh, inputs.spanCache->GetSpansBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
    cr << SetShaderUniform(uniformIndex++, "OutVisibility"_sh, RI.textureViewCache->GetOrCreate(m_visibilityTexture));
    cr << SetShaderUniform(uniformIndex++, "OutBounce"_sh, RI.textureViewCache->GetOrCreate(m_bounceTexture));
    cr << SetShaderUniform(uniformIndex++, "OutState"_sh, RI.textureViewCache->GetOrCreate(m_stateTexture));
    cr << SetShaderUniform(uniformIndex++, "OutDepthX"_sh, RI.textureViewCache->GetOrCreate(m_depthTextures[0]));
    cr << SetShaderUniform(uniformIndex++, "OutDepthY"_sh, RI.textureViewCache->GetOrCreate(m_depthTextures[1]));
    cr << SetShaderUniform(uniformIndex++, "OutDepthZ"_sh, RI.textureViewCache->GetOrCreate(m_depthTextures[2]));
    cr << SetShaderUniform(uniformIndex++, "GlimmerSHOccupancyTexture"_sh, inputs.occupancyImageView);

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

    if (!m_visibilityTexture.IsValid())
    {
        CreateResources();
    }

    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float spacing = GetCascadeSpacing(cascadeIndex);

        const Vec3i origin = Vec3i(
            int32(MathUtil::Floor(inputs.viewerPosition.x / spacing)) - int32(GlimmerSHGridXZ / 2),
            int32(MathUtil::Floor(inputs.viewerPosition.y / spacing)) - int32(GlimmerSHGridY / 2),
            int32(MathUtil::Floor(inputs.viewerPosition.z / spacing)) - int32(GlimmerSHGridXZ / 2));

        MoveWindow(cascadeIndex, origin);
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

    // spare budget re-traces settled windows a z slice at a time, which is how scene and sun changes get in
    const int32 sliceVoxels = int32(GlimmerSHGridXZ * GlimmerSHGridY);

    for (uint32 attempt = 0; attempt < GlimmerSHCascades && budget >= sliceVoxels; attempt++)
    {
        Cascade& cascade = m_cascades[m_refreshCascade];

        if (cascade.pending.Any())
        {
            m_refreshCascade = (m_refreshCascade + 1) % GlimmerSHCascades;
            m_refreshSlice = 0;

            continue;
        }

        Box slice = GetWindow(cascade.origin);
        slice.min.z += m_refreshSlice;
        slice.max.z = slice.min.z + 1;

        DispatchBox(frame, m_refreshCascade, slice, inputs, hasBarriers);

        budget -= sliceVoxels;

        if (++m_refreshSlice >= int32(GlimmerSHGridXZ))
        {
            m_refreshSlice = 0;
            m_refreshCascade = (m_refreshCascade + 1) % GlimmerSHCascades;
        }

        attempt = 0;
    }

    if (hasBarriers)
    {
        CommandRecorder& cr = frame->cr;

        cr << InsertBarrier(m_visibilityTexture->GetGpuImage(), ResourceState::ShaderResource);
        cr << InsertBarrier(m_bounceTexture->GetGpuImage(), ResourceState::ShaderResource);
        cr << InsertBarrier(m_stateTexture->GetGpuImage(), ResourceState::ShaderResource);

        for (const Handle<Texture>& depthTexture : m_depthTextures)
        {
            cr << InsertBarrier(depthTexture->GetGpuImage(), ResourceState::ShaderResource);
        }

        m_shaderData.info.w = 1;
    }
}

} // namespace Hyperion
