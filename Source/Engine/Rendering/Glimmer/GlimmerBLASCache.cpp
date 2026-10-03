/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerBLASCache.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Mesh.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/TaskThread.hpp>
#include <Core/Threading/ThreadPool.hpp>
#include <Core/Threading/Threads.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <Core/Logging/Logger.hpp>

#include <algorithm>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

static constexpr uint32 FreedRangeReuseDelayFrames = 4;
static constexpr uint32 MinFramesBeforeEviction = 8;
static constexpr uint32 FailedRetryDelayFrames = 120;

static constexpr uint32 PackedVertexSizeInFloats = sizeof(TVertex<VT_Simple>) / sizeof(float);

static constexpr uint64 PoolBytes = 192ull * 1024ull * 1024ull;
static constexpr uint64 NodePoolBytes = PoolBytes * 3 / 16;

static constexpr uint32 BLASMinLeafSize = 4;
static constexpr uint32 BLASMaxLeafSize = 16;
static constexpr uint32 MaxBuildsInFlight = 4;
static constexpr size_t UploadBudgetBytes = 8u * 1024u * 1024u;

// a full pool only gives up a mesh's BLAS for one this much nearer, so two meshes at about the same distance don't take turns
static constexpr float DemotionDistanceRatio = 1.25f;
static constexpr float DemotionDistanceMargin = 8.0f;
// a mesh waiting for a stretch of the pool to be cleared for it asks again no sooner than this, giving the TLASes time to let go
static constexpr uint32 RoomRequestIntervalFrames = 30;

// a mesh that didn't fit is built again no sooner than this (doubling each time it doesn't fit again), and only when there's room for it
static constexpr uint32 DeferredRetryDelayFrames = 30;
static constexpr uint32 MaxDeferredRetryShift = 5;
static constexpr uint32 PoolFullLogIntervalFrames = 600;

static constexpr uint32 LodErrorScaleIntervalFrames = 120;
static constexpr float LodErrorScaleStepUp = 1.5f;
static constexpr float LodErrorScaleStepDown = 1.25f;
static constexpr float MaxLodErrorScale = 4.0f;
static constexpr float LodErrorScaleMinUsageDrop = 0.005f;
static constexpr float LodErrorScaleRelaxUsage = 0.8f;

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

