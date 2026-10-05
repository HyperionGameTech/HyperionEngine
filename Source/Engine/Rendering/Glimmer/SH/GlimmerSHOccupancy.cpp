/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/SH/GlimmerSHOccupancy.hpp>
#include <Rendering/Glimmer/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/GlimmerBLASCache.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderHelpers.hpp>
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

#include <Framework/EngineStats.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statGlimmerSHOccupancy("Rendering/GPU/Glimmer/SHOccupancy");

static StaticShaderPropertyId s_propOccupancyModeClear { ShaderProperty(NAME("MODE"), NAME("CLEAR")) };
static StaticShaderPropertyId s_propOccupancyModeSplat { ShaderProperty(NAME("MODE"), NAME("SPLAT")) };
static StaticShaderPropertyId s_propOccupancyModeResolve { ShaderProperty(NAME("MODE"), NAME("RESOLVE")) };

static constexpr uint32 OccupancyGroupSize = 64;
static constexpr int32 WindowSnapVoxels = 8;
static constexpr uint32 AlbedoSumWords = 4;

struct GlimmerSHOccupancySplatConstants
{
    Vec4i origin; // xyz = absolute voxel of the cascade's window, w = cascade
    Vec4f params; // x = voxel spacing, y = 1 / spacing
    Vec4u counts; // x = span instances, y = span triangles, z = groups along x
    Vec4i boxMin; // xyz = absolute voxel; only voxels in the box are cleared and splatted
    Vec4i boxMax; // xyz = absolute voxel, exclusive
};

static constexpr size_t MaxDirtyBoxes = 8;

static const Vec3i OccupancyGridSize = Vec3i(int32(GlimmerSHOccupancyGridXZ), int32(GlimmerSHOccupancyGridY), int32(GlimmerSHOccupancyGridXZ));
static constexpr int32 RebuildSlices = 8;

#pragma region GlimmerSHOccupancy

GlimmerSHOccupancy::GlimmerSHOccupancy()
    : m_shaderData {},
      m_isSettled(true)
{
    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float spacing = GetGlimmerSHOccupancySpacing(cascadeIndex);

        m_shaderData.cascades[cascadeIndex].params = Vec4f(spacing, 1.0f / spacing, 0.0f, 0.0f);
        m_builtGenerations[cascadeIndex] = ~0u;
        m_rebuildSlices[cascadeIndex] = -1;
        m_rebuildGenerations[cascadeIndex] = ~0u;
    }
}

GlimmerSHOccupancy::~GlimmerSHOccupancy()
{
    m_texture = Handle<Texture>();

    EnqueueDeletion(std::move(m_maskBuffer));
    EnqueueDeletion(std::move(m_albedoSumsBuffer));
}

void GlimmerSHOccupancy::CreateResources()
{
    m_texture = CreateGlimmerStorageTexture(
        TextureType::Texture3D,
        TextureFormat::RGBA8,
        Vec3u(GlimmerSHOccupancyGridXZ, GlimmerSHOccupancyGridY * GlimmerSHCascades, GlimmerSHOccupancyGridXZ),
        1,
        NAME("GlimmerSHOccupancy"),
        TextureWrapMode::ClampToEdge);

    const size_t numVoxels = size_t(GlimmerSHOccupancyGridXZ) * GlimmerSHOccupancyGridY * GlimmerSHCascades * GlimmerSHOccupancyGridXZ;

    m_maskBuffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, numVoxels * GlimmerSHOccupancyMaskWords * sizeof(uint32), alignof(uint32));
    Check(m_maskBuffer->Create());

    const size_t numCascadeVoxels = size_t(GlimmerSHOccupancyGridXZ) * GlimmerSHOccupancyGridY * GlimmerSHOccupancyGridXZ;

    m_albedoSumsBuffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, numCascadeVoxels * AlbedoSumWords * sizeof(uint32), alignof(uint32));
    Check(m_albedoSumsBuffer->Create());
}

const GpuImageViewRef& GlimmerSHOccupancy::GetImageView() const
{
    return m_texture.IsValid() ? RI.textureViewCache->GetOrCreate(m_texture) : RI.placeholderData->GetImageView3D1x1x1R8();
}

