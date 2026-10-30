/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerSceneInstances.hpp>
#include <Rendering/Glimmer/GlimmerTriangleCache.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/RenderHelpers.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/Entity.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Framework/CVarManager.hpp>

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/Threads.hpp>

#include <Core/Containers/Set.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/PerformanceClock.hpp>
#include <Core/Profiling/ProfileScope.hpp>

#include <algorithm>

namespace Hyperion {

extern CVar<bool> g_cvLightmapVolumes;

static constexpr uint32 MaxSpanInstances = 262144;
static constexpr uint32 MaxTracedInstances = 131072;

static constexpr float SolidLodErrorMeters = 0.05f;
static constexpr float FoliageLodErrorMeters = 1.0f;
static constexpr float LodErrorPerMeter = 0.02f;
static constexpr float LodHysteresis = 0.25f;

static constexpr double RebuildDebounceMs = 250.0;

static constexpr float ViewerMoveRegatherDistance = 4.0f;
static constexpr float ViewerMoveLodRegatherDistance = 16.0f;

static constexpr size_t MaxChangesPerGeneration = 64;
static constexpr size_t ChangeJournalGenerations = 8;

static constexpr uint32 QuantizeMaxResolution = 8;

#pragma region GlimmerSceneChanges

void GlimmerSceneChanges::Quantize(size_t maxChanges)
{
    if (changes.Size() <= maxChanges || maxChanges == 0)
    {
        return;
    }

    BoundingBox extent;

    size_t numLodOnly = 0;

    for (const GlimmerSceneChange& change : changes)
    {
        extent = extent.Union(change.bounds);

        numLodOnly += change.isLodOnly ? 1 : 0;
    }

    const Vec3f extentSize = MathUtil::Max(extent.max - extent.min, Vec3f(1e-3f));

    const size_t numOther = changes.Size() - numLodOnly;

    size_t lodOnlyBoxes = numLodOnly != 0 ? MathUtil::Max(maxChanges / 4, size_t(1)) : 0;
    size_t otherBoxes = numOther != 0 ? MathUtil::Max(maxChanges - lodOnlyBoxes, size_t(1)) : 0;

    if (numOther == 0)
    {
        lodOnlyBoxes = maxChanges;
    }

    Array<GlimmerSceneChange, GlimmerAllocator> merged;
    merged.Reserve(maxChanges);

    Array<BoundingBox, GlimmerTempAllocator> cells;
    Array<uint8, GlimmerTempAllocator> cellsUsed;

    const auto mergeGroup = [&](bool isLodOnly, size_t numBoxes)
    {
        if (numBoxes == 0)
        {
            return;
        }

        for (uint32 resolution = QuantizeMaxResolution; resolution; --resolution)
        {
            cells.Clear();
            cells.Resize(size_t(resolution * resolution));

            cellsUsed.Clear();
            cellsUsed.ResizeZeroed(size_t(resolution * resolution));

            size_t numUsed = 0;

            for (const GlimmerSceneChange& change : changes)
            {
                if (change.isLodOnly != isLodOnly)
                {
                    continue;
                }

                const Vec3f normalized = (change.bounds.GetCenter() - extent.min) / extentSize;

                const int32 cellX = MathUtil::Clamp(int32(normalized.x * float(resolution)), 0, int32(resolution) - 1);
                const int32 cellZ = MathUtil::Clamp(int32(normalized.z * float(resolution)), 0, int32(resolution) - 1);
                
                const size_t cellIndex = size_t(cellZ * resolution + cellX);

                if (!cellsUsed[cellIndex])
                {
                    cellsUsed[cellIndex] = 1;
                    cells[cellIndex] = change.bounds;

                    numUsed++;
                }
                else
                {
                    cells[cellIndex] = cells[cellIndex].Union(change.bounds);
                }
            }

            if (numUsed > numBoxes)
            {
                continue;
            }

            for (size_t cellIndex = 0; cellIndex < cells.Size(); cellIndex++)
            {
                if (cellsUsed[cellIndex])
                {
                    merged.PushBack(GlimmerSceneChange { cells[cellIndex], isLodOnly });
                }
            }

            return;
        }
    };

    mergeGroup(false, otherBoxes);
    mergeGroup(true, lodOnlyBoxes);

    changes = std::move(merged);
}

#pragma endregion GlimmerSceneChanges

#pragma region GlimmerSceneInstances

GlimmerSceneInstances::GlimmerSceneInstances()
    : m_lastBuildStartTime(0),
      m_triangleGenerationAtGather(~0u),
      m_triangleEvictionGenerationAtGather(0),
      m_activeInputHash(0),
      m_spanFullyDirty(true),
      m_dirty(true),
      m_waitingForTriangles(false),
      m_usesLightmaps(false)
{
}

GlimmerSceneInstances::~GlimmerSceneInstances()
{
    AssertDebug(m_activeMeshKeys.Empty() && m_pendingMeshKeys.Empty());

    if (m_buildTask.IsValid() && !m_buildTask.IsCompleted())
    {
        m_buildTask.Await();
    }

    m_spanInstancesBuffer.Shutdown();
    m_tlasNodesBuffer.Shutdown();
    m_tracedInstancesBuffer.Shutdown();
    m_spanChunksBuffer.Shutdown();
}

void GlimmerSceneInstances::Release(GlimmerTriangleCache& triangleCache)
{
    if (m_buildTask.IsValid() && !m_buildTask.IsCompleted())
    {
        m_buildTask.Await();
    }

    m_buildTask = Task<BuildResult>();

    triangleCache.RemoveReferences(m_activeMeshKeys.ToSpan());
    triangleCache.RemoveReferences(m_pendingMeshKeys.ToSpan());

    m_activeMeshKeys.Clear();
    m_pendingMeshKeys.Clear();
}

bool GlimmerSceneInstances::GetChangesSince(
    uint32 seenGeneration,
    GlimmerSceneChanges& outChanges) const
{
    const uint32 generation = GetGeneration();

    if (seenGeneration == generation)
    {
        return true;
    }

    if (m_changeJournal.Empty() || seenGeneration > generation || m_changeJournal.Front().generation > seenGeneration + 1)
    {
        return false;
    }

    for (const ChangeSet& changeSet : m_changeJournal)
    {
        if (changeSet.generation <= seenGeneration)
        {
            continue;
        }

        if (changeSet.isEverywhere)
        {
            return false;
        }

        ////////////////////////// turn and face the strange... //////////////////////////
        outChanges.changes.Concat(changeSet.changes.changes.ToSpan());
    }

    return true;
}

void GlimmerSceneInstances::Gather(RenderProxyList& rpl, const BoundingBox& region, const Vec3f& viewerPosition, GlimmerTriangleCache& triangleCache, BuildInput& outInput, uint32& outNumWaitingForTriangles)
{
    HYP_SCOPE;

    m_usesLightmaps = g_cvLightmapVolumes.Get();

    outNumWaitingForTriangles = 0;

    Set<uint64, GlimmerAllocator> meshKeysSeen;
    Map<uint64, uint8, GlimmerAllocator> instanceLods;

    const float lodErrorScale = triangleCache.GetLodErrorScale();

    // near-field rays trace what stands within this of the viewer's cell; the rest is the far field's.
    // the traced set and its LODs follow the cell, so the TLAS only changes when the viewer crosses one (or the scene changes)
    const float tracedCellSize = MathUtil::Max(g_cvGlimmerBVHCellSize.Get(), 1.0f);

    // a cell in xz only: climbing and diving don't change the set or its LODs
    const Vec2f tracedCenter = {
        (float(MathUtil::Floor(viewerPosition.x / tracedCellSize)) + 0.5f) * tracedCellSize,
        (float(MathUtil::Floor(viewerPosition.z / tracedCellSize)) + 0.5f) * tracedCellSize
    };

    const float tracedRadius = MathUtil::Max(g_cvGlimmerBVHRadius.Get(), 0.0f);

    BoundingBox tracedRegion = region;
    tracedRegion.min.x = MathUtil::Max(region.min.x, tracedCenter.x - tracedRadius);
    tracedRegion.max.x = MathUtil::Min(region.max.x, tracedCenter.x + tracedRadius);
    tracedRegion.min.z = MathUtil::Max(region.min.z, tracedCenter.y - tracedRadius);
    tracedRegion.max.z = MathUtil::Min(region.max.z, tracedCenter.y + tracedRadius);

    uint64 materialHash = 0;

    // the rect offset is 0 for a mesh that isn't lit by a lightmap
    const auto addInstance = [&](
            const RenderProxyMesh& proxy,
            const Mat4f& objectToWorld,
            uint32 flags,
            uint32 materialIndex,
            uint32 instanceId,
            uint32 lightmapRectOffset,
            uint32 lightmapRectSize)
    {
        const bool isFoliage = (flags & GIF_FOLIAGE) != 0;

        if (outInput.spanInstances.Size() >= MaxSpanInstances)
        {
            return;
        }

        float priority = viewerPosition.Distance(objectToWorld.ExtractTranslation());

        if (proxy.meshAabb.IsValid())
        {
            const BoundingBox estimatedBounds = objectToWorld * proxy.meshAabb;

            if (!estimatedBounds.Overlaps(region) || (isFoliage && estimatedBounds.max.y - estimatedBounds.min.y < GlimmerSpansMinFoliageHeight))
            {
                return;
            }

            const Vec3f nearestPoint = {
                MathUtil::Clamp(viewerPosition.x, estimatedBounds.min.x, estimatedBounds.max.x),
                MathUtil::Clamp(viewerPosition.y, estimatedBounds.min.y, estimatedBounds.max.y),
                MathUtil::Clamp(viewerPosition.z, estimatedBounds.min.z, estimatedBounds.max.z)
            };

            priority = viewerPosition.Distance(nearestPoint);
        }

        const MeshDesc& meshDesc = proxy.mesh->GetMeshDesc();
        const float worldScale = objectToWorld.ExtractMaxScale();
        
        const float maxError = MathUtil::Max(
            isFoliage ? FoliageLodErrorMeters : SolidLodErrorMeters,
            priority * LodErrorPerMeter * lodErrorScale);

        uint8 lodIndex = meshDesc.GetCoarsestLodWithinError(worldScale, maxError);

        const uint64 instanceKey = (uint64(proxy.entity ? proxy.entity->Id().Value() : 0u) << 32) | instanceId;

        uint8 previousLod = lodIndex;

        if (const KeyValuePair<uint64, uint8>* previous = m_instanceLods.TryGet(instanceKey))
        {
            previousLod = previous->second;

            const uint8 coarsestTight = meshDesc.GetCoarsestLodWithinError(worldScale, maxError * (1.0f - LodHysteresis));
            const uint8 coarsestLoose = meshDesc.GetCoarsestLodWithinError(worldScale, maxError * (1.0f + LodHysteresis));

            if (previousLod >= coarsestTight && previousLod <= coarsestLoose && previousLod < MathUtil::Max<uint8>(meshDesc.GetNumLods(), 1))
            {
                lodIndex = previousLod;
            }
        }

        instanceLods.Set(instanceKey, lodIndex);

        GlimmerTriangleRef triangleRef;

        const GlimmerTriangleRequestResult requestResult = triangleCache.Request(proxy.mesh, lodIndex, priority, triangleRef);

        if (requestResult != GlimmerTriangleRequestResult::Resident)
        {
            if (requestResult == GlimmerTriangleRequestResult::Pending)
            {
                outNumWaitingForTriangles++;
            }

            bool hasFallback = previousLod != lodIndex && triangleCache.TryGetResident(proxy.mesh, previousLod, priority, triangleRef);

            for (uint8 distance = 1; !hasFallback && distance < MaxMeshLods; distance++)
            {
                hasFallback = (lodIndex + distance < MaxMeshLods
                    && triangleCache.TryGetResident(proxy.mesh, uint8(lodIndex + distance), priority, triangleRef));

                hasFallback |= (lodIndex >= distance
                    && triangleCache.TryGetResident(proxy.mesh, uint8(lodIndex - distance), priority, triangleRef));
            }

            if (!hasFallback)
            {
                return;
            }
        }

        const BoundingBox worldBounds = objectToWorld * triangleRef.localBounds;

        if (!worldBounds.IsValid() || !worldBounds.Overlaps(region))
        {
            return;
        }

        if (isFoliage && worldBounds.max.y - worldBounds.min.y < GlimmerSpansMinFoliageHeight)
        {
            return;
        }

        const uint32 instanceFlags = flags | (objectToWorld.Determinant() < 0.0f ? GIF_MIRRORED : GIF_NONE);

        const Mat4f gridToWorld = objectToWorld * Mat4f::Translation(triangleRef.gridOrigin) * Mat4f::Scaling(triangleRef.gridScale);

        GlimmerSpanInstanceShaderData& spanInstance = outInput.spanInstances.EmplaceBack();
        Memory::Copy(spanInstance.objectToWorld, gridToWorld.values, sizeof(spanInstance.objectToWorld));

        spanInstance.triangleBase = triangleRef.triangleBase;
        spanInstance.triangleCount = triangleRef.triangleCount;
        spanInstance.materialIndex = materialIndex;
        spanInstance.flags = instanceFlags;

        const bool hasLightmap = lightmapRectOffset != 0 && triangleRef.lightmapUVBase != ~0u;

        spanInstance.lightmapUVBase = hasLightmap ? triangleRef.lightmapUVBase : ~0u;
        spanInstance.lightmapRectOffset = hasLightmap ? lightmapRectOffset : 0;
        spanInstance.lightmapRectSize = hasLightmap ? lightmapRectSize : 0;
        spanInstance.texcoordBase = triangleRef.texcoordBase;

        outInput.spanInstanceBounds.PushBack(worldBounds);
        outInput.spanMaterialHashes.PushBack(materialHash);

        // leaves dim light rather than stop it: they're the canopy spans' business, not the BVH's
        if (!isFoliage && outInput.tracedInstances.Size() < MaxTracedInstances && worldBounds.Overlaps(tracedRegion))
        {
            // from the nearest point of the cell, so wherever the viewer is in it the LOD is no coarser than its own would be
            const float tracedGapX = MathUtil::Max(MathUtil::Abs(tracedCenter.x - MathUtil::Clamp(tracedCenter.x, worldBounds.min.x, worldBounds.max.x)) - tracedCellSize * 0.5f, 0.0f);
            const float tracedGapZ = MathUtil::Max(MathUtil::Abs(tracedCenter.y - MathUtil::Clamp(tracedCenter.y, worldBounds.min.z, worldBounds.max.z)) - tracedCellSize * 0.5f, 0.0f);

            const float tracedMaxError = MathUtil::Max(SolidLodErrorMeters, Vec2f(tracedGapX, tracedGapZ).Length() * LodErrorPerMeter * lodErrorScale);
            const uint8 tracedLod = meshDesc.GetCoarsestLodWithinError(worldScale, tracedMaxError);

            // the span's triangles stand in until the traced LOD's are resident
            GlimmerTriangleRef tracedRef = triangleRef;

            if (tracedLod != lodIndex || requestResult != GlimmerTriangleRequestResult::Resident)
            {
                GlimmerTriangleRef tracedLodRef;

                const GlimmerTriangleRequestResult tracedResult = triangleCache.Request(proxy.mesh, tracedLod, priority, tracedLodRef);

                if (tracedResult == GlimmerTriangleRequestResult::Resident)
                {
                    tracedRef = tracedLodRef;
                }
                else if (tracedResult == GlimmerTriangleRequestResult::Pending)
                {
                    outNumWaitingForTriangles++;
                }
            }

            if (tracedRef.nodeCount != 0)
            {
                const Mat4f tracedGridToWorld = objectToWorld * Mat4f::Translation(tracedRef.gridOrigin) * Mat4f::Scaling(tracedRef.gridScale);
                const Mat4f worldToGrid = tracedGridToWorld.Inverse();

                GlimmerTracedInstanceShaderData& traced = outInput.tracedInstances.EmplaceBack();
                Memory::Copy(traced.worldToObject, worldToGrid.values, sizeof(traced.worldToObject));

                traced.blasNodeBase = tracedRef.nodeBase;
                traced.blasTriangleBase = tracedRef.triangleBase;
                traced.materialIndex = materialIndex;
                traced.flags = instanceFlags;

                outInput.tracedBounds.PushBack(objectToWorld * tracedRef.localBounds);

                if (tracedRef.key != triangleRef.key && meshKeysSeen.Insert(tracedRef.key).second)
                {
                    outInput.meshKeys.PushBack(tracedRef.key);
                }
            }
        }

        if (meshKeysSeen.Insert(triangleRef.key).second)
        {
            outInput.meshKeys.PushBack(triangleRef.key);
        }

        // What we hash
        struct
        {
            uint64 mesh;
            uint64 materialHash;
            uint32 materialIndex;
            uint32 flags;
            uint32 lightmapRectOffset;
            uint32 lightmapRectSize;
        } content;

        content.mesh = uint64(uintptr_t(proxy.mesh));
        content.materialHash = materialHash;
        content.materialIndex = materialIndex;
        content.flags = instanceFlags;
        content.lightmapRectOffset = lightmapRectOffset;
        content.lightmapRectSize = lightmapRectSize;

        InstanceRecord& record = outInput.instanceRecords.EmplaceBack();
        record.identity = instanceKey;
        
        record.worldHash = FNV1::DoHashWords(
            objectToWorld.values,
            sizeof(objectToWorld.values),
            FNV1::DoHashWords(&content, sizeof(content), 0));

        record.geometryKey = triangleRef.key;
        record.bounds = worldBounds;
    };

    for (Entity* entity : rpl.GetMeshEntities())
    {
        RenderProxyMesh* proxy = rpl.GetMeshEntities().GetProxy(entity->Id());

        if (!proxy || !proxy->mesh || !proxy->material || proxy->skeleton != nullptr)
        {
            continue;
        }

        const MaterialAttributes& materialAttributes = proxy->attributes.GetMaterialAttributes();

        if (materialAttributes.bucket != RenderBucket::Opaque
            && materialAttributes.bucket != RenderBucket::Lightmapped)
        {
            continue;
        }

        // terrain comes from the terrain itself, and foliage only goes into the heightfield, as canopy

        if (RenderHelpers::IsTerrainShader(materialAttributes.shaderName))
        {
            continue;
        }

        uint32 flags = proxy->material->GetParameters().foliage ? GIF_FOLIAGE : GIF_NONE;

        if (materialAttributes.cullFaces == FaceCullMode::None)
        {
            flags |= GIF_DOUBLE_SIDED;
        }

        if (materialAttributes.flags & MaterialAttributeFlags::AlphaDiscard)
        {
            flags |= GIF_ALPHA_TESTED;
        }

        const uint32 materialIndex = Resources::GetBinding(proxy->material);

        materialHash = GlimmerHelpers::GetMaterialHash(rpl, *proxy->material);
        
        const Mat4f& modelMatrix = proxy->bufferData.modelMatrix;

        uint32 lightmapRectOffset = 0;
        uint32 lightmapRectSize = 0;

        if (m_usesLightmaps
            && materialAttributes.bucket == RenderBucket::Lightmapped
            && proxy->lightmapStencilValue != 0
            && proxy->lightmapVolume != nullptr)
        {
            const bool isEmissive = RenderHelpers::IsEmissive(proxy->material->GetParameters());

            const RenderProxyLightmapVolume* volumeProxy = static_cast<RenderProxyLightmapVolume*>(GetRenderProxy(proxy->lightmapVolume));

            if (!isEmissive && volumeProxy && volumeProxy->stencilBase != 0 && proxy->lightmapStencilValue >= volumeProxy->stencilBase)
            {
                const uint32 atlasIndex = uint32(proxy->lightmapStencilValue - volumeProxy->stencilBase);

                if (atlasIndex < volumeProxy->numAtlases && volumeProxy->atlasIrradianceTextures[atlasIndex] != nullptr)
                {
                    lightmapRectOffset = proxy->bufferData.lightmapRectOffset;
                    lightmapRectSize = proxy->bufferData.lightmapRectSize;
                }
            }
        }

        if (proxy->numInstances == 0)
        {
            addInstance(*proxy, modelMatrix, flags, materialIndex, 0, lightmapRectOffset, lightmapRectSize);

            continue;
        }

        const InstanceData& instanceData = proxy->instanceData;

        // buffer 0 is always the instance transform, relative to the entity
        if (instanceData.bufferStructSizes[0] != sizeof(Mat4f)
            || instanceData.buffers[0].Size() < proxy->numInstances * sizeof(Mat4f))
        {
            continue;
        }

        const ubyte* instanceTransforms = instanceData.buffers[0].Data();

        for (uint32 instanceIndex = 0; instanceIndex < proxy->numInstances; instanceIndex++)
        {
            Mat4f instanceTransform;
            Memory::Copy(instanceTransform.values, instanceTransforms + size_t(instanceIndex) * sizeof(Mat4f), sizeof(Mat4f));

            // instances are told apart by where they stand, as painting or streaming foliage reorders them
            const uint32 instanceId = uint32(FNV1::DoHashWords(instanceTransform.values, sizeof(instanceTransform.values), 0));

            addInstance(*proxy, modelMatrix * instanceTransform, flags, materialIndex, instanceId, lightmapRectOffset, lightmapRectSize);
        }
    }

    m_instanceLods = std::move(instanceLods);
}

GlimmerSceneInstances::BuildResult GlimmerSceneInstances::Build(BuildInput&& input)
{
    HYP_SCOPE;

    const uint64 startTime = PerformanceClock::Now();

    BuildResult result;

    result.region = input.region;

    result.inputHash = FNV1::DoHashWords(input.spanInstances.Data(), input.spanInstances.ByteSize(), 0);
    result.inputHash = FNV1::DoHashWords(input.tracedInstances.Data(), input.tracedInstances.ByteSize(), result.inputHash);
    result.inputHash = FNV1::DoHashWords(input.spanMaterialHashes.Data(), input.spanMaterialHashes.ByteSize(), result.inputHash);

    result.meshKeys = std::move(input.meshKeys);

    result.spanInstances = std::move(input.spanInstances);

    for (size_t spanIndex = 0; spanIndex < result.spanInstances.Size(); spanIndex++)
    {
        const uint32 triangleCount = result.spanInstances[spanIndex].triangleCount;
        const BoundingBox& bounds = input.spanInstanceBounds[spanIndex];

        for (uint32 firstTriangle = 0; firstTriangle < triangleCount; firstTriangle += GlimmerSpanChunkTriangles)
        {
            GlimmerSpanChunkShaderData& chunk = result.spanChunks.EmplaceBack();

            Memory::Copy(chunk.boundsMin, &bounds.min.x, sizeof(chunk.boundsMin));
            chunk.spanInstance = uint32(spanIndex);
            Memory::Copy(chunk.boundsMax, &bounds.max.x, sizeof(chunk.boundsMax));
            chunk.firstTriangle = firstTriangle;
        }

        result.numSpanTriangles += triangleCount;
    }

    result.spanKeys.Reserve(result.spanInstances.Size());

    for (size_t spanIndex = 0; spanIndex < result.spanInstances.Size(); spanIndex++)
    {
        result.spanKeys.PushBack(SpanKey {
            FNV1::DoHashWords(
                &result.spanInstances[spanIndex],
                sizeof(GlimmerSpanInstanceShaderData),
                input.spanMaterialHashes[spanIndex]),
            input.spanInstanceBounds[spanIndex]
        });
    }

    std::sort(result.spanKeys.Begin(), result.spanKeys.End());

    result.spanFullyDirty = !input.hasPrevious;

    if (input.hasPrevious)
    {
        BoundingBox quadrants[GlimmerSpanMaxDirtyBounds];

        const auto addChanged = [&](const BoundingBox& bounds)
        {
            const Vec3f center = bounds.GetCenter();
            const uint32 quadrant = (center.x >= input.regionCenter.x ? 1u : 0u) | (center.z >= input.regionCenter.z ? 2u : 0u);

            quadrants[quadrant] = quadrants[quadrant].Union(bounds);
        };

        const Array<SpanKey, GlimmerAllocator>& previousKeys = input.previousSpanKeys;
        const Array<SpanKey, GlimmerAllocator>& currentKeys = result.spanKeys;

        size_t previousIndex = 0;
        size_t currentIndex = 0;

        while (previousIndex < previousKeys.Size() || currentIndex < currentKeys.Size())
        {
            if (currentIndex == currentKeys.Size()
                || (previousIndex < previousKeys.Size() && previousKeys[previousIndex].hash < currentKeys[currentIndex].hash))
            {
                addChanged(previousKeys[previousIndex++].bounds);
            }
            else if (previousIndex == previousKeys.Size()
                || currentKeys[currentIndex].hash < previousKeys[previousIndex].hash)
            {
                addChanged(currentKeys[currentIndex++].bounds);
            }
            else
            {
                previousIndex++;
                currentIndex++;
            }
        }

        for (const BoundingBox& quadrant : quadrants)
        {
            if (quadrant.IsValid())
            {
                result.spanDirtyBounds.PushBack(quadrant);
            }
        }
    }

    result.instanceRecords = std::move(input.instanceRecords);

    std::sort(result.instanceRecords.Begin(), result.instanceRecords.End());

    if (input.hasPrevious)
    {
        const Array<InstanceRecord, GlimmerAllocator>& previousRecords = input.previousInstanceRecords;
        const Array<InstanceRecord, GlimmerAllocator>& currentRecords = result.instanceRecords;

        size_t previousIndex = 0;
        size_t currentIndex = 0;

        while (previousIndex < previousRecords.Size() || currentIndex < currentRecords.Size())
        {
            if (currentIndex == currentRecords.Size()
                || (previousIndex < previousRecords.Size() && previousRecords[previousIndex].identity < currentRecords[currentIndex].identity))
            {
                result.changes.Add(GlimmerSceneChange {
                    previousRecords[previousIndex++].bounds,
                    false
                });

                continue;
            }

            if (previousIndex == previousRecords.Size()
                || currentRecords[currentIndex].identity < previousRecords[previousIndex].identity)
            {
                result.changes.Add(GlimmerSceneChange {
                    currentRecords[currentIndex++].bounds,
                    false
                });

                continue;
            }

            const InstanceRecord& previous = previousRecords[previousIndex++];
            const InstanceRecord& current = currentRecords[currentIndex++];

            if (previous.worldHash != current.worldHash)
            {
                result.changes.Add(GlimmerSceneChange {
                    previous.bounds,
                    false
                });

                result.changes.Add(GlimmerSceneChange {
                    current.bounds,
                    false
                });
            }
            else if (previous.geometryKey != current.geometryKey)
            {
                result.changes.Add(GlimmerSceneChange {
                    previous.bounds.Union(current.bounds),
                    true
                });

                result.numLodOnlyChanges++;
            }
        }
    }

    result.tlasHash = FNV1::DoHashWords(input.tracedInstances.Data(), input.tracedInstances.ByteSize(), 0);
    result.isTLASUnchanged = result.tlasHash == input.previousTLASHash;

    if (input.tracedInstances.Any() && !result.isTLASUnchanged)
    {
        SAHBVHBuildParams params;
        params.minLeafSize = 1;
        params.maxLeafSize = 1;
        params.traversalCost = 0.25f;

        Array<uint32, GlimmerAllocator> instanceOrder;
        SAHBVHBuilder::Build(input.tracedBounds.ToSpan(), params, result.tlasNodes, instanceOrder);

        result.tracedInstances.Resize(instanceOrder.Size());

        for (size_t orderIndex = 0; orderIndex < instanceOrder.Size(); orderIndex++)
        {
            result.tracedInstances[orderIndex] = input.tracedInstances[instanceOrder[orderIndex]];
        }

        result.tlasDepth = SAHBVHBuilder::CalculateDepth(result.tlasNodes.ToSpan());
    }

    result.buildMs = PerformanceClock::TimeSince(startTime);

    return result;
}

void GlimmerSceneInstances::Upload(Frame* frame, BuildResult& result)
{
    HYP_SCOPE;

    CommandRecorder& cr = frame->cr;

    StructuredBuffer spanInstancesBuffer = GlimmerHelpers::CreateGlimmerStructuredBuffer(
        sizeof(GlimmerSpanInstanceShaderData),
        result.spanInstances.Size());

    StructuredBuffer spanChunksBuffer = GlimmerHelpers::CreateGlimmerStructuredBuffer(
        sizeof(GlimmerSpanChunkShaderData),
        result.spanChunks.Size());

    const size_t spanInstancesByteSize = result.spanInstances.ByteSize();
    const size_t spanChunksByteSize = result.spanChunks.ByteSize();
    const size_t totalByteSize = spanInstancesByteSize + spanChunksByteSize;

    if (totalByteSize != 0)
    {
        GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(totalByteSize);
        Assert(stagingBuffer != nullptr);

        stagingBuffer->Copy(0, spanInstancesByteSize, result.spanInstances.Data());
        stagingBuffer->Copy(spanInstancesByteSize, spanChunksByteSize, result.spanChunks.Data());
        stagingBuffer->Flush(0, totalByteSize);

        cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);
        cr << InsertBarrier(spanInstancesBuffer, ResourceState::CopyDst);
        cr << InsertBarrier(spanChunksBuffer, ResourceState::CopyDst);

        size_t copyOffset = 0;

        const auto copyInto = [&](const StructuredBuffer& buffer, size_t byteSize)
        {
            if (byteSize != 0)
            {
                cr << CopyBuffer(stagingBuffer, buffer.gpuBuffer, uint32(copyOffset), 0, uint32(byteSize));
            }

            copyOffset += byteSize;
        };

        copyInto(spanInstancesBuffer, spanInstancesByteSize);
        copyInto(spanChunksBuffer, spanChunksByteSize);
    }

