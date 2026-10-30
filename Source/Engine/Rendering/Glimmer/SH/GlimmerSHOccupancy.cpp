/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/SH/GlimmerSHOccupancy.hpp>
#include <Rendering/Glimmer/GlimmerSceneInstances.hpp>
#include <Rendering/Glimmer/GlimmerTriangleCache.hpp>
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
static EngineStatGpuTimer s_statGlimmerSHOccupancySplat("Rendering/GPU/Glimmer/SHOccupancySplat");
static EngineStatCounter<uint32> s_statGlimmerSHOccupancyScrolls("Rendering/Glimmer/SHOccupancyScrolls", false);
static EngineStatCounter<uint32> s_statGlimmerSHOccupancyDirtyBoxes("Rendering/Glimmer/SHOccupancyDirtyBoxes", false);
static EngineStatCounter<uint32> s_statGlimmerSHOccupancyRebuildSlices("Rendering/Glimmer/SHOccupancyRebuildSlices", false);
static EngineStatCounter<uint32> s_statGlimmerSHOccupancyFullBuilds("Rendering/Glimmer/SHOccupancyFullBuilds", false);
static EngineStatCounter<uint32> s_statGlimmerSHOccupancyVoxels("Rendering/Glimmer/SHOccupancyVoxels", false);

static StaticShaderPropertyId s_propOccupancyModeClear { ShaderProperty(NAME("MODE"), NAME("CLEAR")) };
static StaticShaderPropertyId s_propOccupancyModeSplat { ShaderProperty(NAME("MODE"), NAME("SPLAT")) };
static StaticShaderPropertyId s_propOccupancyModeResolve { ShaderProperty(NAME("MODE"), NAME("RESOLVE")) };
static StaticShaderPropertyId s_propOccupancyModeBlocks { ShaderProperty(NAME("MODE"), NAME("BLOCKS")) };

static constexpr uint32 OccupancyGroupSize = 64;
static constexpr int32 OccupancyBlockShift = 2;
static constexpr int32 OccupancyBlockVoxels = 1 << OccupancyBlockShift;

static constexpr int32 WindowSnapVoxels = 8;
static constexpr uint32 AlbedoSumWords = 4;
static constexpr uint32 LightmapSumWords = GlimmerSHOccupancyLightmapFaces * 4;

struct GlimmerSHOccupancySplatConstants
{
    Vec4i origin; // xyz = absolute voxel of the cascade's window, w = cascade
    
    Vec4f params; // x = voxel spacing, y = 1 / spacing
    
    Vec4u counts; // x = span instances, y = span triangles, z = groups along x
    
    Vec4i boxMin; // xyz = absolute voxel; only voxels in the box are cleared and splatted
    Vec4i boxMax; // xyz = absolute voxel, exclusive

    Vec4u lightmapStencils;
};

static constexpr size_t MaxDirtyBoxes = 8;

static const Vec3i OccupancyGridSize = Vec3i(int32(GlimmerSHOccupancyGridXZ), int32(GlimmerSHOccupancyGridY), int32(GlimmerSHOccupancyGridXZ));
static constexpr int32 RebuildSlices = 8;

static constexpr StringHash LightmapTextureNames[GlimmerMaxLightmapPages] = {
    "LightmapVolumeIrradianceTexture0"_sh,
    "LightmapVolumeIrradianceTexture1"_sh,
    "LightmapVolumeIrradianceTexture2"_sh,
    "LightmapVolumeIrradianceTexture3"_sh
};

#pragma region GlimmerSHOccupancy

GlimmerSHOccupancy::GlimmerSHOccupancy()
    : m_albedoSumsBuffer(nullptr),
      m_shaderData {},
      m_isSettled(true)
{
    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float spacing = GlimmerHelpers::GetGlimmerSHOccupancySpacing(cascadeIndex);

        m_shaderData.cascades[cascadeIndex].params = { spacing, 1.0f / spacing, 0.0f, 0.0f };
        m_builtGenerations[cascadeIndex] = ~0u;
        m_rebuildSlices[cascadeIndex] = -1;
        m_rebuildGenerations[cascadeIndex] = ~0u;
    }
}