void GlimmerSHOccupancy::SplatBox(Frame* frame, uint32 cascadeIndex, const Vec3i& origin, const Box& box, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache)
{
    HYP_SCOPE;

    CommandRecorder& cr = frame->cr;

    const float spacing = GetGlimmerSHOccupancySpacing(cascadeIndex);

    const auto dispatchPass = [&](const StaticShaderPropertyId& modeProperty, uint32 numThreads)
    {
        const Vec3u groups = helpers::WrapComputeGroupCount((numThreads + OccupancyGroupSize - 1) / OccupancyGroupSize);

        GlimmerSHOccupancySplatConstants constants {};
        constants.origin = Vec4i(origin.x, origin.y, origin.z, int32(cascadeIndex));
        constants.params = Vec4f(spacing, 1.0f / spacing, 0.0f, 0.0f);
        constants.counts = Vec4u(tlas.GetNumSpanInstances(), tlas.GetNumSpanChunks(), groups.x, 0);
        constants.boxMin = Vec4i(box.min.x, box.min.y, box.min.z, 0);
        constants.boxMax = Vec4i(box.max.x, box.max.y, box.max.z, 0);

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
        cr << SetShaderUniform(uniformIndex++, "OutOccupancyMask"_sh, m_maskBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "OutAlbedoSums"_sh, m_albedoSumsBuffer.Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "SpanInstancesBuffer"_sh, tlas.GetSpanInstancesBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerSpanInstanceShaderData)));
        cr << SetShaderUniform(uniformIndex++, "SpanChunksBuffer"_sh, tlas.GetSpanChunksBuffer().Get(), ShaderDataOffset(0, sizeof(GlimmerSpanChunkShaderData)));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBLASTrianglesBuffer"_sh, blasCache.GetTrianglesBuffer().Get(), ShaderDataOffset(0, sizeof(uint32)));
        cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
        cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());

        cr << DispatchCompute(groups);
    };

    const auto insertWriteBarriers = [&]()
    {
        cr << InsertBarrier(m_texture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_maskBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_albedoSumsBuffer.Get(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    };

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSHOccupancy);

    insertWriteBarriers();

    const Vec3i boxExtent = box.max - box.min;
    const uint32 numBoxVoxels = uint32(boxExtent.x * boxExtent.y * boxExtent.z);

    dispatchPass(s_propOccupancyModeClear, numBoxVoxels);

    if (tlas.GetNumSpanChunks() != 0)
    {
        insertWriteBarriers();

        // a group per chunk
        dispatchPass(s_propOccupancyModeSplat, tlas.GetNumSpanChunks() * OccupancyGroupSize);

        insertWriteBarriers();

        dispatchPass(s_propOccupancyModeResolve, numBoxVoxels);
    }

    cr << InsertBarrier(m_texture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(m_maskBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
}

void GlimmerSHOccupancy::SetBuilt(uint32 cascadeIndex, const Vec3i& origin)
{
    m_builtOrigins[cascadeIndex] = origin;
    m_shaderData.cascades[cascadeIndex].origin = Vec4i(origin.x, origin.y, origin.z, 1);
}

void GlimmerSHOccupancy::ScrollWindow(Frame* frame, uint32 cascadeIndex, const Vec3i& origin, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache)
{
    const Vec3i oldMin = m_builtOrigins[cascadeIndex];
    const Vec3i oldMax = oldMin + OccupancyGridSize;

    Box remaining { origin, origin + OccupancyGridSize };

    for (int axis = 0; axis < 3; axis++)
    {
        Box entering = remaining;

        if (origin[axis] > oldMin[axis])
        {
            entering.min[axis] = oldMax[axis];
            remaining.max[axis] = oldMax[axis];
        }
        else if (origin[axis] < oldMin[axis])
        {
            entering.max[axis] = oldMin[axis];
            remaining.min[axis] = oldMin[axis];
        }
        else
        {
            continue;
        }

        SplatBox(frame, cascadeIndex, origin, entering, tlas, blasCache);
    }

    SetBuilt(cascadeIndex, origin);
}

void GlimmerSHOccupancy::Update(Frame* frame, const Vec3f& viewerPosition, const GlimmerTLAS& tlas, const GlimmerBLASCache& blasCache)
{
    HYP_SCOPE;

    if (!m_texture.IsValid())
    {
        CreateResources();

        frame->cr << InsertBarrier(m_texture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
        frame->cr << InsertBarrier(m_maskBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    }

    if (!tlas.IsReady() || !blasCache.IsReady() || !tlas.GetSpanInstancesBuffer().IsValid())
    {
        m_isSettled = true;

        return;
    }

    const uint32 generation = tlas.GetGeneration();

    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        if (m_builtGenerations[cascadeIndex] == generation || m_builtGenerations[cascadeIndex] + 1 != generation || tlas.IsSpanFullyDirty())
        {
            continue;
        }

        if (m_rebuildSlices[cascadeIndex] >= 0)
        {
            continue;
        }

        const float invSpacing = 1.0f / GetGlimmerSHOccupancySpacing(cascadeIndex);
        const Vec3i windowMin = m_builtOrigins[cascadeIndex];
        const Vec3i windowMax = windowMin + Vec3i(int32(GlimmerSHOccupancyGridXZ), int32(GlimmerSHOccupancyGridY), int32(GlimmerSHOccupancyGridXZ));

        Array<Box>& dirtyBoxes = m_dirtyBoxes[cascadeIndex];

        for (const BoundingBox& bounds : tlas.GetSpanDirtyBounds())
        {
            const Vec3i boundsMin = Vec3i(int32(MathUtil::Floor(bounds.min.x * invSpacing)), int32(MathUtil::Floor(bounds.min.y * invSpacing)), int32(MathUtil::Floor(bounds.min.z * invSpacing)));
            const Vec3i boundsMax = Vec3i(int32(MathUtil::Floor(bounds.max.x * invSpacing)), int32(MathUtil::Floor(bounds.max.y * invSpacing)), int32(MathUtil::Floor(bounds.max.z * invSpacing)));

            const Box box {
                Vec3i(MathUtil::Max(boundsMin.x - 1, windowMin.x), MathUtil::Max(boundsMin.y - 1, windowMin.y), MathUtil::Max(boundsMin.z - 1, windowMin.z)),
                Vec3i(MathUtil::Min(boundsMax.x + 2, windowMax.x), MathUtil::Min(boundsMax.y + 2, windowMax.y), MathUtil::Min(boundsMax.z + 2, windowMax.z))
            };

            if (box.max.x > box.min.x && box.max.y > box.min.y && box.max.z > box.min.z)
            {
                dirtyBoxes.PushBack(box);
            }
        }

        if (dirtyBoxes.Size() > MaxDirtyBoxes)
        {
            dirtyBoxes.Clear();

            continue;
        }

        m_builtGenerations[cascadeIndex] = generation;
    }

    // finest first, one scroll or rebuild step a frame: what's nearest the camera settles first
    bool hasRebuilt = false;

    m_isSettled = true;

    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float spacing = GetGlimmerSHOccupancySpacing(cascadeIndex);

        const Vec3i origin = Vec3i(
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(viewerPosition.x / spacing)) - int32(GlimmerSHOccupancyGridXZ / 2), WindowSnapVoxels),
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(viewerPosition.y / spacing)) - int32(GlimmerSHOccupancyGridY / 2), WindowSnapVoxels),
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(viewerPosition.z / spacing)) - int32(GlimmerSHOccupancyGridXZ / 2), WindowSnapVoxels));

        Array<Box>& dirtyBoxes = m_dirtyBoxes[cascadeIndex];

        const Vec3i builtOrigin = m_builtOrigins[cascadeIndex];

        const bool overlapsBuilt = m_builtGenerations[cascadeIndex] != ~0u
            && MathUtil::Abs(origin.x - builtOrigin.x) < OccupancyGridSize.x
            && MathUtil::Abs(origin.y - builtOrigin.y) < OccupancyGridSize.y
            && MathUtil::Abs(origin.z - builtOrigin.z) < OccupancyGridSize.z;

        if (m_rebuildSlices[cascadeIndex] < 0 && m_builtGenerations[cascadeIndex] == generation && overlapsBuilt)
        {
            if (builtOrigin != origin)
            {
                if (hasRebuilt)
                {
                    m_isSettled = false;

                    continue;
                }

                ScrollWindow(frame, cascadeIndex, origin, tlas, blasCache);

                hasRebuilt = true;
            }

            for (const Box& dirtyBox : dirtyBoxes)
            {
                const Box box {
                    Vec3i(MathUtil::Max(dirtyBox.min.x, origin.x), MathUtil::Max(dirtyBox.min.y, origin.y), MathUtil::Max(dirtyBox.min.z, origin.z)),
                    Vec3i(MathUtil::Min(dirtyBox.max.x, origin.x + OccupancyGridSize.x), MathUtil::Min(dirtyBox.max.y, origin.y + OccupancyGridSize.y), MathUtil::Min(dirtyBox.max.z, origin.z + OccupancyGridSize.z))
                };

                if (box.max.x > box.min.x && box.max.y > box.min.y && box.max.z > box.min.z)
                {
                    SplatBox(frame, cascadeIndex, origin, box, tlas, blasCache);
                }
            }

            dirtyBoxes.Clear();

            continue;
        }

        if (hasRebuilt)
        {
            m_isSettled = false;

            continue;
        }

        hasRebuilt = true;

        if (m_rebuildSlices[cascadeIndex] < 0 || !overlapsBuilt)
        {
            m_rebuildSlices[cascadeIndex] = -1;
            m_rebuildGenerations[cascadeIndex] = generation;

            dirtyBoxes.Clear();

            if (!overlapsBuilt)
            {
                SplatBox(frame, cascadeIndex, origin, Box { origin, origin + OccupancyGridSize }, tlas, blasCache);

                SetBuilt(cascadeIndex, origin);
                m_builtGenerations[cascadeIndex] = generation;

                continue;
            }

            m_rebuildSlices[cascadeIndex] = 0;
        }

        // in place, a slice a frame: the window stays where it was built and scrolls to the viewer once it's done
        const int32 sliceDepth = OccupancyGridSize.z / RebuildSlices;

        Box slice { builtOrigin, builtOrigin + OccupancyGridSize };
        slice.min.z += m_rebuildSlices[cascadeIndex] * sliceDepth;
        slice.max.z = slice.min.z + sliceDepth;

        SplatBox(frame, cascadeIndex, builtOrigin, slice, tlas, blasCache);

        if (++m_rebuildSlices[cascadeIndex] == RebuildSlices)
        {
            m_rebuildSlices[cascadeIndex] = -1;
            m_builtGenerations[cascadeIndex] = m_rebuildGenerations[cascadeIndex];
        }

        if (m_rebuildSlices[cascadeIndex] >= 0 || builtOrigin != origin || m_builtGenerations[cascadeIndex] != generation)
        {
            m_isSettled = false;
        }
    }
}

#pragma endregion GlimmerSHOccupancy

} // namespace Hyperion
