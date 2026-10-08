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
    static constexpr uint32 GridMax = 65535;

    uint16 grid[9];
    uint16 leafFlags;
};

static_assert(sizeof(GlimmerTriangle) == 20);

struct GlimmerTriangleLightmapUVs
{
    uint16 uvs[6]; //the mesh's UV1 at each corner, unorm
};

static_assert(sizeof(GlimmerTriangleLightmapUVs) == 12);

struct GlimmerBLASRef
{
    uint64 key = 0;
    uint32 nodeBase = 0;
    uint32 nodeCount = 0;
    uint32 triangleBase = 0;
    uint32 triangleCount = 0;
    uint32 lightmapUVBase = ~0u; // ~0u for a mesh without lightmap UVs, or when their pool is full
    uint32 depth = 0;
    
    BoundingBox localBounds;

    Vec3f gridOrigin = Vec3f::Zero();
    Vec3f gridScale = Vec3f::One();
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
    uint32 lightmapUVsUsed = 0;
    uint32 lightmapUVsCapacity = 0;
};

class GlimmerBLASCache final
{
public:
    GlimmerBLASCache();

    GlimmerBLASCache(const GlimmerBLASCache& other) = delete;
    GlimmerBLASCache& operator=(const GlimmerBLASCache& other) = delete;

    ~GlimmerBLASCache();

    static uint64 MakeKey(const Mesh* mesh, uint8 lodIndex);

    ///\p priority  how far (m) the nearest instance asking for it is from the viewer.
    ///             nearer meshes are built and given pool space first,
    ///             and push out the BLASes of meshes much farther away when the pool is full
    GlimmerBLASRequestResult Request(Mesh* mesh, uint8 lodIndex, float priority, GlimmerBLASRef& outRef);

    /// Try to get blas ref for the given params. Will not create one if none exists.
    bool TryGetResident(Mesh* mesh, uint8 lodIndex, float priority, GlimmerBLASRef& outRef);

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

    /// Bumped when the TLASes have to gather again: resident BLASes were demoted to make room (they're freed once no TLAS references
    /// them), or the LOD error scale changed
    HYP_FORCE_INLINE uint32 GetEvictionGeneration() const
    {
        return m_evictionGeneration;
    }

    /// Scales the geometric error the TLASes allow a mesh's LOD; above 1 while the pool can't hold everything in range
    HYP_FORCE_INLINE float GetLodErrorScale() const
    {
        return m_lodErrorScale;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetNodesBuffer() const
    {
        return m_nodesBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetTrianglesBuffer() const
    {
        return m_trianglesBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetLightmapUVsBuffer() const
    {
        return m_lightmapUVsBuffer;
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
        Array<GlimmerBLASNode> nodes;
        Array<GlimmerTriangle> triangles;
        Array<GlimmerTriangleLightmapUVs> lightmapUVs; // one per triangle, or empty
        BoundingBox localBounds;
        Vec3f gridOrigin = Vec3f::Zero();
        Vec3f gridScale = Vec3f::One();
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
        uint32 lightmapUVOffset;
        uint32 lightmapUVCount;
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
    GpuBufferRef m_lightmapUVsBuffer;

    GlimmerPoolAllocator m_nodeAllocator;
    GlimmerPoolAllocator m_triangleAllocator;
    GlimmerPoolAllocator m_lightmapUVAllocator;

    Array<DeferredFree> m_deferredFrees;
    Array<uint64> m_keysToErase; // entries to drop after the Update() loop, which may be iterating them

    uint32 m_residentGeneration;
    uint32 m_evictionGeneration;
    uint32 m_numBuildsInFlight;
    uint32 m_lastUpdateFrame;
    uint32 m_lastPoolFullLogFrame;
    float m_lodErrorScale;
    uint32 m_lastLodErrorScaleFrame;
    float m_poolUsageAtLodErrorScaleStep;
};

} // namespace Hyperion
