/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/SWRT/GlimmerBLASCache.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Mesh.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/Threads.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

static constexpr uint32 FreedRangeReuseDelayFrames = 4;
static constexpr uint32 MinFramesBeforeEviction = 8;
static constexpr uint32 FailedRetryDelayFrames = 120;

static constexpr uint32 PackedVertexSizeInFloats = sizeof(TVertex<VT_Simple>) / sizeof(float);

static constexpr uint64 PoolBytes = 64ull * 1024ull * 1024ull;
static constexpr uint32 MaxBuildsInFlight = 4;
static constexpr size_t UploadBudgetBytes = 8u * 1024u * 1024u;

#pragma region GlimmerPoolAllocator

void GlimmerPoolAllocator::Reset(uint32 capacity)
{
    m_capacity = capacity;
    m_numUsed = 0;

    m_freeRanges.Clear();

    if (capacity != 0)
    {
        m_freeRanges.PushBack(Range { 0, capacity });
    }
}

uint32 GlimmerPoolAllocator::Allocate(uint32 count)
{
    if (count == 0)
    {
        return InvalidOffset;
    }

    for (size_t rangeIndex = 0; rangeIndex < m_freeRanges.Size(); rangeIndex++)
    {
        Range& range = m_freeRanges[rangeIndex];

        if (range.count < count)
        {
            continue;
        }

        const uint32 offset = range.offset;

        range.offset += count;
        range.count -= count;

        if (range.count == 0)
        {
            m_freeRanges.EraseAt(rangeIndex);
        }

        m_numUsed += count;

        return offset;
    }

    return InvalidOffset;
}

void GlimmerPoolAllocator::Free(uint32 offset, uint32 count)
{
    if (count == 0 || offset == InvalidOffset)
    {
        return;
    }

    AssertDebug(m_numUsed >= count);
    m_numUsed -= count;

    size_t insertIndex = 0;

    while (insertIndex < m_freeRanges.Size() && m_freeRanges[insertIndex].offset < offset)
    {
        insertIndex++;
    }

    m_freeRanges.Insert(m_freeRanges.Begin() + insertIndex, Range { offset, count });

    // merge with the following range, then the preceding one
    if (insertIndex + 1 < m_freeRanges.Size()
        && m_freeRanges[insertIndex].offset + m_freeRanges[insertIndex].count == m_freeRanges[insertIndex + 1].offset)
    {
        m_freeRanges[insertIndex].count += m_freeRanges[insertIndex + 1].count;
        m_freeRanges.EraseAt(insertIndex + 1);
    }

    if (insertIndex > 0
        && m_freeRanges[insertIndex - 1].offset + m_freeRanges[insertIndex - 1].count == m_freeRanges[insertIndex].offset)
    {
        m_freeRanges[insertIndex - 1].count += m_freeRanges[insertIndex].count;
        m_freeRanges.EraseAt(insertIndex);
    }
}

#pragma endregion GlimmerPoolAllocator

#pragma region GlimmerBLASCache

GlimmerBLASCache::GlimmerBLASCache()
    : m_residentGeneration(0),
      m_numBuildsInFlight(0)
{
}

GlimmerBLASCache::~GlimmerBLASCache()
{
    for (auto& it : m_entries)
    {
        Entry& entry = *it.second;

        if (entry.buildTask.IsValid() && !entry.buildTask.IsCompleted())
        {
            entry.buildTask.Await();
        }
    }

    m_entries.Clear();

    EnqueueDeletion(std::move(m_nodesBuffer));
    EnqueueDeletion(std::move(m_trianglesBuffer));
}

uint64 GlimmerBLASCache::MakeKey(const Mesh* mesh, uint8 lodIndex)
{
    AssertDebug(mesh != nullptr);

    return (uint64(mesh->Id().Value()) << 8) | uint64(lodIndex);
}