    cr << InsertBarrier(spanInstancesBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(spanChunksBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);

    m_spanInstancesBuffer = std::move(spanInstancesBuffer);
    m_spanChunksBuffer = std::move(spanChunksBuffer);

    if (!result.isTLASUnchanged)
    {
        UploadTLAS(frame, result);
    }
}

void GlimmerSceneInstances::UploadTLAS(Frame* frame, BuildResult& result)
{
    CommandRecorder& cr = frame->cr;

    m_numTracedInstances = uint32(result.tracedInstances.Size());
    m_tlasDepth = result.tlasDepth;
    m_activeTLASHash = result.tlasHash;
    m_stats.numTLASBuilds++;

    // a single placeholder element keeps the buffers bindable without a TLAS
    StructuredBuffer nodesBuffer = GlimmerHelpers::CreateGlimmerStructuredBuffer(
        sizeof(SAHBVHNode),
        MathUtil::Max(result.tlasNodes.Size(), size_t(1)));

    StructuredBuffer instancesBuffer = GlimmerHelpers::CreateGlimmerStructuredBuffer(
        sizeof(GlimmerTracedInstanceShaderData),
        MathUtil::Max(result.tracedInstances.Size(), size_t(1)));

    const size_t nodesByteSize = result.tlasNodes.ByteSize();
    const size_t instancesByteSize = result.tracedInstances.ByteSize();

    if (nodesByteSize + instancesByteSize != 0)
    {
        GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(nodesByteSize + instancesByteSize);
        Assert(stagingBuffer != nullptr);

        stagingBuffer->Copy(0, nodesByteSize, result.tlasNodes.Data());
        stagingBuffer->Copy(nodesByteSize, instancesByteSize, result.tracedInstances.Data());
        stagingBuffer->Flush(0, nodesByteSize + instancesByteSize);

        cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);
        cr << InsertBarrier(nodesBuffer, ResourceState::CopyDst);
        cr << InsertBarrier(instancesBuffer, ResourceState::CopyDst);

        cr << CopyBuffer(stagingBuffer, nodesBuffer.gpuBuffer, 0, 0, uint32(nodesByteSize));
        cr << CopyBuffer(stagingBuffer, instancesBuffer.gpuBuffer, uint32(nodesByteSize), 0, uint32(instancesByteSize));
    }

