/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/GlimmerBVHBuilder.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/SharedPtr.hpp>

#include <Core/Threading/Task.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/BoundingBox.hpp>

namespace Hyperion {

namespace threading {
class TaskThreadPool;
} // namespace threading

using threading::TaskThreadPool;

class Mesh;

struct GlimmerTriangle
{
    float position0[3];
    uint32 padding0;
    float edge1[3];
    uint32 padding1;
    float edge2[3];
    uint32 padding2;
};

static_assert(sizeof(GlimmerTriangle) == 48);

struct GlimmerBLASRef
{
    uint64 key = 0;
    uint32 nodeBase = 0;
    uint32 nodeCount = 0;
    uint32 triangleBase = 0;
    uint32 triangleCount = 0;
    uint32 depth = 0;
    BoundingBox localBounds;
};

enum class GlimmerBLASRequestResult : uint8
{
    Resident,
    Pending,
    Failed  //!< the mesh has nothing to trace, or doesn't fit the pool
};

class GlimmerPoolAllocator final
{
public:
    static constexpr uint32 InvalidOffset = ~0u;

    void Reset(uint32 capacity);

    uint32 Allocate(uint32 count);
    void Free(uint32 offset, uint32 count);

    HYP_FORCE_INLINE uint32 GetCapacity() const
    {
        return m_capacity;
    }

    HYP_FORCE_INLINE uint32 GetNumUsed() const
    {
        return m_numUsed;
    }

    uint32 GetLargestFreeRange() const;

private:
    struct Range
    {
        uint32 offset;
        uint32 count;
    };

    Array<Range> m_freeRanges; // sorted by offset, never adjacent
    uint32 m_capacity = 0;
    uint32 m_numUsed = 0;
};

struct GlimmerBLASCacheStats
{
    uint32 numResident = 0;
    uint32 numBuilding = 0;
    uint32 numPendingUpload = 0;
    uint32 numFailed = 0;
    uint32 numDemoted = 0;
    uint32 numWaitingForRoom = 0;
    uint32 numResidentTriangles = 0;
    uint32 nodesUsed = 0;
    uint32 nodesCapacity = 0;
    uint32 trianglesUsed = 0;
    uint32 trianglesCapacity = 0;
};

class GlimmerBLASCache final
{
public:
    GlimmerBLASCache();

    GlimmerBLASCache(const GlimmerBLASCache& other) = delete;
    GlimmerBLASCache& operator=(const GlimmerBLASCache& other) = delete;

    ~GlimmerBLASCache();

    static uint64 MakeKey(const Mesh* mesh, uint8 lodIndex);

    /// priority: how far (m) the nearest instance asking for it is from the viewer; nearer meshes are built and given pool space first,
    /// and push out the BLASes of meshes much farther away when the pool is full
    GlimmerBLASRequestResult Request(Mesh* mesh, uint8 lodIndex, float priority, GlimmerBLASRef& outRef);

    void AddReferences(Span<const uint64> keys);
    void RemoveReferences(Span<const uint64> keys);

    static SharedPtr<GlimmerBLASCache> AcquireShared();

    void UpdateOncePerFrame(Frame* frame);
    void Update(Frame* frame);

    HYP_FORCE_INLINE TaskThreadPool& GetBuildPool() const
    {
        return *m_buildPool;
    }

    HYP_FORCE_INLINE uint32 GetResidentGeneration() const
    {
        return m_residentGeneration;
    }

    HYP_FORCE_INLINE uint32 GetEvictionGeneration() const
    {
        return m_evictionGeneration;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetNodesBuffer() const
    {
        return m_nodesBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetTrianglesBuffer() const
    {
        return m_trianglesBuffer;
    }

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_nodesBuffer.IsValid() && m_trianglesBuffer.IsValid();
    }

    GlimmerBLASCacheStats GetStats() const;

private:
    enum class EntryState : uint8
    {
        Queued,
        Building,
        PendingUpload,
        Resident,
        Failed,
        Evicted
    };

    struct BuildResult
    {
        Array<GlimmerBVHNode> nodes;
        Array<GlimmerTriangle> triangles;
        BoundingBox localBounds;
        uint32 depth = 0;
        bool isFail = false; // set when there's nothing to trace, as opposed to mesh data not being readable yet
    };

    struct Entry
    {
        EntryState state = EntryState::Queued;
        Handle<Mesh> mesh; // only held until the build finishes
        WeakHandle<Mesh> meshWeak;
        WeakHandle<Mesh> replacementMesh; // a newer mesh with the same id, waiting for this entry's build or references to finish
        uint8 lodIndex = 0;
        Task<BuildResult> buildTask;
        BuildResult result;
        GlimmerBLASRef ref;
        uint32 numReferences = 0;
        uint32 lastUsedFrame = 0;
        uint32 failedFrame = 0;
        float priority = 0.0f;
        uint32 priorityFrame = ~0u;
        uint32 lastNodeCount = 0;
        uint32 lastTriangleCount = 0;
        uint32 numDefers = 0;
        float deferredPriority = 0.0f;
        uint32 roomRequestFrame = 0;
        bool dead = false;
        bool isDemoted = false;
        bool isDeferred = false;
    };

    struct DeferredFree
    {
        uint32 nodeOffset;
        uint32 nodeCount;
        uint32 triangleOffset;
        uint32 triangleCount;
        uint32 frameFreed;
    };

    static BuildResult BuildBLAS(const Handle<Mesh>& mesh, uint8 lodIndex);

    void CreatePoolBuffers();
    bool AllocateEntry(Entry& entry);
    void Requeue(Entry& entry, Mesh* mesh);
    bool RequeueForReplacement(Entry& entry);
    bool EvictOne();
    bool IsWorthBuilding(const Entry& entry, uint32 frameCounter, float farthestDemotablePriority) const;
    bool MakeContiguousRoom(const Entry& requester, uint32 nodeCount, uint32 triangleCount, uint32& numDemotions);
    void Defer(Entry& entry, uint32 frameCounter);
    void FreeEntryRanges(Entry& entry);
    void UploadEntry(Frame* frame, Entry& entry);

    Map<uint64, UniquePtr<Entry>> m_entries;

    UniquePtr<TaskThreadPool> m_buildPool;

    GpuBufferRef m_nodesBuffer;
    GpuBufferRef m_trianglesBuffer;

    GlimmerPoolAllocator m_nodeAllocator;
    GlimmerPoolAllocator m_triangleAllocator;

    Array<DeferredFree> m_deferredFrees;
    Array<uint64> m_keysToErase; // entries to drop after the Update() loop, which may be iterating them

    uint32 m_residentGeneration;
    uint32 m_evictionGeneration;
    uint32 m_numBuildsInFlight;
    uint32 m_lastUpdateFrame;
    uint32 m_lastPoolFullLogFrame;
};

} // namespace Hyperion
