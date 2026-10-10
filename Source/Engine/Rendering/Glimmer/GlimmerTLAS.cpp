/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/GlimmerBLASCache.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

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

static constexpr uint32 MaxInstances = 131072;
static constexpr uint32 MaxSpanInstances = 262144;

static constexpr float SolidLodErrorMeters = 0.05f;
static constexpr float FoliageLodErrorMeters = 1.0f;
static constexpr float LodErrorPerMeter = 0.02f;
static constexpr float LodHysteresis = 0.25f;

static constexpr double RebuildDebounceMs = 250.0;

static constexpr float ViewerMoveRegatherDistance = 4.0f;
static constexpr float ViewerMoveLodRegatherDistance = 16.0f;

static constexpr size_t MaxChangesPerGeneration = 64;
static constexpr size_t ChangeJournalGenerations = 8;

#pragma region GlimmerSceneChanges

void GlimmerSceneChanges::Quantize(size_t maxChanges)
{
    if (changes.Size() <= maxChanges || maxChanges == 0)
    {
        return;
    }

    BoundingBox extent;

    for (const GlimmerSceneChange& change : changes)
    {
        extent = extent.Union(change.bounds);
    }

    const Vec3f extentSize = MathUtil::Max(extent.max - extent.min, Vec3f(1e-3f));

    const auto spreadBits = [](uint32 value) -> uint64
    {
        uint64 spread = value & 0x1FFFFFu;
        spread = (spread | (spread << 32)) & 0x1F00000000FFFFull;
        spread = (spread | (spread << 16)) & 0x1F0000FF0000FFull;
        spread = (spread | (spread << 8)) & 0x100F00F00F00F00Full;
        spread = (spread | (spread << 4)) & 0x10C30C30C30C30C3ull;
        spread = (spread | (spread << 2)) & 0x1249249249249249ull;

        return spread;
    };

    struct Keyed
    {
        uint64 key;
        uint32 index;
    };

    Array<Keyed> order;
    order.Reserve(changes.Size());

    size_t numLodOnly = 0;

    for (size_t changeIndex = 0; changeIndex < changes.Size(); changeIndex++)
    {
        const GlimmerSceneChange& change = changes[changeIndex];
        const Vec3f normalized = (change.bounds.GetCenter() - extent.min) / extentSize;

        const uint64 morton = spreadBits(uint32(MathUtil::Clamp(normalized.x, 0.0f, 1.0f) * 1023.0f))
            | (spreadBits(uint32(MathUtil::Clamp(normalized.y, 0.0f, 1.0f) * 1023.0f)) << 1)
            | (spreadBits(uint32(MathUtil::Clamp(normalized.z, 0.0f, 1.0f) * 1023.0f)) << 2);

        // LOD only changes sort after the rest, so the two never share a box
        order.PushBack(Keyed { morton | (change.isLodOnly ? (1ull << 63) : 0ull), uint32(changeIndex) });

        numLodOnly += change.isLodOnly ? 1 : 0;
    }

    std::sort(order.Begin(), order.End(), [](const Keyed& a, const Keyed& b)
        {
            return a.key < b.key;
        });

    const size_t numOther = changes.Size() - numLodOnly;

    size_t lodOnlyBoxes = numLodOnly != 0 ? MathUtil::Max(maxChanges / 4, size_t(1)) : 0;
    size_t otherBoxes = numOther != 0 ? MathUtil::Max(maxChanges - lodOnlyBoxes, size_t(1)) : 0;

    if (numOther == 0)
    {
        lodOnlyBoxes = maxChanges;
    }

    Array<GlimmerSceneChange> merged;
    merged.Reserve(maxChanges);

    const auto mergeRange = [&](size_t first, size_t count, size_t numBoxes)
    {
        if (count == 0 || numBoxes == 0)
        {
            return;
        }

        const size_t perBox = (count + numBoxes - 1) / numBoxes;

        for (size_t start = 0; start < count; start += perBox)
        {
            GlimmerSceneChange box = changes[order[first + start].index];

            for (size_t offset = 1; offset < perBox && start + offset < count; offset++)
            {
                box.bounds = box.bounds.Union(changes[order[first + start + offset].index].bounds);
            }

            merged.PushBack(box);
        }
    };

    mergeRange(0, numOther, otherBoxes);
    mergeRange(numOther, numLodOnly, lodOnlyBoxes);

    changes = std::move(merged);
}

#pragma endregion GlimmerSceneChanges