uint32 GlimmerPoolAllocator::GetLargestFreeRange() const
{
    uint32 largest = 0;

    for (const Range& range : m_freeRanges)
    {
        largest = MathUtil::Max(largest, range.count);
    }

    return largest;
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

class GlimmerBuildThread final : public TaskThread
{
public:
    GlimmerBuildThread(const ThreadId& id)
        : TaskThread(id, ThreadPriorityValue::LOW)
    {
    }

    virtual ~GlimmerBuildThread() override = default;
};

GlimmerBLASCache::GlimmerBLASCache()
    : m_buildPool(MakeUnique<TaskThreadPool>(TypeWrapper<GlimmerBuildThread>(), "GlimmerBuild", 1)),
      m_residentGeneration(0),
      m_evictionGeneration(0),
      m_numBuildsInFlight(0),
      m_lastUpdateFrame(~0u),
      m_lastPoolFullLogFrame(~0u),
      m_lodErrorScale(1.0f),
      m_lastLodErrorScaleFrame(0),
      m_poolUsageAtLodErrorScaleStep(1.0f)
{
    m_buildPool->Start();
}

SharedPtr<GlimmerBLASCache> GlimmerBLASCache::AcquireShared()
{
    AssertOnThread(g_renderThread);

    static WeakPtr<GlimmerBLASCache> s_blasCache;

    SharedPtr<GlimmerBLASCache> blasCache = s_blasCache.Lock();

    if (!blasCache)
    {
        blasCache = MakeShared<GlimmerBLASCache>();
        s_blasCache = blasCache;
    }

    return blasCache;
}

void GlimmerBLASCache::UpdateOncePerFrame(Frame* frame)
{
    const uint32 frameCounter = GetFrameCounter();

    if (m_lastUpdateFrame == frameCounter)
    {
        return;
    }

    m_lastUpdateFrame = frameCounter;

    Update(frame);
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

    if (m_buildPool->IsRunning())
    {
        m_buildPool->Stop();

        for (const UniquePtr<ThreadBase>& thread : m_buildPool->GetThreads())
        {
            thread->GetScheduler().WakeUpOwnerThread();
        }

        while (m_buildPool->IsRunning())
        {
            ThreadSleep(1);
        }
    }

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
    const uint32 nodeCapacity = uint32(NodePoolBytes / sizeof(GlimmerBLASNode));
    const uint32 triangleCapacity = uint32((PoolBytes - NodePoolBytes) / sizeof(GlimmerTriangle));

    Assert(nodeCapacity <= GlimmerBLASNode::MaxIndex + 1 && triangleCapacity <= GlimmerBLASNode::MaxIndex + 1);

    m_nodesBuffer = CreateGlimmerStructuredBuffer(sizeof(GlimmerBLASNode), nodeCapacity);
    m_trianglesBuffer = CreateGlimmerStructuredBuffer(sizeof(GlimmerTriangle), triangleCapacity);

#ifdef HYP_RHI_DEBUG_NAMES
    m_nodesBuffer->SetDebugName(NAME("GlimmerBLASNodes"));
    m_trianglesBuffer->SetDebugName(NAME("GlimmerBLASTriangles"));
#endif

    m_nodeAllocator.Reset(nodeCapacity);
    m_triangleAllocator.Reset(triangleCapacity);

    HYP_LOG(Rendering, Info, "Glimmer: created BLAS pool ({} MB, {} nodes, {} triangles)",
        PoolBytes / (1024ull * 1024ull), nodeCapacity, triangleCapacity);
}

static void UpdateEntryPriority(float& entryPriority, uint32& entryPriorityFrame, float priority)
{
    const uint32 frameCounter = GetFrameCounter();

    if (entryPriorityFrame != frameCounter)
    {
        entryPriority = priority;
        entryPriorityFrame = frameCounter;
    }
    else
    {
        entryPriority = MathUtil::Min(entryPriority, priority);
    }
}

GlimmerBLASRequestResult GlimmerBLASCache::Request(Mesh* mesh, uint8 lodIndex, float priority, GlimmerBLASRef& outRef)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (!mesh)
    {
        return GlimmerBLASRequestResult::Failed;
    }

    const uint32 numLods = mesh->GetMeshDesc().GetNumLods();

    if (lodIndex >= numLods)
    {
        lodIndex = 0;
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

            UpdateEntryPriority(entry.priority, entry.priorityFrame, priority);

            if (entry.isDeferred && entry.numDefers != 0 && entry.priority < entry.deferredPriority * 0.5f)
            {
                entry.numDefers = 0;
                entry.failedFrame = GetFrameCounter() - DeferredRetryDelayFrames;
            }

            if (entry.state == EntryState::Failed && entry.dead)
            {
                return GlimmerBLASRequestResult::Failed;
            }

            // on its way out: once the TLASes have rebuilt without it, it's freed
            if (entry.isDemoted)
            {
                return GlimmerBLASRequestResult::Pending;
            }

            if (entry.state != EntryState::Resident)
            {
                return GlimmerBLASRequestResult::Pending;
            }

            outRef = entry.ref;

            return GlimmerBLASRequestResult::Resident;
        }

        if (entry.state == EntryState::Building || entry.numReferences != 0)
        {
            // let the stale build finish, or wait for the TLASes still using the old one to let go, then requeue for this mesh
            entry.replacementMesh = MakeWeakRef(mesh);

            return GlimmerBLASRequestResult::Pending;
        }

        Requeue(entry, mesh);
        UpdateEntryPriority(entry.priority, entry.priorityFrame, priority);

        return GlimmerBLASRequestResult::Pending;
    }

    UniquePtr<Entry> entry = MakeUnique<Entry>();
    entry->state = EntryState::Queued;
    entry->mesh = MakeStrongRef(mesh);
    entry->meshWeak = MakeWeakRef(mesh);
    entry->lodIndex = lodIndex;
    entry->ref.key = key;
    entry->lastUsedFrame = GetFrameCounter();

    UpdateEntryPriority(entry->priority, entry->priorityFrame, priority);

    m_entries.Set(key, std::move(entry));

    return GlimmerBLASRequestResult::Pending;
}