GlimmerSHOccupancy::~GlimmerSHOccupancy()
{
    m_texture.Reset();

    m_maskBuffer.Shutdown();
}

void GlimmerSHOccupancy::CreateResources()
{
    m_texture = GlimmerHelpers::CreateGlimmerStorageTexture(
        TextureType::Texture3D,
        TextureFormat::RGBA8,
        Vec3u {
            GlimmerSHOccupancyGridXZ,
            GlimmerSHOccupancyGridY * GlimmerSHCascades,
            GlimmerSHOccupancyGridXZ
        },
        1,
        NAME("GlimmerSHOccupancy"),
        TextureWrapMode::ClampToEdge);

    const size_t numCascadeVoxels = size_t(GlimmerSHOccupancyGridXZ) * GlimmerSHOccupancyGridY * GlimmerSHOccupancyGridXZ;
    const size_t numVoxels = numCascadeVoxels * GlimmerSHCascades;

    // the mask words, then the traced cascades' lightmap words, then a word per block of their voxels
    const size_t numMaskWords = numVoxels * GlimmerSHOccupancyMaskWords + numCascadeVoxels * GlimmerSHOccupancyTracedCascades * GlimmerSHOccupancyLightmapFaces
        + (numCascadeVoxels / (OccupancyBlockVoxels * OccupancyBlockVoxels * OccupancyBlockVoxels)) * GlimmerSHOccupancyTracedCascades;

    m_maskBuffer = GlimmerHelpers::CreateGlimmerRWBuffer(sizeof(uint32), numMaskWords);
}

const GpuImageViewRef& GlimmerSHOccupancy::GetImageView() const
{
    return m_texture.IsValid()
        ? RI.textureViewCache->GetOrCreate(m_texture)
        : RI.placeholderData->GetImageView3D1x1x1R8();
}