void GlimmerBLASCache::CreatePoolBuffers()
{
    // nodes average about a quarter of a triangle's footprint with leaves of 2-8 triangles
    const uint32 nodeCapacity = uint32((PoolBytes / 4) / sizeof(GlimmerBVHNode));
    const uint32 triangleCapacity = uint32((PoolBytes - PoolBytes / 4) / sizeof(GlimmerTriangle));

    m_nodesBuffer = RI.MakeGpuBuffer(GpuBufferType::StructuredBuffer, size_t(nodeCapacity) * sizeof(GlimmerBVHNode), alignof(Vec4f));
    Check(m_nodesBuffer->Create());

    m_trianglesBuffer = RI.MakeGpuBuffer(GpuBufferType::StructuredBuffer, size_t(triangleCapacity) * sizeof(GlimmerTriangle), alignof(Vec4f));
    Check(m_trianglesBuffer->Create());

#ifdef HYP_RHI_DEBUG_NAMES
    m_nodesBuffer->SetDebugName(NAME("GlimmerBLASNodes"));
    m_trianglesBuffer->SetDebugName(NAME("GlimmerBLASTriangles"));
#endif

    m_nodeAllocator.Reset(nodeCapacity);
    m_triangleAllocator.Reset(triangleCapacity);

    HYP_LOG(Rendering, Info, "Glimmer: created BLAS pool ({} MB, {} nodes, {} triangles)",
        PoolBytes / (1024ull * 1024ull), nodeCapacity, triangleCapacity);
}

bool GlimmerBLASCache::Request(Mesh* mesh, uint8 lodIndex, GlimmerBLASRef& outRef)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (!mesh)
    {
        return false;
    }

    const uint64 key = MakeKey(mesh, lodIndex);

    if (KeyValuePair<uint64, UniquePtr<Entry>>* it = m_entries.TryGet(key))
    {
        Entry& entry = *it->second;

        // ids get reused, so make sure this is still the mesh the BLAS was built from
        const bool isSameMesh = !entry.meshWeak.Expired() && entry.meshWeak.GetUnsafe() == mesh;

        if (isSameMesh && entry.state != EntryState::Evicted)
        {
            entry.lastUsedFrame = GetFrameCounter();

            if (entry.state != EntryState::Resident)
            {
                return false;
            }

            outRef = entry.ref;

            return true;
        }

        if (entry.state == EntryState::Building || entry.numReferences != 0)
        {
            // let the stale build finish, or wait for the TLASes still using the old one to let go
            return false;
        }

        if (entry.state == EntryState::Resident)
        {
            FreeEntryRanges(entry);
        }

        entry = Entry {};
        entry.state = EntryState::Queued;
        entry.mesh = MakeStrongRef(mesh);
        entry.meshWeak = MakeWeakRef(mesh);
        entry.lodIndex = lodIndex;
        entry.ref.key = key;
        entry.lastUsedFrame = GetFrameCounter();

        return false;
    }

    UniquePtr<Entry> entry = MakeUnique<Entry>();
    entry->state = EntryState::Queued;
    entry->mesh = MakeStrongRef(mesh);
    entry->meshWeak = MakeWeakRef(mesh);
    entry->lodIndex = lodIndex;
    entry->ref.key = key;
    entry->lastUsedFrame = GetFrameCounter();

    m_entries.Set(key, std::move(entry));

    return false;
}

void GlimmerBLASCache::AddReferences(Span<const uint64> keys)
{
    for (const uint64 key : keys)
    {
        if (KeyValuePair<uint64, UniquePtr<Entry>>* it = m_entries.TryGet(key))
        {
            it->second->numReferences++;
        }
    }
}

void GlimmerBLASCache::RemoveReferences(Span<const uint64> keys)
{
    for (const uint64 key : keys)
    {
        if (KeyValuePair<uint64, UniquePtr<Entry>>* it = m_entries.TryGet(key))
        {
            Entry& entry = *it->second;

            AssertDebug(entry.numReferences != 0);

            if (entry.numReferences != 0)
            {
                entry.numReferences--;
            }

            entry.lastUsedFrame = GetFrameCounter();
        }
    }
}