bool GlimmerBLASCache::TryGetResident(Mesh* mesh, uint8 lodIndex, float priority, GlimmerBLASRef& outRef)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (!mesh || lodIndex >= MathUtil::Max<uint8>(mesh->GetMeshDesc().GetNumLods(), 1))
    {
        return false;
    }

    KeyValuePair<uint64, UniquePtr<Entry>>* it = m_entries.TryGet(MakeKey(mesh, lodIndex));

    if (!it)
    {
        return false;
    }

    Entry& entry = *it->second;

    if (entry.state != EntryState::Resident || entry.isDemoted || entry.meshWeak.Expired() || entry.meshWeak.GetUnsafe() != mesh)
    {
        return false;
    }

    entry.lastUsedFrame = GetFrameCounter();

    UpdateEntryPriority(entry.priority, entry.priorityFrame, priority);

    outRef = entry.ref;

    return true;
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

            if (entry.numReferences == 0 && entry.state != EntryState::Building)
            {
                RequeueForReplacement(entry);
            }
        }
    }
}

void GlimmerBLASCache::Requeue(Entry& entry, Mesh* mesh)
{
    AssertDebug(entry.numReferences == 0 && !entry.buildTask.IsValid());

    if (entry.state == EntryState::Resident)
    {
        FreeEntryRanges(entry);
    }

    const uint64 key = entry.ref.key;
    const uint8 lodIndex = entry.lodIndex;

    entry = Entry {};
    entry.state = EntryState::Queued;
    entry.mesh = MakeStrongRef(mesh);
    entry.meshWeak = MakeWeakRef(mesh);
    entry.lodIndex = lodIndex;
    entry.ref.key = key;
    entry.lastUsedFrame = GetFrameCounter();
}

bool GlimmerBLASCache::RequeueForReplacement(Entry& entry)
{
    if (!entry.replacementMesh.IsValid())
    {
        return false;
    }

    const Handle<Mesh> replacementMesh = entry.replacementMesh.Expired() ? Handle<Mesh>() : entry.replacementMesh.Lock();
    entry.replacementMesh = WeakHandle<Mesh>();

    if (!replacementMesh.IsValid())
    {
        return false;
    }

    Requeue(entry, replacementMesh.Get());

    return true;
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
            result.isFail = true;

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

        triangle.leafFlags = 0;
        triangle.padding1 = 0;
        triangle.padding2 = 0;

        const BoundingBox bounds = BoundingBox(positions[0], positions[0]).Union(positions[1]).Union(positions[2]);

        triangleBounds.PushBack(bounds);
        result.localBounds = result.localBounds.Union(bounds);
    }

    if (unorderedTriangles.Empty())
    {
        result.isFail = !indices.Empty();

        return result;
    }

    GlimmerBVHBuildParams buildParams;
    buildParams.minLeafSize = BLASMinLeafSize;
    buildParams.maxLeafSize = BLASMaxLeafSize;

    Array<GlimmerBVHNode> bvhNodes;
    Array<uint32> triangleOrder;
    GlimmerBVHBuilder::Build(triangleBounds.ToSpan(), buildParams, bvhNodes, triangleOrder);

    result.triangles.Resize(triangleOrder.Size());

    for (size_t orderIndex = 0; orderIndex < triangleOrder.Size(); orderIndex++)
    {
        result.triangles[orderIndex] = unorderedTriangles[triangleOrder[orderIndex]];
    }

    result.depth = GlimmerBVHBuilder::CalculateDepth(bvhNodes.ToSpan());

    Array<uint32> leafEnds;

    if (!GlimmerBVHBuilder::PackBLAS(bvhNodes.ToSpan(), result.nodes, leafEnds))
    {
        // more triangles or nodes than a child ref can address, which is more than the pool holds anyway
        result = BuildResult {};
        result.isFail = true;

        return result;
    }

    for (const uint32 leafEnd : leafEnds)
    {
        result.triangles[leafEnd].leafFlags |= GlimmerBLASNode::LeafEndFlag;
    }

    return result;
}