#pragma region GlimmerTLAS

GlimmerTLAS::GlimmerTLAS()
    : m_lastBuildStartTime(0),
      m_blasGenerationAtGather(~0u),
      m_blasEvictionGenerationAtGather(0),
      m_activeInputHash(0),
      m_spanFullyDirty(true),
      m_dirty(true),
      m_waitingForBLAS(false),
      m_usesLightmaps(false)
{
}

GlimmerTLAS::~GlimmerTLAS()
{
    AssertDebug(m_activeBlasKeys.Empty() && m_pendingBlasKeys.Empty());

    if (m_buildTask.IsValid() && !m_buildTask.IsCompleted())
    {
        m_buildTask.Await();
    }

    EnqueueDeletion(std::move(m_nodesBuffer));
    EnqueueDeletion(std::move(m_instancesBuffer));
    EnqueueDeletion(std::move(m_instanceBoundsBuffer));
    EnqueueDeletion(std::move(m_spanInstancesBuffer));
    EnqueueDeletion(std::move(m_spanChunksBuffer));
}

void GlimmerTLAS::Release(GlimmerBLASCache& blasCache)
{
    if (m_buildTask.IsValid() && !m_buildTask.IsCompleted())
    {
        m_buildTask.Await();
    }

    m_buildTask = Task<BuildResult>();

    blasCache.RemoveReferences(m_activeBlasKeys.ToSpan());
    blasCache.RemoveReferences(m_pendingBlasKeys.ToSpan());

    m_activeBlasKeys.Clear();
    m_pendingBlasKeys.Clear();
}

