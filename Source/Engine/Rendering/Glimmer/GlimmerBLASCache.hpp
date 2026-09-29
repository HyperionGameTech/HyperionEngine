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

#include <Core/Threading/Task.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/BoundingBox.hpp>

namespace Hyperion {

class Mesh;

// Must match BVHTriangle in Shaders/Include/RayTracing/BVH.hlsli
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

/*! \brief Where a resident BLAS lives in the shared pool buffers. Node and triangle indices inside it are local. */
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

/*! \brief First fit allocator over element ranges of a fixed capacity buffer. */
class GlimmerPoolAllocator
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
    uint32 numResidentTriangles = 0;
    uint32 nodesUsed = 0;
    uint32 nodesCapacity = 0;
    uint32 trianglesUsed = 0;
    uint32 trianglesCapacity = 0;
};

/*! \brief Per (mesh, LOD) bottom level BVHs in mesh local space, shared by every world.
 *  Builds run on background threads; results are uploaded into fixed size pool buffers and evicted LRU once no active TLAS references them.
 *  Render thread only. */
class GlimmerBLASCache
{
public:
    GlimmerBLASCache();
    GlimmerBLASCache(const GlimmerBLASCache& other) = delete;
    GlimmerBLASCache& operator=(const GlimmerBLASCache& other) = delete;
    ~GlimmerBLASCache();

    static uint64 MakeKey(const Mesh* mesh, uint8 lodIndex);

    /*! \brief Returns true with outRef filled if the BLAS is resident. Otherwise queues a build (once) and returns false. */
    bool Request(Mesh* mesh, uint8 lodIndex, GlimmerBLASRef& outRef);

    /*! \brief Increments / decrements the count of active TLASes referencing each key. Referenced entries are never evicted. */
    void AddReferences(Span<const uint64> keys);
    void RemoveReferences(Span<const uint64> keys);

    /*! \brief Polls finished builds, uploads within the frame budget and recycles freed ranges. Call once per frame before any TLAS reads. */
    void Update(Frame* frame);

    /*! \brief Bumped whenever an entry becomes resident, so TLASes waiting on BLASes know to rebuild. */
    HYP_FORCE_INLINE uint32 GetResidentGeneration() const
    {
        return m_residentGeneration;
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
    };

    struct Entry
    {
        EntryState state = EntryState::Queued;
        Handle<Mesh> mesh; // only held until the build finishes
        WeakHandle<Mesh> meshWeak;
        uint8 lodIndex = 0;
        Task<BuildResult> buildTask;
        BuildResult result;
        GlimmerBLASRef ref;
        uint32 numReferences = 0;
        uint32 lastUsedFrame = 0;
        uint32 failedFrame = 0;
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
    bool EvictOne();
    void FreeEntryRanges(Entry& entry);
    void UploadEntry(Frame* frame, Entry& entry);

    Map<uint64, UniquePtr<Entry>> m_entries;

    GpuBufferRef m_nodesBuffer;
    GpuBufferRef m_trianglesBuffer;

    GlimmerPoolAllocator m_nodeAllocator;
    GlimmerPoolAllocator m_triangleAllocator;

    Array<DeferredFree> m_deferredFrees;
    Array<uint64> m_keysToErase; // entries to drop after the Update() loop, which may be iterating them

    uint32 m_residentGeneration;
    uint32 m_numBuildsInFlight;
};

} // namespace Hyperion