void GlimmerSHOccupancy::SplatBox(
    Frame* frame,
    uint32 cascadeIndex,
    const Vec3i& origin,
    const GlimmerBox& box,
    const GlimmerSceneInstances& sceneInstances,
    const GlimmerTriangleCache& triangleCache)
{
    HYP_SCOPE;

    CommandRecorder& cr = frame->cr;

    const float spacing = GlimmerHelpers::GetGlimmerSHOccupancySpacing(cascadeIndex);

    const auto dispatchPass = [&](const StaticShaderPropertyId& modeProperty, uint32 numThreads)
    {
        const Vec3u groups = RenderHelpers::WrapComputeGroupCount((numThreads + OccupancyGroupSize - 1) / OccupancyGroupSize);

        GlimmerSHOccupancySplatConstants constants {};
        constants.origin = Vec4i(origin.x, origin.y, origin.z, int32(cascadeIndex));
        constants.params = Vec4f(spacing, 1.0f / spacing, 0.0f, 0.0f);
        constants.counts = Vec4u(sceneInstances.GetNumSpanInstances(), sceneInstances.GetNumSpanChunks(), groups.x, 0);
        constants.boxMin = Vec4i(box.min.x, box.min.y, box.min.z, 0);
        constants.boxMax = Vec4i(box.max.x, box.max.y, box.max.z, 0);
        constants.lightmapStencils = m_lightmapPages.stencilValues;

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
        cr << SetShaderUniform(uniformIndex++, "OutOccupancyMask"_sh, m_maskBuffer);
        cr << SetShaderUniform(uniformIndex++, "OutAlbedoSums"_sh, *m_albedoSumsBuffer);
        cr << SetShaderUniform(uniformIndex++, "SpanInstancesBuffer"_sh, sceneInstances.GetSpanInstancesBuffer());
        cr << SetShaderUniform(uniformIndex++, "SpanChunksBuffer"_sh, sceneInstances.GetSpanChunksBuffer());
        cr << SetShaderUniform(uniformIndex++, "GlimmerTrianglesBuffer"_sh, triangleCache.GetTrianglesBuffer());
        cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
        cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
        cr << SetShaderUniform(uniformIndex++, "GlimmerTriangleLightmapUVsBuffer"_sh, triangleCache.GetLightmapUVsBuffer());

        for (uint32 pageIndex = 0; pageIndex < GlimmerMaxLightmapPages; pageIndex++)
        {
            Texture* irradianceTexture = m_lightmapPages.irradianceTextures[pageIndex];

            cr << SetShaderUniform(uniformIndex++, LightmapTextureNames[pageIndex],
                RI.textureViewCache->GetOrCreate(irradianceTexture != nullptr ? irradianceTexture : RI.placeholderData->textureSolidBlack));
        }

        cr << DispatchCompute(groups);
    };

    const auto insertWriteBarriers = [&]()
    {
        cr << InsertBarrier(m_texture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_maskBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(*m_albedoSumsBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    };

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSHOccupancy);

    insertWriteBarriers();

    const Vec3i boxExtent = box.max - box.min;
    const uint32 numBoxVoxels = uint32(boxExtent.x * boxExtent.y * boxExtent.z);

    s_statGlimmerSHOccupancyVoxels += numBoxVoxels;

    dispatchPass(s_propOccupancyModeClear, numBoxVoxels);

    if (sceneInstances.GetNumSpanChunks() != 0)
    {
        insertWriteBarriers();

        // a group per chunk
        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerSHOccupancySplat);

            dispatchPass(s_propOccupancyModeSplat, sceneInstances.GetNumSpanChunks() * OccupancyGroupSize);
        }

        insertWriteBarriers();

        dispatchPass(s_propOccupancyModeResolve, numBoxVoxels);
    }

    if (cascadeIndex < GlimmerSHOccupancyTracedCascades)
    {
        insertWriteBarriers();

        const Vec3i blockMin = Vec3i(box.min.x >> OccupancyBlockShift, box.min.y >> OccupancyBlockShift, box.min.z >> OccupancyBlockShift);
        const Vec3i blockMax = Vec3i(
            (box.max.x + OccupancyBlockVoxels - 1) >> OccupancyBlockShift,
            (box.max.y + OccupancyBlockVoxels - 1) >> OccupancyBlockShift,
            (box.max.z + OccupancyBlockVoxels - 1) >> OccupancyBlockShift);

        const Vec3i blockExtent = blockMax - blockMin;

        dispatchPass(s_propOccupancyModeBlocks, uint32(blockExtent.x * blockExtent.y * blockExtent.z));
    }

    cr << InsertBarrier(m_texture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(m_maskBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);
}

void GlimmerSHOccupancy::SetBuilt(uint32 cascadeIndex, const Vec3i& origin)
{
    m_builtOrigins[cascadeIndex] = origin;
    m_shaderData.cascades[cascadeIndex].origin = Vec4i(origin, 1);
}

void GlimmerSHOccupancy::ScrollWindow(
    Frame* frame,
    uint32 cascadeIndex,
    const Vec3i& origin,
    const GlimmerSceneInstances& sceneInstances,
    const GlimmerTriangleCache& triangleCache)
{
    const Vec3i oldMin = m_builtOrigins[cascadeIndex];
    const Vec3i oldMax = oldMin + OccupancyGridSize;

    GlimmerBox remaining {
        origin,
        origin + OccupancyGridSize
    };

    for (int axis = 0; axis < 3; axis++)
    {
        GlimmerBox entering = remaining;

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

        SplatBox(
            frame,
            cascadeIndex,
            origin,
            entering,
            sceneInstances,
            triangleCache);
    }

    SetBuilt(cascadeIndex, origin);
}