GlimmerBLASCache::BuildResult GlimmerBLASCache::BuildBLAS(const Handle<Mesh>& mesh, uint8 lodIndex)
{
    HYP_SCOPE;

    BuildResult result;

    if (!mesh.IsValid())
    {
        return result;
    }

    Array<float> packedVertices;
    Array<uint32> indices;

    {
        auto resourceGuard = mesh->GetReadScope();

        if (!resourceGuard)
        {
            return result;
        }

        if (lodIndex >= mesh->GetMeshDesc().GetNumLods())
        {
            lodIndex = 0;
        }

        mesh->BuildVertexBuffer(StaticVertexInputLayout<VT_Simple>, lodIndex, packedVertices);

        const Span<const ubyte> indexData = mesh->GetIndexData(lodIndex);
        const size_t indexSize = GpuElemTypeSize(mesh->GetMeshDesc().meshAttributes.indexBufferElemType);

        if (indexSize != sizeof(uint16) && indexSize != sizeof(uint32))
        {
            return result;
        }

        const size_t numIndices = indexData.Size() / indexSize;
        indices.Resize(numIndices);

        for (size_t index = 0; index < numIndices; index++)
        {
            if (indexSize == sizeof(uint32))
            {
                Memory::Copy(&indices[index], indexData.Data() + index * sizeof(uint32), sizeof(uint32));
            }
            else
            {
                uint16 index16;
                Memory::Copy(&index16, indexData.Data() + index * sizeof(uint16), sizeof(uint16));

                indices[index] = index16;
            }
        }
    }

    const uint32 numVertices = uint32(packedVertices.Size() / PackedVertexSizeInFloats);

    Array<GlimmerTriangle> unorderedTriangles;
    unorderedTriangles.Reserve(indices.Size() / 3);

    Array<BoundingBox> triangleBounds;
    triangleBounds.Reserve(indices.Size() / 3);

    for (size_t firstIndex = 0; firstIndex + 2 < indices.Size(); firstIndex += 3)
    {
        Vec3f positions[3];
        Vec3f normalSum = Vec3f::Zero();
        bool isValid = true;

        for (uint32 corner = 0; corner < 3; corner++)
        {
            const uint32 vertexIndex = indices[firstIndex + corner];

            if (vertexIndex >= numVertices)
            {
                isValid = false;

                break;
            }

            const float* vertex = packedVertices.Data() + size_t(vertexIndex) * PackedVertexSizeInFloats;
            positions[corner] = Vec3f(vertex[0], vertex[1], vertex[2]);
            normalSum += Vec3f(vertex[3], vertex[4], vertex[5]);
        }

        if (!isValid || !MathUtil::IsFinite(positions[0]) || !MathUtil::IsFinite(positions[1]) || !MathUtil::IsFinite(positions[2]))
        {
            continue;
        }

        // wind every triangle so cross(edge1, edge2) faces the way its vertex normals do, so front and back faces
        // mean the same thing for every asset no matter which winding it was authored with
        if ((positions[1] - positions[0]).Cross(positions[2] - positions[0]).Dot(normalSum) < 0.0f)
        {
            std::swap(positions[1], positions[2]);
        }

        const Vec3f edge1 = positions[1] - positions[0];
        const Vec3f edge2 = positions[2] - positions[0];

        // Zero area triangles can never be hit
        if (!(edge1.Cross(edge2).LengthSquared() > 0.0f))
        {
            continue;
        }

        GlimmerTriangle& triangle = unorderedTriangles.EmplaceBack();

        for (int axis = 0; axis < 3; axis++)
        {
            triangle.position0[axis] = positions[0][axis];
            triangle.edge1[axis] = edge1[axis];
            triangle.edge2[axis] = edge2[axis];
        }

        triangle.padding0 = 0;
        triangle.padding1 = 0;
        triangle.padding2 = 0;

        const BoundingBox bounds = BoundingBox(positions[0], positions[0]).Union(positions[1]).Union(positions[2]);

        triangleBounds.PushBack(bounds);
        result.localBounds = result.localBounds.Union(bounds);
    }

    if (unorderedTriangles.Empty())
    {
        return result;
    }

    Array<uint32> triangleOrder;
    GlimmerBVHBuilder::Build(triangleBounds.ToSpan(), GlimmerBVHBuildParams {}, result.nodes, triangleOrder);

    result.triangles.Resize(triangleOrder.Size());

    for (size_t orderIndex = 0; orderIndex < triangleOrder.Size(); orderIndex++)
    {
        result.triangles[orderIndex] = unorderedTriangles[triangleOrder[orderIndex]];
    }

    result.depth = GlimmerBVHBuilder::CalculateDepth(result.nodes.ToSpan());

    return result;
}

