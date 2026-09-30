/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerTLAS.hpp>
#include <Rendering/Glimmer/GlimmerBLASCache.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>

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

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/Threads.hpp>

#include <Core/Containers/Set.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/PerformanceClock.hpp>
#include <Core/Profiling/ProfileScope.hpp>

#include <algorithm>

namespace Hyperion {

static constexpr uint32 MaxInstances = 131072;
static constexpr uint32 MaxSpanInstances = 262144;

static constexpr float SolidLodErrorMeters = 0.25f;

// canopy only needs its extent and density, so leaves can come from much coarser LODs
static constexpr float FoliageLodErrorMeters = 1.0f;

static constexpr double RebuildDebounceMs = 250.0;

static float GetMaxAxisScale(const Mat4f& transform)
{
    float maxScaleSquared = 0.0f;

    for (uint32 column = 0; column < 3; column++)
    {
        const Vec3f axis = Vec3f(transform.values[column], transform.values[4 + column], transform.values[8 + column]);
        maxScaleSquared = MathUtil::Max(maxScaleSquared, axis.LengthSquared());
    }

    return MathUtil::Sqrt(maxScaleSquared);
}

static uint8 SelectBLASLod(const Mesh& mesh, float worldScale, float maxErrorMeters)
{
    const MeshDesc& meshDesc = mesh.GetMeshDesc();
    const uint8 numLods = meshDesc.GetNumLods();

    for (uint8 lodIndex = numLods; lodIndex > 1; lodIndex--)
    {
        if (meshDesc.lods[lodIndex - 1].geometricError * worldScale <= maxErrorMeters)
        {
            return lodIndex - 1;
        }
    }

    return 0;
}

static GpuBufferRef CreateStructuredBuffer(size_t elementSize, size_t numElements)
{
    GpuBufferRef buffer = RI.MakeGpuBuffer(GpuBufferType::StructuredBuffer, elementSize * MathUtil::Max(numElements, size_t(1)), alignof(Vec4f));
    Check(buffer->Create());

    return buffer;
}

GlimmerTLAS::GlimmerTLAS()
    : m_lastBuildStartTime(0),
      m_blasGenerationAtGather(~0u),
      m_dirty(true),
      m_waitingForBLAS(false)
{
}

GlimmerTLAS::~GlimmerTLAS()
{
    // references must be dropped through Release(); the cache can't be reached from here
    AssertDebug(m_activeBlasKeys.Empty() && m_pendingBlasKeys.Empty());

    if (m_buildTask.IsValid() && !m_buildTask.IsCompleted())
    {
        m_buildTask.Await();
    }

    EnqueueDeletion(std::move(m_nodesBuffer));
    EnqueueDeletion(std::move(m_instancesBuffer));
    EnqueueDeletion(std::move(m_instanceBoundsBuffer));
    EnqueueDeletion(std::move(m_spanInstancesBuffer));
    EnqueueDeletion(std::move(m_spanTriangleOffsetsBuffer));
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

void GlimmerTLAS::Gather(RenderProxyList& rpl, const BoundingBox& region, const BoundingBox& swrtRegion, GlimmerBLASCache& blasCache, BuildInput& outInput, uint32& outNumWaitingForBLAS)
{
    HYP_SCOPE;

    outNumWaitingForBLAS = 0;

    Set<uint64> blasKeysSeen;

    const auto addInstance = [&](const RenderProxyMesh& proxy, const Mat4f& objectToWorld, uint32 flags, uint32 materialIndex)
    {
        const bool isFoliage = (flags & GIF_FOLIAGE) != 0;

        const bool wantsSpan = outInput.spanInstances.Size() < MaxSpanInstances;
        const bool wantsSWRT = !isFoliage && outInput.instances.Size() < MaxInstances;

        if (!wantsSpan && !wantsSWRT)
        {
            return;
        }

        // reject with the mesh bounds first, so instances that won't be used never ask for a BLAS
        if (proxy.meshAabb.IsValid())
        {
            const BoundingBox estimatedBounds = objectToWorld * proxy.meshAabb;

            if (!estimatedBounds.Overlaps(region) || (isFoliage && estimatedBounds.max.y - estimatedBounds.min.y < GlimmerSpansMinFoliageHeight))
            {
                return;
            }
        }

        const uint8 lodIndex = SelectBLASLod(*proxy.mesh, GetMaxAxisScale(objectToWorld), isFoliage ? FoliageLodErrorMeters : SolidLodErrorMeters);

        GlimmerBLASRef blasRef;

        if (!blasCache.Request(proxy.mesh, lodIndex, blasRef))
        {
            outNumWaitingForBLAS++;

            return;
        }

        const BoundingBox worldBounds = objectToWorld * blasRef.localBounds;

        if (!worldBounds.IsValid() || !worldBounds.Overlaps(region))
        {
            return;
        }

        // grass and low plants are far thinner than a probe spacing; screen space and material AO cover them
        if (isFoliage && worldBounds.max.y - worldBounds.min.y < GlimmerSpansMinFoliageHeight)
        {
            return;
        }

        const uint32 instanceFlags = flags | (objectToWorld.Determinant() < 0.0f ? GIF_MIRRORED : GIF_NONE);

        bool isReferenced = false;

        if (wantsSpan)
        {
            GlimmerSpanInstanceShaderData& spanInstance = outInput.spanInstances.EmplaceBack();
            Memory::Copy(spanInstance.objectToWorld, objectToWorld.values, sizeof(spanInstance.objectToWorld));

            spanInstance.blasTriangleBase = blasRef.triangleBase;
            spanInstance.triangleCount = blasRef.triangleCount;
            spanInstance.materialIndex = materialIndex;
            spanInstance.flags = instanceFlags;

            isReferenced = true;
        }

        if (wantsSWRT && worldBounds.Overlaps(swrtRegion))
        {
            const Mat4f worldToObject = objectToWorld.Inverse();

            GlimmerInstanceShaderData& instance = outInput.instances.EmplaceBack();
            Memory::Copy(instance.worldToObject, worldToObject.values, sizeof(instance.worldToObject));

            instance.blasNodeBase = blasRef.nodeBase;
            instance.blasTriangleBase = blasRef.triangleBase;
            instance.materialIndex = materialIndex;
            instance.flags = instanceFlags;

            outInput.instanceBounds.PushBack(worldBounds);

            isReferenced = true;
        }

        if (isReferenced && blasKeysSeen.Insert(blasRef.key).second)
        {
            outInput.blasKeys.PushBack(blasRef.key);
        }
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

        if (proxy->numInstances == 0)
        {
            addInstance(*proxy, modelMatrix, flags, materialIndex);

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

            addInstance(*proxy, modelMatrix * instanceTransform, flags, materialIndex);
        }
    }
}

GlimmerTLAS::BuildResult GlimmerTLAS::Build(BuildInput&& input)
{
    HYP_SCOPE;

    const uint64 startTime = PerformanceClock::Now();

    BuildResult result;
    result.blasKeys = std::move(input.blasKeys);

    result.spanInstances = std::move(input.spanInstances);
    result.spanTriangleOffsets.Resize(result.spanInstances.Size());

    for (size_t spanIndex = 0; spanIndex < result.spanInstances.Size(); spanIndex++)
    {
        result.spanTriangleOffsets[spanIndex] = result.numSpanTriangles;
        result.numSpanTriangles += result.spanInstances[spanIndex].triangleCount;
    }

    if (input.instances.Empty())
    {
        result.buildMs = PerformanceClock::TimeSince(startTime);

        return result;
    }

    // entering an instance costs far more than a node visit, so split all the way down
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

    GpuBufferRef nodesBuffer = CreateStructuredBuffer(sizeof(GlimmerBVHNode), result.nodes.Size());
    GpuBufferRef instancesBuffer = CreateStructuredBuffer(sizeof(GlimmerInstanceShaderData), result.instances.Size());
    GpuBufferRef instanceBoundsBuffer = CreateStructuredBuffer(sizeof(GlimmerInstanceBoundsShaderData), result.instanceBounds.Size());
    GpuBufferRef spanInstancesBuffer = CreateStructuredBuffer(sizeof(GlimmerSpanInstanceShaderData), result.spanInstances.Size());
    GpuBufferRef spanTriangleOffsetsBuffer = CreateStructuredBuffer(sizeof(uint32), result.spanTriangleOffsets.Size());

    const size_t nodesByteSize = result.nodes.ByteSize();
    const size_t instancesByteSize = result.instances.ByteSize();
    const size_t instanceBoundsByteSize = result.instanceBounds.ByteSize();
    const size_t spanInstancesByteSize = result.spanInstances.ByteSize();
    const size_t spanTriangleOffsetsByteSize = result.spanTriangleOffsets.ByteSize();
    const size_t totalByteSize = nodesByteSize + instancesByteSize + instanceBoundsByteSize + spanInstancesByteSize + spanTriangleOffsetsByteSize;

    if (totalByteSize != 0)
    {
        GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(totalByteSize);
        Assert(stagingBuffer != nullptr);

        stagingBuffer->Copy(0, nodesByteSize, result.nodes.Data());
        stagingBuffer->Copy(nodesByteSize, instancesByteSize, result.instances.Data());
        stagingBuffer->Copy(nodesByteSize + instancesByteSize, instanceBoundsByteSize, result.instanceBounds.Data());
        stagingBuffer->Copy(nodesByteSize + instancesByteSize + instanceBoundsByteSize, spanInstancesByteSize, result.spanInstances.Data());
        stagingBuffer->Copy(nodesByteSize + instancesByteSize + instanceBoundsByteSize + spanInstancesByteSize, spanTriangleOffsetsByteSize, result.spanTriangleOffsets.Data());
        stagingBuffer->Flush(0, totalByteSize);

        cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);
        cr << InsertBarrier(nodesBuffer.Get(), ResourceState::CopyDst);
        cr << InsertBarrier(instancesBuffer.Get(), ResourceState::CopyDst);
        cr << InsertBarrier(instanceBoundsBuffer.Get(), ResourceState::CopyDst);
        cr << InsertBarrier(spanInstancesBuffer.Get(), ResourceState::CopyDst);
        cr << InsertBarrier(spanTriangleOffsetsBuffer.Get(), ResourceState::CopyDst);

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
        copyInto(spanTriangleOffsetsBuffer, spanTriangleOffsetsByteSize);
    }

    cr << InsertBarrier(nodesBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(instancesBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(instanceBoundsBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(spanInstancesBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(spanTriangleOffsetsBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);

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
    EnqueueDeletion(std::move(m_spanTriangleOffsetsBuffer));

    m_spanInstancesBuffer = std::move(spanInstancesBuffer);
    m_spanTriangleOffsetsBuffer = std::move(spanTriangleOffsetsBuffer);
}

bool GlimmerTLAS::Update(Frame* frame, RenderProxyList& rpl, const BoundingBox& region, const BoundingBox& swrtRegion, GlimmerBLASCache& blasCache)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    // the diff only lives for the frame a change syncs, so latch it
    if (rpl.GetMeshEntities().GetDiff().NeedsUpdate() || region != m_lastRegion || swrtRegion != m_lastSWRTRegion)
    {
        m_dirty = true;
    }

    if (m_waitingForBLAS && blasCache.GetResidentGeneration() != m_blasGenerationAtGather)
    {
        m_dirty = true;
    }

    bool swapped = false;

    if (m_buildTask.IsValid() && m_buildTask.IsCompleted())
    {
        BuildResult result = std::move(m_buildTask).Await();
        m_buildTask = Task<BuildResult>();

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
        m_stats.numBuilds++;

        swapped = true;
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

    Gather(rpl, region, swrtRegion, blasCache, input, numWaitingForBLAS);

    // keep the gathered BLASes where they are until this build is swapped in
    blasCache.AddReferences(input.blasKeys.ToSpan());
    m_pendingBlasKeys = input.blasKeys;

    m_blasGenerationAtGather = blasCache.GetResidentGeneration();
    m_waitingForBLAS = numWaitingForBLAS != 0;
    m_stats.numWaitingForBLAS = numWaitingForBLAS;

    m_lastRegion = region;
    m_lastSWRTRegion = swrtRegion;
    m_lastBuildStartTime = PerformanceClock::Now();
    m_dirty = false;

    m_buildTask = TaskSystem::GetInstance().Enqueue(
        [input = std::move(input)]() mutable -> BuildResult
        {
            return Build(std::move(input));
        },
        TaskThreadPoolName::THREAD_POOL_BACKGROUND);

    return swapped;
}

} // namespace Hyperion