void GlimmerSHOccupancy::Update(
    Frame* frame,
    const Vec3f& viewerPosition,
    const Vec3f& lookAhead,
    const GlimmerSceneInstances& sceneInstances,
    const GlimmerTriangleCache& triangleCache,
    const GlimmerLightmapPages& lightmapPages,
    RWStructuredBuffer& scratchBuffer)
{
    HYP_SCOPE;

    m_lightmapPages = lightmapPages;

    if (!m_texture.IsValid())
    {
        CreateResources();

        frame->cr << InsertBarrier(m_texture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
        frame->cr << InsertBarrier(m_maskBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);
    }

    const size_t numCascadeVoxels = size_t(GlimmerSHOccupancyGridXZ) * GlimmerSHOccupancyGridY * GlimmerSHOccupancyGridXZ;

    GlimmerHelpers::EnsureGlimmerBufferCapacity(scratchBuffer, numCascadeVoxels * (AlbedoSumWords + LightmapSumWords) * sizeof(uint32));

    m_albedoSumsBuffer = &scratchBuffer;

    if (!sceneInstances.IsReady() || !triangleCache.IsReady() || !sceneInstances.GetSpanInstancesBuffer().IsInitialized())
    {
        m_isSettled = true;

        return;
    }

    const uint32 generation = sceneInstances.GetGeneration();

    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        if (m_builtGenerations[cascadeIndex] == generation
            || m_builtGenerations[cascadeIndex] == ~0u
            || sceneInstances.IsSpanFullyDirty())
        {
            continue;
        }

        if (m_rebuildSlices[cascadeIndex] >= 0)
        {
            continue;
        }

        // a cascade too far behind the journal is rebuilt below
        GlimmerSceneChanges changes;
        if (!sceneInstances.GetChangesSince(m_builtGenerations[cascadeIndex], changes))
        {
            continue;
        }

        const float invSpacing = 1.0f / GlimmerHelpers::GetGlimmerSHOccupancySpacing(cascadeIndex);

        const Vec3i windowMin = m_builtOrigins[cascadeIndex];
        const Vec3i windowMax = windowMin + Vec3i(int32(GlimmerSHOccupancyGridXZ), int32(GlimmerSHOccupancyGridY), int32(GlimmerSHOccupancyGridXZ));

        Array<GlimmerBox, GlimmerAllocator>& dirtyBoxes = m_dirtyBoxes[cascadeIndex];

        for (const GlimmerSceneChange& change : changes)
        {
            // another LOD of the same instance is within its LOD error of what was splatted, not worth a splat
            if (change.isLodOnly)
            {
                continue;
            }

            const BoundingBox& bounds = change.bounds;

            const Vec3i boundsMin = Vec3i(MathUtil::Floor(bounds.min * invSpacing));
            const Vec3i boundsMax = Vec3i(MathUtil::Floor(bounds.max * invSpacing));

            const GlimmerBox box {
                MathUtil::Max(boundsMin - 1, windowMin),
                MathUtil::Min(boundsMax + 2, windowMax)
            };

            if (box.max.x > box.min.x && box.max.y > box.min.y && box.max.z > box.min.z)
            {
                dirtyBoxes.PushBack(box);
            }
        }

        // too many to splat one by one: the pair whose union adds the least is merged, until few enough are left
        while (dirtyBoxes.Size() > MaxDirtyBoxes)
        {
            size_t bestA = 0;
            size_t bestB = 1;
            int64 bestGrowth = MathUtil::MaxSafeValue<int64>();

            for (size_t a = 0; a < dirtyBoxes.Size(); a++)
            {
                for (size_t b = a + 1; b < dirtyBoxes.Size(); b++)
                {
                    const GlimmerBox merged = GlimmerBox::Union(dirtyBoxes[a], dirtyBoxes[b]);

                    const int64 growth = merged.Volume() - dirtyBoxes[a].Volume() - dirtyBoxes[b].Volume();

                    if (growth < bestGrowth)
                    {
                        bestGrowth = growth;
                        bestA = a;
                        bestB = b;
                    }
                }
            }

            dirtyBoxes[bestA] = GlimmerBox::Union(dirtyBoxes[bestA], dirtyBoxes[bestB]);
            dirtyBoxes.EraseAt(bestB);
        }

        m_builtGenerations[cascadeIndex] = generation;
    }

    // finest first, one scroll or rebuild step a frame: what's nearest the camera settles first
    bool hasRebuilt = false;

    m_isSettled = true;

    for (uint32 cascadeIndex = 0; cascadeIndex < GlimmerSHCascades; cascadeIndex++)
    {
        const float spacing = GlimmerHelpers::GetGlimmerSHOccupancySpacing(cascadeIndex);

        // must lean exactly as the SH volume's windows do, which line up with these
        const Vec3f center = GlimmerHelpers::GetGlimmerWindowCenter(
            viewerPosition,
            lookAhead,
            Vec3f {
                float(GlimmerSHOccupancyGridXZ),
                float(GlimmerSHOccupancyGridY),
                float(GlimmerSHOccupancyGridXZ)
            } * spacing);

        const Vec3i origin = {
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(center.x / spacing)) - int32(GlimmerSHOccupancyGridXZ / 2) + WindowSnapVoxels / 2, WindowSnapVoxels),
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(center.y / spacing)) - int32(GlimmerSHOccupancyGridY / 2) + WindowSnapVoxels / 2, WindowSnapVoxels),
            MathUtil::FloorToMultiple(int32(MathUtil::Floor(center.z / spacing)) - int32(GlimmerSHOccupancyGridXZ / 2) + WindowSnapVoxels / 2, WindowSnapVoxels)
        };

        Array<GlimmerBox, GlimmerAllocator>& dirtyBoxes = m_dirtyBoxes[cascadeIndex];

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

                ScrollWindow(frame, cascadeIndex, origin, sceneInstances, triangleCache);

                s_statGlimmerSHOccupancyScrolls += 1;

                hasRebuilt = true;
            }

            for (const GlimmerBox& dirtyBox : dirtyBoxes)
            {
                const GlimmerBox box {
                    MathUtil::Max(dirtyBox.min, origin),
                    MathUtil::Min(dirtyBox.max, origin + OccupancyGridSize)
                };

                if (box.max.x > box.min.x && box.max.y > box.min.y && box.max.z > box.min.z)
                {
                    s_statGlimmerSHOccupancyDirtyBoxes += 1;

                    SplatBox(frame, cascadeIndex, origin, box, sceneInstances, triangleCache);
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
                SplatBox(
                    frame,
                    cascadeIndex,
                    origin,
                    GlimmerBox { origin, origin + OccupancyGridSize },
                    sceneInstances,
                    triangleCache);

                s_statGlimmerSHOccupancyFullBuilds += 1;

                SetBuilt(cascadeIndex, origin);
                m_builtGenerations[cascadeIndex] = generation;

                continue;
            }

            m_rebuildSlices[cascadeIndex] = 0;
        }

        // in place, a slice a frame: the window stays where it was built and scrolls to the viewer once it's done
        const int32 sliceDepth = OccupancyGridSize.z / RebuildSlices;

        GlimmerBox slice { builtOrigin, builtOrigin + OccupancyGridSize };
        slice.min.z += m_rebuildSlices[cascadeIndex] * sliceDepth;
        slice.max.z = slice.min.z + sliceDepth;

        SplatBox(frame, cascadeIndex, builtOrigin, slice, sceneInstances, triangleCache);

        s_statGlimmerSHOccupancyRebuildSlices += 1;

        if (++m_rebuildSlices[cascadeIndex] == RebuildSlices)
        {
            m_rebuildSlices[cascadeIndex] = -1;
            m_builtGenerations[cascadeIndex] = m_rebuildGenerations[cascadeIndex];
        }

        if (m_rebuildSlices[cascadeIndex] >= 0
            || builtOrigin != origin
            || m_builtGenerations[cascadeIndex] != generation)
        {
            m_isSettled = false;
        }
    }
}

#pragma endregion GlimmerSHOccupancy

} // namespace Hyperion