bool GlimmerBLASCache::AllocateEntry(Entry& entry)
{
    const uint32 nodeCount = uint32(entry.result.nodes.Size());
    const uint32 triangleCount = uint32(entry.result.triangles.Size());

    if (nodeCount > m_nodeAllocator.GetCapacity() || triangleCount > m_triangleAllocator.GetCapacity())
    {
        return false;
    }

    const uint32 nodeBase = m_nodeAllocator.Allocate(nodeCount);
    const uint32 triangleBase = nodeBase != GlimmerPoolAllocator::InvalidOffset
        ? m_triangleAllocator.Allocate(triangleCount)
        : GlimmerPoolAllocator::InvalidOffset;

    if (nodeBase != GlimmerPoolAllocator::InvalidOffset && triangleBase != GlimmerPoolAllocator::InvalidOffset)
    {
        entry.ref.nodeBase = nodeBase;
        entry.ref.nodeCount = nodeCount;
        entry.ref.triangleBase = triangleBase;
        entry.ref.triangleCount = triangleCount;

        return true;
    }

    if (nodeBase != GlimmerPoolAllocator::InvalidOffset)
    {
        m_nodeAllocator.Free(nodeBase, nodeCount);
    }

    // evicted ranges only become allocatable after a few frames, so make room for a later attempt
    EvictOne();

    return false;
}

bool GlimmerBLASCache::EvictOne()
{
    const uint32 frameCounter = GetFrameCounter();

    uint64 evictKey = 0;
    Entry* evictEntry = nullptr;

    for (auto& it : m_entries)
    {
        Entry& entry = *it.second;

        if (entry.state != EntryState::Resident || entry.numReferences != 0)
        {
            continue;
        }

        if (frameCounter - entry.lastUsedFrame < MinFramesBeforeEviction)
        {
            continue;
        }

        if (!evictEntry || entry.lastUsedFrame < evictEntry->lastUsedFrame)
        {
            evictKey = it.first;
            evictEntry = &entry;
        }
    }

    if (!evictEntry)
    {
        return false;
    }

    FreeEntryRanges(*evictEntry);

    // erased after the Update() loop, which may be iterating the entries
    evictEntry->state = EntryState::Evicted;
    m_keysToErase.PushBack(evictKey);

    return true;
}

void GlimmerBLASCache::FreeEntryRanges(Entry& entry)
{
    m_deferredFrees.PushBack(DeferredFree {
        entry.ref.nodeBase,
        entry.ref.nodeCount,
        entry.ref.triangleBase,
        entry.ref.triangleCount,
        GetFrameCounter() });

    entry.ref.nodeCount = 0;
    entry.ref.triangleCount = 0;
}

void GlimmerBLASCache::UploadEntry(Frame* frame, Entry& entry)
{
    CommandRecorder& cr = frame->cr;

    const size_t nodesByteSize = entry.result.nodes.ByteSize();
    const size_t trianglesByteSize = entry.result.triangles.ByteSize();

    GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(nodesByteSize + trianglesByteSize);
    Assert(stagingBuffer != nullptr);

    stagingBuffer->Copy(0, nodesByteSize, entry.result.nodes.Data());
    stagingBuffer->Copy(nodesByteSize, trianglesByteSize, entry.result.triangles.Data());
    stagingBuffer->Flush(0, nodesByteSize + trianglesByteSize);

    cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);
    cr << InsertBarrier(m_nodesBuffer.Get(), ResourceState::CopyDst);
    cr << InsertBarrier(m_trianglesBuffer.Get(), ResourceState::CopyDst);

    cr << CopyBuffer(stagingBuffer, m_nodesBuffer.Get(),
        0,
        uint32(size_t(entry.ref.nodeBase) * sizeof(GlimmerBVHNode)),
        uint32(nodesByteSize));

    cr << CopyBuffer(stagingBuffer, m_trianglesBuffer.Get(),
        uint32(nodesByteSize),
        uint32(size_t(entry.ref.triangleBase) * sizeof(GlimmerTriangle)),
        uint32(trianglesByteSize));

    entry.ref.depth = entry.result.depth;
    entry.ref.localBounds = entry.result.localBounds;

    entry.result = BuildResult {};
    entry.state = EntryState::Resident;
}