    cr << InsertBarrier(nodesBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(instancesBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);

    m_tlasNodesBuffer = std::move(nodesBuffer);
    m_tracedInstancesBuffer = std::move(instancesBuffer);
}

void GlimmerSceneInstances::Update(
    Frame* frame,
    RenderProxyList& rpl,
    const BoundingBox& region,
    const Vec3f& viewerPosition,
    GlimmerTriangleCache& triangleCache)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (rpl.GetMeshEntities().GetDiff().NeedsUpdate()
        || rpl.GetMaterials().GetDiff().NeedsUpdate()
        || region != m_lastRegion)
    {
        m_dirty = true;
    }

    if (rpl.GetLightmapVolumes().GetDiff().NeedsUpdate()
        || g_cvLightmapVolumes.Get() != m_usesLightmaps)
    {
        m_dirty = true;
    }

    if (m_waitingForTriangles && triangleCache.GetResidentGeneration() != m_triangleGenerationAtGather)
    {
        m_dirty = true;
    }

    const float viewerMoved = viewerPosition.Distance(m_viewerPositionAtGather);

    if ((m_waitingForTriangles && viewerMoved > ViewerMoveRegatherDistance)
        || viewerMoved > ViewerMoveLodRegatherDistance)
    {
        m_dirty = true;
    }

