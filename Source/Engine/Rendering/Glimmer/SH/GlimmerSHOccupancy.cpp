/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/SH/GlimmerSHOccupancy.hpp>
#include <Rendering/Glimmer/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/GlimmerBLASCache.hpp>

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

#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Framework/EngineStats.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statGlimmerSHOccupancy("Rendering/GPU/Glimmer/SHOccupancy");

static StaticShaderPropertyId s_propOccupancyModeClear { ShaderProperty(NAME("MODE"), NAME("CLEAR")) };
static StaticShaderPropertyId s_propOccupancyModeSplat { ShaderProperty(NAME("MODE"), NAME("SPLAT")) };

static constexpr uint32 OccupancyGroupSize = 64;
static constexpr uint32 MaxGroupsPerDimension = 65535;

// windows move in steps of this many voxels, so a walking viewer only rebuilds a cascade now and then
static constexpr int32 WindowSnapVoxels = 8;

// Must match GlimmerSHOccupancySplatConstants in Shaders/Glimmer/SH/GlimmerSHOccupancySplat.hlsl
struct GlimmerSHOccupancySplatConstants
{
    Vec4i origin; // xyz = absolute voxel of the cascade's window, w = cascade
    Vec4f params; // x = voxel spacing, y = 1 / spacing
    Vec4u counts; // x = span instances, y = span triangles, z = groups along x
};

static float GetOccupancySpacing(uint32 cascadeIndex)
{
    return 0.5f * GlimmerSHSpacing * float(1u << cascadeIndex);
}

static int32 FloorToMultiple(int32 value, int32 multiple)
{
    return int32(MathUtil::Floor(float(value) / float(multiple))) * multiple;
}

GlimmerSHOccupancy::GlimmerSHOccupancy()
    : m_shaderData {}
{
    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float spacing = GetOccupancySpacing(cascadeIndex);

        m_shaderData.cascades[cascadeIndex].params = Vec4f(spacing, 1.0f / spacing, 0.0f, 0.0f);
        m_builtGenerations[cascadeIndex] = ~0u;
    }
}

GlimmerSHOccupancy::~GlimmerSHOccupancy()
{
    m_texture = Handle<Texture>();
}

void GlimmerSHOccupancy::CreateResources()
{
    m_texture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture3D,
        TextureFormat::RGBA8,
        Vec3u(GlimmerSHOccupancyGridXZ, GlimmerSHOccupancyGridY * GlimmerSHCascades, GlimmerSHOccupancyGridXZ),
        TextureFilterMode::Nearest,
        TextureFilterMode::Nearest,
        TextureWrapMode::ClampToEdge,
        1,
        ImageUsage::Storage | ImageUsage::Sampled });

    m_texture->SetIsTransient(true);
    m_texture->SetName(NAME("GlimmerSHOccupancy"));
    Check(m_texture->Create());
}

const GpuImageViewRef& GlimmerSHOccupancy::GetImageView() const
{
    return m_texture.IsValid() ? RI.textureViewCache->GetOrCreate(m_texture) : RI.placeholderData->GetImageView3D1x1x1R8();
}

void GlimmerSHOccupancy::RebuildCascade(Frame* frame, uint32 cascadeIndex, const Vec3i& origin, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache)
{
    HYP_SCOPE;

    CommandRecorder& cr = frame->cr;

    const float spacing = GetOccupancySpacing(cascadeIndex);

    const auto dispatchPass = [&](const StaticShaderPropertyId& modeProperty, uint32 numThreads)
    {
        const uint32 numGroups = MathUtil::Max((numThreads + OccupancyGroupSize - 1) / OccupancyGroupSize, 1u);
        const uint32 groupsX = MathUtil::Min(numGroups, MaxGroupsPerDimension);
        const uint32 groupsY = (numGroups + groupsX - 1) / groupsX;

        GlimmerSHOccupancySplatConstants constants {};
        constants.origin = Vec4i(origin.x, origin.y, origin.z, int32(cascadeIndex));
        constants.params = Vec4f(spacing, 1.0f / spacing, 0.0f, 0.0f);
        constants.counts = Vec4u(tlas.GetNumSpanInstances(), tlas.GetNumSpanChunks(), groupsX, 0);

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        ShaderPropertySet shaderProperties;
        shaderProperties.Add(modeProperty);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerSHOccupancySplat"), shaderProperties));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "OutOccupancy"_sh, RI.textureViewCache->GetOrCreate(m_texture));
        cr << SetShaderUniform(uniformIndex++, "SpanInstancesBuffer"_sh, tlas.GetSpanInstancesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerSpanInstanceShaderData)));
        cr << SetShaderUniform(uniformIndex++, "SpanChunksBuffer"_sh, tlas.GetSpanChunksBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerSpanChunkShaderData)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBLASTrianglesBuffer"_sh, blasCache.GetTrianglesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerTriangle)));
        cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
        cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());

        cr << DispatchCompute(Vec3u { groupsX, groupsY, 1 });
    };

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSHOccupancy);

    cr << InsertBarrier(m_texture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    dispatchPass(s_propOccupancyModeClear, GlimmerSHOccupancyGridXZ * GlimmerSHOccupancyGridY * GlimmerSHOccupancyGridXZ);

    if (tlas.GetNumSpanChunks() != 0)
    {
        cr << InsertBarrier(m_texture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        // a group per chunk
        dispatchPass(s_propOccupancyModeSplat, tlas.GetNumSpanChunks() * OccupancyGroupSize);
    }

    cr << InsertBarrier(m_texture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);

    m_builtOrigins[cascadeIndex] = origin;
    m_builtGenerations[cascadeIndex] = tlas.GetGeneration();

    m_shaderData.cascades[cascadeIndex].origin = Vec4i(origin.x, origin.y, origin.z, 1);
}

void GlimmerSHOccupancy::Update(Frame* frame, const Vec3f& viewerPosition, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache)
{
    HYP_SCOPE;

    if (!tlas.IsReady() || !blasCache.IsReady() || !tlas.GetSpanInstancesBuffer().IsValid())
    {
        return;
    }

    if (!m_texture.IsValid())
    {
        CreateResources();
    }

    // finest first, one cascade a frame: what's nearest the camera settles first
    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float spacing = GetOccupancySpacing(cascadeIndex);

        const Vec3i origin = Vec3i(
            FloorToMultiple(int32(MathUtil::Floor(viewerPosition.x / spacing)) - int32(GlimmerSHOccupancyGridXZ / 2), WindowSnapVoxels),
            FloorToMultiple(int32(MathUtil::Floor(viewerPosition.y / spacing)) - int32(GlimmerSHOccupancyGridY / 2), WindowSnapVoxels),
            FloorToMultiple(int32(MathUtil::Floor(viewerPosition.z / spacing)) - int32(GlimmerSHOccupancyGridXZ / 2), WindowSnapVoxels));

        if (m_builtGenerations[cascadeIndex] == tlas.GetGeneration() && m_builtOrigins[cascadeIndex] == origin)
        {
            continue;
        }

        RebuildCascade(frame, cascadeIndex, origin, tlas, blasCache);

        break;
    }
}

} // namespace Hyperion