void GlimmerBLASCache::Update(Frame* frame)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (!IsReady())
    {
        CreatePoolBuffers();
    }

    const uint32 frameCounter = GetFrameCounter();

    for (size_t freeIndex = 0; freeIndex < m_deferredFrees.Size();)
    {
        const DeferredFree& deferredFree = m_deferredFrees[freeIndex];

        if (frameCounter - deferredFree.frameFreed < FreedRangeReuseDelayFrames)
        {
            ++freeIndex;

            continue;
        }

        m_nodeAllocator.Free(deferredFree.nodeOffset, deferredFree.nodeCount);
        m_triangleAllocator.Free(deferredFree.triangleOffset, deferredFree.triangleCount);

        m_deferredFrees.EraseAt(freeIndex);
    }

    size_t uploadedBytes = 0;
    bool uploadedAny = false;

    for (auto& it : m_entries)
    {
        Entry& entry = *it.second;

        switch (entry.state)
        {
        case EntryState::Queued:
        {
            if (m_numBuildsInFlight >= MaxBuildsInFlight)
            {
                break;
            }

            entry.buildTask = TaskSystem::GetInstance().Enqueue(
                [mesh = entry.mesh, lodIndex = entry.lodIndex]() -> BuildResult
                {
                    return BuildBLAS(mesh, lodIndex);
                },
                TaskThreadPoolName::THREAD_POOL_BACKGROUND);

            entry.state = EntryState::Building;
            m_numBuildsInFlight++;

            break;
        }
        case EntryState::Building:
        {
            if (!entry.buildTask.IsCompleted())
            {
                break;
            }

            entry.result = std::move(entry.buildTask).Await();
            entry.buildTask = Task<BuildResult>();
            entry.mesh = Handle<Mesh>();

            AssertDebug(m_numBuildsInFlight != 0);
            m_numBuildsInFlight--;

            if (entry.result.nodes.Empty())
            {
                entry.state = EntryState::Failed;
                entry.failedFrame = frameCounter;

                break;
            }

            entry.state = EntryState::PendingUpload;

            [[fallthrough]];
        }
        case EntryState::PendingUpload:
        {
            const size_t entryBytes = entry.result.nodes.ByteSize() + entry.result.triangles.ByteSize();

            // always allow one upload per frame so a single large BLAS can't stall forever
            if (uploadedAny && uploadedBytes + entryBytes > UploadBudgetBytes)
            {
                break;
            }

            if (!AllocateEntry(entry))
            {
                break;
            }

            UploadEntry(frame, entry);

            uploadedBytes += entryBytes;
            uploadedAny = true;

            m_residentGeneration++;

            break;
        }
        case EntryState::Failed:
        {
            if (frameCounter - entry.failedFrame < FailedRetryDelayFrames)
            {
                break;
            }

            entry.mesh = entry.meshWeak.Lock();

            if (entry.mesh.IsValid())
            {
                entry.state = EntryState::Queued;
                entry.lastUsedFrame = frameCounter;
            }
            else
            {
                entry.state = EntryState::Evicted;
                m_keysToErase.PushBack(it.first);
            }

            break;
        }
        default:
            break;
        }
    }

    for (const uint64 keyToErase : m_keysToErase)
    {
        if (KeyValuePair<uint64, UniquePtr<Entry>>* it = m_entries.TryGet(keyToErase))
        {
            if (it->second->state == EntryState::Evicted)
            {
                m_entries.Erase(keyToErase);
            }
        }
    }

    m_keysToErase.Clear();

    if (uploadedAny)
    {
        frame->cr << InsertBarrier(m_nodesBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
        frame->cr << InsertBarrier(m_trianglesBuffer.Get(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    }
}

GlimmerBLASCacheStats GlimmerBLASCache::GetStats() const
{
    GlimmerBLASCacheStats stats;

    for (const auto& it : m_entries)
    {
        const Entry& entry = *it.second;

        switch (entry.state)
        {
        case EntryState::Queued:
        case EntryState::Building:
            stats.numBuilding++;
            break;
        case EntryState::PendingUpload:
            stats.numPendingUpload++;
            break;
        case EntryState::Resident:
            stats.numResident++;
            stats.numResidentTriangles += entry.ref.triangleCount;
            break;
        case EntryState::Failed:
            stats.numFailed++;
            break;
        }
    }

    stats.nodesUsed = m_nodeAllocator.GetNumUsed();
    stats.nodesCapacity = m_nodeAllocator.GetCapacity();
    stats.trianglesUsed = m_triangleAllocator.GetNumUsed();
    stats.trianglesCapacity = m_triangleAllocator.GetCapacity();

    return stats;
}

#pragma endregion GlimmerBLASCache

} // namespace Hyperion