    if (triangleCache.GetEvictionGeneration() != m_triangleEvictionGenerationAtGather)
    {
        m_dirty = true;
    }

    if (m_buildTask.IsValid() && m_buildTask.IsCompleted())
    {
        BuildResult result = std::move(m_buildTask).Await();

        m_buildTask = Task<BuildResult>();

        if (IsReady() && result.inputHash == m_activeInputHash)
        {
            triangleCache.RemoveReferences(result.meshKeys.ToSpan());
            m_pendingMeshKeys.Clear();

            m_activeRegion = result.region;
        }
        else
        {
            m_activeInputHash = result.inputHash;
            m_activeRegion = result.region;

            m_spanKeys = std::move(result.spanKeys);
            m_spanDirtyBounds = std::move(result.spanDirtyBounds);
            m_spanFullyDirty = result.spanFullyDirty;

            Upload(frame, result);

            triangleCache.RemoveReferences(m_activeMeshKeys.ToSpan());

            m_activeMeshKeys = std::move(result.meshKeys);
            m_pendingMeshKeys.Clear();

            m_stats.lastBuildMs = result.buildMs;
            m_stats.numSpanInstances = uint32(result.spanInstances.Size());
            m_stats.numSpanTriangles = result.numSpanTriangles;
            m_stats.numSpanChunks = uint32(result.spanChunks.Size());
            m_stats.numBuilds++;

            m_stats.numChanges = uint32(result.changes.Size());
            m_stats.numLodOnlyChanges = result.numLodOnlyChanges;

            ChangeSet changeSet;
            changeSet.generation = m_stats.numBuilds;
            changeSet.isEverywhere = result.spanFullyDirty;

            if (changeSet.isEverywhere)
            {
                m_stats.numEverywhereBuilds++;
            }
            else
            {
                result.changes.Quantize(MaxChangesPerGeneration);

                changeSet.changes = std::move(result.changes);
            }

            if (m_changeJournal.Size() >= ChangeJournalGenerations)
            {
                m_changeJournal.PopFront();
            }

            m_changeJournal.PushBack(std::move(changeSet));

            m_instanceRecords = std::move(result.instanceRecords);
        }
    }