bool GlimmerBLASCache::AllocateEntry(Entry& entry)
{
    const uint32 nodeCount = uint32(entry.result.nodes.Size());
    const uint32 triangleCount = uint32(entry.result.triangles.Size());

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

        if (entry.state != EntryState::Resident || entry.numReferences != 0 || entry.isDemoted)
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

bool GlimmerBLASCache::IsWorthBuilding(const Entry& entry, uint32 frameCounter, float farthestDemotablePriority) const
{
    if (!entry.isDeferred)
    {
        return true;
    }

    const uint32 retryDelay = DeferredRetryDelayFrames << MathUtil::Min(entry.numDefers, MaxDeferredRetryShift);

    if (frameCounter - entry.failedFrame < retryDelay)
    {
        return false;
    }

    const bool fits = m_nodeAllocator.GetLargestFreeRange() >= entry.lastNodeCount
        && m_triangleAllocator.GetLargestFreeRange() >= entry.lastTriangleCount;

    return fits || farthestDemotablePriority > entry.priority * DemotionDistanceRatio + DemotionDistanceMargin;
}

bool GlimmerBLASCache::MakeContiguousRoom(const Entry& requester, uint32 nodeCount, uint32 triangleCount, uint32& numDemotions)
{
    const float threshold = requester.priority * DemotionDistanceRatio + DemotionDistanceMargin;

    Array<Entry*> residents;

    for (auto& it : m_entries)
    {
        if (it.second->state == EntryState::Resident)
        {
            residents.PushBack(it.second.Get());
        }
    }

    const auto findStretch = [&](bool isNodes, uint32 count, uint32 capacity, Array<Entry*>& outVictims) -> bool
    {
        const auto baseOf = [isNodes](const Entry* entry)
        {
            return isNodes ? entry->ref.nodeBase : entry->ref.triangleBase;
        };

        const auto sizeOf = [isNodes](const Entry* entry)
        {
            return isNodes ? entry->ref.nodeCount : entry->ref.triangleCount;
        };

        std::sort(residents.Begin(), residents.End(), [&baseOf](const Entry* lhs, const Entry* rhs)
            {
                return baseOf(lhs) < baseOf(rhs);
            });

        uint64 bestCost = ~0ull;
        uint32 bestStart = 0;

        // the cheapest stretch starts at the start of the pool or right after a BLAS
        for (size_t first = 0; first <= residents.Size(); first++)
        {
            const uint32 start = first == 0 ? 0u : baseOf(residents[first - 1]) + sizeOf(residents[first - 1]);

            if (uint64(start) + count > capacity)
            {
                break;
            }

            uint64 cost = 0;
            bool isBlocked = false;

            for (size_t index = first; index < residents.Size() && baseOf(residents[index]) < start + count; index++)
            {
                const Entry* occupant = residents[index];

                if (occupant->isDemoted)
                {
                    continue;
                }

                if (occupant->priority <= threshold)
                {
                    isBlocked = true;

                    break;
                }

                cost += uint64(occupant->ref.triangleCount) + 1;
            }

            if (!isBlocked && cost < bestCost)
            {
                bestCost = cost;
                bestStart = start;

                if (cost == 0)
                {
                    break;
                }
            }
        }

        if (bestCost == ~0ull)
        {
            return false;
        }

        for (Entry* occupant : residents)
        {
            if (!occupant->isDemoted && baseOf(occupant) < bestStart + count && baseOf(occupant) + sizeOf(occupant) > bestStart)
            {
                outVictims.PushBack(occupant);
            }
        }

        return true;
    };

    Array<Entry*> victims;

    if (!findStretch(true, nodeCount, m_nodeAllocator.GetCapacity(), victims)
        || !findStretch(false, triangleCount, m_triangleAllocator.GetCapacity(), victims))
    {
        return false;
    }

    for (Entry* victim : victims)
    {
        if (!victim->isDemoted)
        {
            victim->isDemoted = true;
            numDemotions++;
        }
    }

    return true;
}

void GlimmerBLASCache::Defer(Entry& entry, uint32 frameCounter)
{
    entry.lastNodeCount = uint32(entry.result.nodes.Size());
    entry.lastTriangleCount = uint32(entry.result.triangles.Size());
    entry.result = BuildResult {};
    entry.mesh = entry.meshWeak.Lock();
    entry.failedFrame = frameCounter;

    if (!entry.mesh.IsValid())
    {
        entry.state = EntryState::Evicted;
        m_keysToErase.PushBack(entry.ref.key);

        return;
    }

    entry.state = EntryState::Queued;
    entry.isDeferred = true;
    entry.deferredPriority = entry.priority;
    entry.numDefers++;
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
        uint32(size_t(entry.ref.nodeBase) * sizeof(GlimmerBLASNode)),
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

    for (auto& it : m_entries)
    {
        Entry& entry = *it.second;

        if (entry.isDemoted && entry.state == EntryState::Resident && entry.numReferences == 0)
        {
            FreeEntryRanges(entry);

            entry.state = EntryState::Evicted;
            m_keysToErase.PushBack(it.first);
        }
    }

    float farthestDemotablePriority = 0.0f;

    uint32 pendingDemotedNodes = 0;
    uint32 pendingDemotedTriangles = 0;

    Array<Entry*> queued;
    Array<Entry*> pendingUploads;

    for (auto& it : m_entries)
    {
        Entry& entry = *it.second;

        if (entry.state == EntryState::Resident)
        {
            if (entry.isDemoted)
            {
                pendingDemotedNodes += entry.ref.nodeCount;
                pendingDemotedTriangles += entry.ref.triangleCount;
            }
            else if (entry.numReferences != 0)
            {
                farthestDemotablePriority = MathUtil::Max(farthestDemotablePriority, entry.priority);
            }
        }
        else if (entry.state == EntryState::Queued)
        {
            queued.PushBack(&entry);
        }
    }

    const auto byPriority = [](const Entry* lhs, const Entry* rhs)
    {
        return lhs->priority < rhs->priority;
    };

    // nearest first
    std::sort(queued.Begin(), queued.End(), byPriority);

    for (Entry* entryPtr : queued)
    {
        if (m_numBuildsInFlight >= MaxBuildsInFlight)
        {
            break;
        }

        Entry& entry = *entryPtr;

        if (!IsWorthBuilding(entry, frameCounter, farthestDemotablePriority))
        {
            continue;
        }

        entry.isDeferred = false;

        entry.buildTask = m_buildPool->Enqueue(
            [mesh = entry.mesh, lodIndex = entry.lodIndex]() -> BuildResult
            {
                return BuildBLAS(mesh, lodIndex);
            });

        entry.state = EntryState::Building;
        m_numBuildsInFlight++;
    }

    for (auto& it : m_entries)
    {
        Entry& entry = *it.second;

        switch (entry.state)
        {
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

            // built from a mesh whose id has since been reused
            if (RequeueForReplacement(entry))
            {
                break;
            }

            if (entry.result.nodes.Empty())
            {
                entry.state = EntryState::Failed;
                entry.failedFrame = frameCounter;

                if ((entry.dead = entry.result.isFail))
                {
                    HYP_LOG(Rendering, Warning, "Glimmer: mesh {} (LOD {}) has no traceable triangles", it.first >> 8, entry.lodIndex);
                }

                break;
            }

            if (entry.result.nodes.Size() > m_nodeAllocator.GetCapacity() || entry.result.triangles.Size() > m_triangleAllocator.GetCapacity())
            {
                HYP_LOG(Rendering, Warning, "Glimmer: mesh {} (LOD {}) has {} triangles, more than the BLAS pool holds",
                    it.first >> 8, entry.lodIndex, entry.result.triangles.Size());

                entry.result = BuildResult {};
                entry.state = EntryState::Failed;
                entry.failedFrame = frameCounter;
                entry.dead = true;

                break;
            }

            entry.state = EntryState::PendingUpload;

            break;
        }
        case EntryState::Failed:
        {
            if (entry.dead)
            {
                if (entry.meshWeak.Expired())
                {
                    entry.state = EntryState::Evicted;
                    m_keysToErase.PushBack(it.first);
                }

                break;
            }

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

        if (entry.state == EntryState::PendingUpload)
        {
            pendingUploads.PushBack(&entry);
        }
    }

    std::sort(pendingUploads.Begin(), pendingUploads.End(), byPriority);

    uint32 availableNodes = m_nodeAllocator.GetCapacity() - m_nodeAllocator.GetNumUsed() + pendingDemotedNodes;
    uint32 availableTriangles = m_triangleAllocator.GetCapacity() - m_triangleAllocator.GetNumUsed() + pendingDemotedTriangles;

    for (const DeferredFree& deferredFree : m_deferredFrees)
    {
        availableNodes += deferredFree.nodeCount;
        availableTriangles += deferredFree.triangleCount;
    }

    size_t uploadedBytes = 0;
    bool uploadedAny = false;

    uint32 numDemotions = 0;
    uint32 numDeferred = 0;
    uint32 numWaiting = 0;

    const Entry* nearestWaiting = nullptr;

    bool isNearerWaiting = false;

    for (const Entry* entryPtr : queued)
    {
        if (entryPtr->isDeferred && entryPtr->state == EntryState::Queued)
        {
            nearestWaiting = entryPtr;

            break;
        }
    }

    for (Entry* entryPtr : pendingUploads)
    {
        Entry& entry = *entryPtr;

        const uint32 nodeCount = uint32(entry.result.nodes.Size());
        const uint32 triangleCount = uint32(entry.result.triangles.Size());
        const size_t entryBytes = entry.result.nodes.ByteSize() + entry.result.triangles.ByteSize();

        const bool isWithinBudget = !uploadedAny || uploadedBytes + entryBytes <= UploadBudgetBytes;

        if (!isWithinBudget || isNearerWaiting)
        {
            availableNodes -= MathUtil::Min(availableNodes, nodeCount);
            availableTriangles -= MathUtil::Min(availableTriangles, triangleCount);

            continue;
        }

        if (AllocateEntry(entry))
        {
            entry.numDefers = 0;

            UploadEntry(frame, entry);

            uploadedBytes += entryBytes;
            uploadedAny = true;

            availableNodes -= MathUtil::Min(availableNodes, nodeCount);
            availableTriangles -= MathUtil::Min(availableTriangles, triangleCount);

            m_residentGeneration++;

            continue;
        }

        if (!nearestWaiting || entry.priority < nearestWaiting->priority)
        {
            nearestWaiting = &entry;
        }

        // unused BLASes go first
        while (availableNodes < nodeCount || availableTriangles < triangleCount)
        {
            const size_t numDeferredFrees = m_deferredFrees.Size();

            if (!EvictOne())
            {
                break;
            }

            for (size_t freeIndex = numDeferredFrees; freeIndex < m_deferredFrees.Size(); freeIndex++)
            {
                availableNodes += m_deferredFrees[freeIndex].nodeCount;
                availableTriangles += m_deferredFrees[freeIndex].triangleCount;
            }
        }

        if (frameCounter - entry.roomRequestFrame >= RoomRequestIntervalFrames)
        {
            entry.roomRequestFrame = frameCounter;

            if (!MakeContiguousRoom(entry, nodeCount, triangleCount, numDemotions))
            {
                Defer(entry, frameCounter);
                numDeferred++;

                continue;
            }
        }

        availableNodes -= MathUtil::Min(availableNodes, nodeCount);
        availableTriangles -= MathUtil::Min(availableTriangles, triangleCount);

        isNearerWaiting = true;
        numWaiting++;
    }

    if (numDemotions != 0)
    {
        m_evictionGeneration++;
    }

    if (frameCounter - m_lastLodErrorScaleFrame >= LodErrorScaleIntervalFrames)
    {
        m_lastLodErrorScaleFrame = frameCounter;

        const float poolUsage = MathUtil::Max(
            float(m_nodeAllocator.GetNumUsed()) / float(MathUtil::Max(m_nodeAllocator.GetCapacity(), 1u)),
            float(m_triangleAllocator.GetNumUsed()) / float(MathUtil::Max(m_triangleAllocator.GetCapacity(), 1u)));

        const float previousScale = m_lodErrorScale;

        if (nearestWaiting != nullptr)
        {
            if (m_lodErrorScale == 1.0f || poolUsage < m_poolUsageAtLodErrorScaleStep - LodErrorScaleMinUsageDrop)
            {
                m_lodErrorScale = MathUtil::Min(m_lodErrorScale * LodErrorScaleStepUp, MaxLodErrorScale);
                m_poolUsageAtLodErrorScaleStep = poolUsage;
            }
        }
        else if (poolUsage < LodErrorScaleRelaxUsage)
        {
            m_lodErrorScale = MathUtil::Max(m_lodErrorScale / LodErrorScaleStepDown, 1.0f);
            m_poolUsageAtLodErrorScaleStep = poolUsage;
        }

        if (m_lodErrorScale != previousScale)
        {
            m_evictionGeneration++;
        }
    }

    if (nearestWaiting && (m_lastPoolFullLogFrame == ~0u || frameCounter - m_lastPoolFullLogFrame >= PoolFullLogIntervalFrames))
    {
        m_lastPoolFullLogFrame = frameCounter;

        const bool isBuilt = nearestWaiting->state == EntryState::PendingUpload;

        HYP_LOG(Rendering, Warning, "Glimmer BLAS pool full (nodes {}/{}, largest free {}; triangles {}/{}, largest free {}); {} built and {} more waiting for room, nearest {} m away ({} nodes, {} triangles)",
            m_nodeAllocator.GetNumUsed(), m_nodeAllocator.GetCapacity(), m_nodeAllocator.GetLargestFreeRange(),
            m_triangleAllocator.GetNumUsed(), m_triangleAllocator.GetCapacity(), m_triangleAllocator.GetLargestFreeRange(),
            numWaiting, numDeferred, nearestWaiting->priority,
            isBuilt ? uint32(nearestWaiting->result.nodes.Size()) : nearestWaiting->lastNodeCount,
            isBuilt ? uint32(nearestWaiting->result.triangles.Size()) : nearestWaiting->lastTriangleCount);
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
            if (entry.isDeferred)
            {
                stats.numWaitingForRoom++;
            }
            else
            {
                stats.numBuilding++;
            }
            break;
        case EntryState::Building:
            stats.numBuilding++;
            break;
        case EntryState::PendingUpload:
            stats.numPendingUpload++;
            break;
        case EntryState::Resident:
            stats.numResident++;
            stats.numResidentTriangles += entry.ref.triangleCount;
            stats.numDemoted += entry.isDemoted ? 1u : 0u;
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