bool GlimmerTLAS::GetChangesSince(
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

void GlimmerTLAS::Gather(RenderProxyList& rpl, const BoundingBox& region, const BoundingBox& tracedRegion, const Vec3f& viewerPosition, GlimmerBLASCache& blasCache, BuildInput& outInput, uint32& outNumWaitingForBLAS)
{
    HYP_SCOPE;

    outNumWaitingForBLAS = 0;

    Set<uint64> blasKeysSeen;

    const float lodErrorScale = blasCache.GetLodErrorScale();

    Map<uint64, uint8> instanceLods;

    m_usesLightmaps = g_cvLightmapVolumes.Get();

    // the rect offset is 0 for a mesh that isn't lit by a lightmap
    const auto addInstance = [&](const RenderProxyMesh& proxy, const Mat4f& objectToWorld, uint32 flags, uint32 materialIndex, uint32 instanceId, uint32 lightmapRectOffset, uint32 lightmapRectSize)
    {
        const bool isFoliage = (flags & GIF_FOLIAGE) != 0;

        const bool wantsSpan = outInput.spanInstances.Size() < MaxSpanInstances;
        const bool wantsTraced = !isFoliage && outInput.instances.Size() < MaxInstances;

        if (!wantsSpan && !wantsTraced)
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

            const Vec3f nearestPoint = Vec3f(
                MathUtil::Clamp(viewerPosition.x, estimatedBounds.min.x, estimatedBounds.max.x),
                MathUtil::Clamp(viewerPosition.y, estimatedBounds.min.y, estimatedBounds.max.y),
                MathUtil::Clamp(viewerPosition.z, estimatedBounds.min.z, estimatedBounds.max.z));

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

        GlimmerBLASRef blasRef;

        const GlimmerBLASRequestResult requestResult = blasCache.Request(proxy.mesh, lodIndex, priority, blasRef);

        if (requestResult != GlimmerBLASRequestResult::Resident)
        {
            if (requestResult == GlimmerBLASRequestResult::Pending)
            {
                outNumWaitingForBLAS++;
            }

            bool hasFallback = previousLod != lodIndex && blasCache.TryGetResident(proxy.mesh, previousLod, priority, blasRef);

            for (uint8 distance = 1; !hasFallback && distance < MaxMeshLods; distance++)
            {
                hasFallback = (lodIndex + distance < MaxMeshLods && blasCache.TryGetResident(proxy.mesh, uint8(lodIndex + distance), priority, blasRef))
                    || (lodIndex >= distance && blasCache.TryGetResident(proxy.mesh, uint8(lodIndex - distance), priority, blasRef));
            }

            if (!hasFallback)
            {
                return;
            }
        }

        const BoundingBox worldBounds = objectToWorld * blasRef.localBounds;

        if (!worldBounds.IsValid() || !worldBounds.Overlaps(region))
        {
            return;
        }

        if (isFoliage && worldBounds.max.y - worldBounds.min.y < GlimmerSpansMinFoliageHeight)
        {
            return;
        }

        const uint32 instanceFlags = flags | (objectToWorld.Determinant() < 0.0f ? GIF_MIRRORED : GIF_NONE);

        const Mat4f gridToWorld = objectToWorld * Mat4f::Translation(blasRef.gridOrigin) * Mat4f::Scaling(blasRef.gridScale);

        bool isReferenced = false;

        if (wantsSpan)
        {
            GlimmerSpanInstanceShaderData& spanInstance = outInput.spanInstances.EmplaceBack();
            Memory::Copy(spanInstance.objectToWorld, gridToWorld.values, sizeof(spanInstance.objectToWorld));

            spanInstance.blasTriangleBase = blasRef.triangleBase;
            spanInstance.triangleCount = blasRef.triangleCount;
            spanInstance.materialIndex = materialIndex;
            spanInstance.flags = instanceFlags;

            const bool hasLightmap = lightmapRectOffset != 0 && blasRef.lightmapUVBase != ~0u;

            spanInstance.lightmapUVBase = hasLightmap ? blasRef.lightmapUVBase : ~0u;
            spanInstance.lightmapRectOffset = hasLightmap ? lightmapRectOffset : 0;
            spanInstance.lightmapRectSize = hasLightmap ? lightmapRectSize : 0;
            spanInstance._pad0 = 0;

            outInput.spanInstanceBounds.PushBack(worldBounds);

            isReferenced = true;
        }

        if (wantsTraced && worldBounds.Overlaps(tracedRegion))
        {
            const Mat4f worldToGrid = gridToWorld.Inverse();

            GlimmerInstanceShaderData& instance = outInput.instances.EmplaceBack();
            Memory::Copy(instance.worldToObject, worldToGrid.values, sizeof(instance.worldToObject));

            instance.blasNodeBase = blasRef.nodeBase;
            instance.blasTriangleBase = blasRef.triangleBase;
            instance.materialIndex = materialIndex;
            instance.flags = instanceFlags;

            outInput.instanceBounds.PushBack(worldBounds);

            isReferenced = true;
        }

        if (!isReferenced)
        {
            return;
        }

        if (blasKeysSeen.Insert(blasRef.key).second)
        {
            outInput.blasKeys.PushBack(blasRef.key);
        }

        struct
        {
            uint64 mesh;
            uint32 materialIndex;
            uint32 flags;
            uint32 lightmapRectOffset;
            uint32 lightmapRectSize;
        } content = { uint64(uintptr_t(proxy.mesh)), materialIndex, instanceFlags, lightmapRectOffset, lightmapRectSize };

        InstanceRecord& record = outInput.instanceRecords.EmplaceBack();
        record.identity = instanceKey;
        record.worldHash = FNV1::DoHashWords(objectToWorld.values, sizeof(objectToWorld.values), FNV1::DoHashWords(&content, sizeof(content), 0));
        record.geometryKey = blasRef.key;
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

        if (materialAttributes.bucket != RenderBucket::Opaque && materialAttributes.bucket != RenderBucket::Lightmapped)
        {
            continue;
        }

        // terrain comes from the terrain itself, and foliage only goes into the heightfield, as canopy
        const StringHash shaderNameHash = materialAttributes.shaderName;

        if (shaderNameHash == "Terrain"_sh)
        {
            continue;
        }

        uint32 flags = proxy->material->GetParameters().foliage ? GIF_FOLIAGE : GIF_NONE;

        if (materialAttributes.cullFaces == FaceCullMode::None)
        {
            flags |= GIF_DOUBLE_SIDED;
        }

        if (materialAttributes.flags & MAF_ALPHA_DISCARD)
        {
            flags |= GIF_ALPHA_TESTED;
        }

        const uint32 materialIndex = Resources::GetBinding(proxy->material);
        const Mat4f& modelMatrix = proxy->bufferData.modelMatrix;

        uint32 lightmapRectOffset = 0;
        uint32 lightmapRectSize = 0;

        if (m_usesLightmaps
            && materialAttributes.bucket == RenderBucket::Lightmapped
            && proxy->lightmapStencilValue != 0
            && proxy->lightmapVolume != nullptr)
        {
            const MaterialParameters& materialParameters = proxy->material->GetParameters();

            const bool isEmissive = materialParameters.emissiveIntensity > 0.0f
                && (materialParameters.emissiveColor.GetRed() > 0.0f || materialParameters.emissiveColor.GetGreen() > 0.0f || materialParameters.emissiveColor.GetBlue() > 0.0f);

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
        if (instanceData.bufferStructSizes[0] != sizeof(Mat4f) || instanceData.buffers[0].Size() < proxy->numInstances * sizeof(Mat4f))
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

GlimmerTLAS::BuildResult GlimmerTLAS::Build(BuildInput&& input)
{
    HYP_SCOPE;

    const uint64 startTime = PerformanceClock::Now();

    BuildResult result;

    result.region = input.region;

    result.inputHash = FNV1::DoHashWords(
        input.instances.Data(),
        input.instances.ByteSize(),
        FNV1::DoHashWords(input.spanInstances.Data(), input.spanInstances.ByteSize(), 0));

    result.blasKeys = std::move(input.blasKeys);

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
        result.spanKeys.PushBack(SpanKey { FNV1::DoHashWords(&result.spanInstances[spanIndex], sizeof(GlimmerSpanInstanceShaderData), 0), input.spanInstanceBounds[spanIndex] });
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

        const Array<SpanKey>& previousKeys = input.previousSpanKeys;
        const Array<SpanKey>& currentKeys = result.spanKeys;

        size_t previousIndex = 0;
        size_t currentIndex = 0;

        while (previousIndex < previousKeys.Size() || currentIndex < currentKeys.Size())
        {
            if (currentIndex == currentKeys.Size() || (previousIndex < previousKeys.Size() && previousKeys[previousIndex].hash < currentKeys[currentIndex].hash))
            {
                addChanged(previousKeys[previousIndex++].bounds);
            }
            else if (previousIndex == previousKeys.Size() || currentKeys[currentIndex].hash < previousKeys[previousIndex].hash)
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
        const Array<InstanceRecord>& previousRecords = input.previousInstanceRecords;
        const Array<InstanceRecord>& currentRecords = result.instanceRecords;

        size_t previousIndex = 0;
        size_t currentIndex = 0;

        while (previousIndex < previousRecords.Size() || currentIndex < currentRecords.Size())
        {
            if (currentIndex == currentRecords.Size() || (previousIndex < previousRecords.Size() && previousRecords[previousIndex].identity < currentRecords[currentIndex].identity))
            {
                result.changes.Add(GlimmerSceneChange { previousRecords[previousIndex++].bounds, false });

                continue;
            }

            if (previousIndex == previousRecords.Size() || currentRecords[currentIndex].identity < previousRecords[previousIndex].identity)
            {
                result.changes.Add(GlimmerSceneChange { currentRecords[currentIndex++].bounds, false });

                continue;
            }

            const InstanceRecord& previous = previousRecords[previousIndex++];
            const InstanceRecord& current = currentRecords[currentIndex++];

            if (previous.worldHash != current.worldHash)
            {
                result.changes.Add(GlimmerSceneChange { previous.bounds, false });
                result.changes.Add(GlimmerSceneChange { current.bounds, false });
            }
            else if (previous.geometryKey != current.geometryKey)
            {
                result.changes.Add(GlimmerSceneChange { previous.bounds.Union(current.bounds), true });
                result.numLodOnlyChanges++;
            }
        }
    }

    if (input.instances.Empty())
    {
        result.buildMs = PerformanceClock::TimeSince(startTime);

        return result;
    }

    GlimmerBVHBuildParams params;
    params.minLeafSize = 1;
    params.maxLeafSize = 1;
    params.traversalCost = 0.25f;

    Array<uint32> instanceOrder;
    GlimmerBVHBuilder::Build(input.instanceBounds.ToSpan(), params, result.nodes, instanceOrder);

    result.instances.Resize(instanceOrder.Size());
    result.instanceBounds.Resize(instanceOrder.Size());

    for (size_t orderIndex = 0; orderIndex < instanceOrder.Size(); orderIndex++)
    {
        const uint32 instanceIndex = instanceOrder[orderIndex];

        result.instances[orderIndex] = input.instances[instanceIndex];

        const BoundingBox& bounds = input.instanceBounds[instanceIndex];
        result.instanceBounds[orderIndex] = GlimmerInstanceBoundsShaderData { Vec4f(bounds.min, 0.0f), Vec4f(bounds.max, 0.0f) };
    }

    result.depth = GlimmerBVHBuilder::CalculateDepth(result.nodes.ToSpan());
    result.buildMs = PerformanceClock::TimeSince(startTime);

    return result;
}

void GlimmerTLAS::Upload(Frame* frame, BuildResult& result)
{
    HYP_SCOPE;

    CommandRecorder& cr = frame->cr;

    GpuBufferRef nodesBuffer = CreateGlimmerStructuredBuffer(sizeof(GlimmerBVHNode), result.nodes.Size());
    GpuBufferRef instancesBuffer = CreateGlimmerStructuredBuffer(sizeof(GlimmerInstanceShaderData), result.instances.Size());
    GpuBufferRef instanceBoundsBuffer = CreateGlimmerStructuredBuffer(sizeof(GlimmerInstanceBoundsShaderData), result.instanceBounds.Size());
    GpuBufferRef spanInstancesBuffer = CreateGlimmerStructuredBuffer(sizeof(GlimmerSpanInstanceShaderData), result.spanInstances.Size());
    GpuBufferRef spanChunksBuffer = CreateGlimmerStructuredBuffer(sizeof(GlimmerSpanChunkShaderData), result.spanChunks.Size());

    const size_t nodesByteSize = result.nodes.ByteSize();
    const size_t instancesByteSize = result.instances.ByteSize();
    const size_t instanceBoundsByteSize = result.instanceBounds.ByteSize();
    const size_t spanInstancesByteSize = result.spanInstances.ByteSize();
    const size_t spanChunksByteSize = result.spanChunks.ByteSize();
    const size_t totalByteSize = nodesByteSize + instancesByteSize + instanceBoundsByteSize + spanInstancesByteSize + spanChunksByteSize;

    if (totalByteSize != 0)
    {
        GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(totalByteSize);
        Assert(stagingBuffer != nullptr);

        stagingBuffer->Copy(0, nodesByteSize, result.nodes.Data());
        stagingBuffer->Copy(nodesByteSize, instancesByteSize, result.instances.Data());
        stagingBuffer->Copy(nodesByteSize + instancesByteSize, instanceBoundsByteSize, result.instanceBounds.Data());
        stagingBuffer->Copy(nodesByteSize + instancesByteSize + instanceBoundsByteSize, spanInstancesByteSize, result.spanInstances.Data());
        stagingBuffer->Copy(nodesByteSize + instancesByteSize + instanceBoundsByteSize + spanInstancesByteSize, spanChunksByteSize, result.spanChunks.Data());
        stagingBuffer->Flush(0, totalByteSize);

        cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);
        cr << InsertBarrier(nodesBuffer.Get(), ResourceState::CopyDst);
        cr << InsertBarrier(instancesBuffer.Get(), ResourceState::CopyDst);
        cr << InsertBarrier(instanceBoundsBuffer.Get(), ResourceState::CopyDst);
        cr << InsertBarrier(spanInstancesBuffer.Get(), ResourceState::CopyDst);
        cr << InsertBarrier(spanChunksBuffer.Get(), ResourceState::CopyDst);

        size_t copyOffset = 0;

        const auto copyInto = [&](const GpuBufferRef& buffer, size_t byteSize)
        {
            if (byteSize != 0)
            {
                cr << CopyBuffer(stagingBuffer, buffer.Get(), uint32(copyOffset), 0, uint32(byteSize));
            }

            copyOffset += byteSize;
        };

        copyInto(nodesBuffer, nodesByteSize);
        copyInto(instancesBuffer, instancesByteSize);
        copyInto(instanceBoundsBuffer, instanceBoundsByteSize);
        copyInto(spanInstancesBuffer, spanInstancesByteSize);
        copyInto(spanChunksBuffer, spanChunksByteSize);
    }

    cr << InsertBarrier(nodesBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(instancesBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(instanceBoundsBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(spanInstancesBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(spanChunksBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);

#ifdef HYP_RHI_DEBUG_NAMES
    nodesBuffer->SetDebugName(NAME("GlimmerTLASNodes"));
    instancesBuffer->SetDebugName(NAME("GlimmerTLASInstances"));
    instanceBoundsBuffer->SetDebugName(NAME("GlimmerTLASInstanceBounds"));
#endif

    EnqueueDeletion(std::move(m_nodesBuffer));
    EnqueueDeletion(std::move(m_instancesBuffer));
    EnqueueDeletion(std::move(m_instanceBoundsBuffer));

    m_nodesBuffer = std::move(nodesBuffer);
    m_instancesBuffer = std::move(instancesBuffer);
    m_instanceBoundsBuffer = std::move(instanceBoundsBuffer);

    EnqueueDeletion(std::move(m_spanInstancesBuffer));
    EnqueueDeletion(std::move(m_spanChunksBuffer));

    m_spanInstancesBuffer = std::move(spanInstancesBuffer);
    m_spanChunksBuffer = std::move(spanChunksBuffer);
}

bool GlimmerTLAS::Update(Frame* frame, RenderProxyList& rpl, const BoundingBox& region, const BoundingBox& tracedRegion, const Vec3f& viewerPosition, GlimmerBLASCache& blasCache)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (rpl.GetMeshEntities().GetDiff().NeedsUpdate() || region != m_lastRegion || tracedRegion != m_lastTracedRegion)
    {
        m_dirty = true;
    }

    if (rpl.GetLightmapVolumes().GetDiff().NeedsUpdate() || g_cvLightmapVolumes.Get() != m_usesLightmaps)
    {
        m_dirty = true;
    }

    if (m_waitingForBLAS && blasCache.GetResidentGeneration() != m_blasGenerationAtGather)
    {
        m_dirty = true;
    }

    const float viewerMoved = viewerPosition.Distance(m_viewerPositionAtGather);

    if ((m_waitingForBLAS && viewerMoved > ViewerMoveRegatherDistance) || viewerMoved > ViewerMoveLodRegatherDistance)
    {
        m_dirty = true;
    }

    if (blasCache.GetEvictionGeneration() != m_blasEvictionGenerationAtGather)
    {
        m_dirty = true;
    }

    bool swapped = false;

    if (m_buildTask.IsValid() && m_buildTask.IsCompleted())
    {
        BuildResult result = std::move(m_buildTask).Await();
        m_buildTask = Task<BuildResult>();

        if (IsReady() && result.inputHash == m_activeInputHash)
        {
            blasCache.RemoveReferences(result.blasKeys.ToSpan());
            m_pendingBlasKeys.Clear();

            if (result.region != m_activeRegion)
            {
                m_activeRegion = result.region;
                swapped = true;
            }
        }
        else
        {
            m_activeInputHash = result.inputHash;
            m_activeRegion = result.region;

            m_spanKeys = std::move(result.spanKeys);
            m_spanDirtyBounds = std::move(result.spanDirtyBounds);
            m_spanFullyDirty = result.spanFullyDirty;

            Upload(frame, result);

            blasCache.RemoveReferences(m_activeBlasKeys.ToSpan());

            m_activeBlasKeys = std::move(result.blasKeys);
            m_pendingBlasKeys.Clear();

            m_stats.numInstances = uint32(result.instances.Size());
            m_stats.numNodes = uint32(result.nodes.Size());
            m_stats.depth = result.depth;
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

            swapped = true;
        }
    }

    if (!m_dirty || m_buildTask.IsValid())
    {
        return swapped;
    }

    if (IsReady() && m_lastBuildStartTime != 0 && PerformanceClock::TimeSince(m_lastBuildStartTime) < RebuildDebounceMs)
    {
        return swapped;
    }

    BuildInput input;
    uint32 numWaitingForBLAS = 0;

    Gather(rpl, region, tracedRegion, viewerPosition, blasCache, input, numWaitingForBLAS);

    input.region = region;
    input.regionCenter = region.GetCenter();
    input.previousSpanKeys = m_spanKeys;
    input.previousInstanceRecords = m_instanceRecords;
    input.hasPrevious = IsReady() || !m_spanKeys.Empty();

    blasCache.AddReferences(input.blasKeys.ToSpan());
    m_pendingBlasKeys = input.blasKeys;

    m_blasGenerationAtGather = blasCache.GetResidentGeneration();
    m_blasEvictionGenerationAtGather = blasCache.GetEvictionGeneration();
    m_viewerPositionAtGather = viewerPosition;
    m_waitingForBLAS = numWaitingForBLAS != 0;
    m_stats.numWaitingForBLAS = numWaitingForBLAS;

    m_lastRegion = region;
    m_lastTracedRegion = tracedRegion;
    m_lastBuildStartTime = PerformanceClock::Now();
    m_dirty = false;

    m_buildTask = blasCache.GetBuildPool().Enqueue(
        [input = std::move(input)]() mutable -> BuildResult
        {
            return Build(std::move(input));
        });

    return swapped;
}
#pragma endregion GlimmerTLAS

} // namespace Hyperion