    if (!m_dirty || m_buildTask.IsValid())
    {
        return;
    }

    if (IsReady() && m_lastBuildStartTime != 0 && PerformanceClock::TimeSince(m_lastBuildStartTime) < RebuildDebounceMs)
    {
        return;
    }

    BuildInput input;
    uint32 numWaitingForTriangles = 0;

    Gather(rpl, region, viewerPosition, triangleCache, input, numWaitingForTriangles);

    input.region = region;
    input.regionCenter = region.GetCenter();
    input.previousSpanKeys = m_spanKeys;
    input.previousInstanceRecords = m_instanceRecords;
    input.hasPrevious = IsReady() || !m_spanKeys.Empty();
    input.previousTLASHash = m_activeTLASHash;

    triangleCache.AddReferences(input.meshKeys.ToSpan());
    m_pendingMeshKeys = input.meshKeys;

    m_triangleGenerationAtGather = triangleCache.GetResidentGeneration();
    m_triangleEvictionGenerationAtGather = triangleCache.GetEvictionGeneration();
    m_viewerPositionAtGather = viewerPosition;
    m_waitingForTriangles = numWaitingForTriangles != 0;
    m_stats.numWaitingForTriangles = numWaitingForTriangles;

    m_lastRegion = region;
    m_lastBuildStartTime = PerformanceClock::Now();
    m_dirty = false;

    m_buildTask = triangleCache.GetBuildPool().Enqueue(
        [input = std::move(input)]() mutable -> BuildResult
        {
            return Build(std::move(input));
        });
}
#pragma endregion GlimmerSceneInstances

} // namespace Hyperion
