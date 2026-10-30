/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RawBuffer.hpp>
#include <Rendering/Glimmer/GlimmerMemory.hpp>

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
    uint16 leafFlags; // GlimmerBLASNode::LeafEndFlag on the last triangle of a BLAS leaf
};

static_assert(sizeof(GlimmerTriangle) == 20);

/*  Compressed BLAS node
 *
 *    words[0..2]  node bounds origin (3x floats)
 *    words[3]     bits 0-17: per-axis scale exponents, 6 bits each, biased by 32 (scale = 2^(value - 32));
 *                 bits 18-31: bits 9-22 of the right child ref
 *    words[4..6]  child bounds, one byte per component. left min xyz, left max xyz, right min xyz, right max xyz
 *    words[7]     bits 0-22: left child ref, bits 23-31: bits 0-8 of the right child ref
 *
 *  A 'child ref' is bit 22 = child is a leaf bits 0-21 = node index (interior) or first triangle (leaf).
 *  Leaves have no count: the last triangle of a leaf has GlimmerBLASNode::LeafEndFlag set in its leafFlags. */
struct GlimmerBLASNode
{
    static constexpr uint32 RefLeafBit = 1u << 22;
    static constexpr uint32 RefIndexMask = RefLeafBit - 1;
    static constexpr uint32 MaxIndex = RefIndexMask;
    static constexpr uint32 LeafEndFlag = 1u;

    uint32 words[8];
};

static_assert(sizeof(GlimmerBLASNode) == 32);

struct GlimmerTriangleLightmapUVs
{
    uint16 uvs[6]; //the mesh's UV1 at each corner, unorm
};

static_assert(sizeof(GlimmerTriangleLightmapUVs) == 12);

struct GlimmerTriangleTexcoords
{
    int16 base[2]; // whole repeats taken off every corner, so the halves stay precise on tiled surfaces
    uint16 uvs[6]; // UV0 at each corner less the base, as halves
};

static_assert(sizeof(GlimmerTriangleTexcoords) == 16);

struct GlimmerTriangleRef
{
    uint64 key = 0;
    uint32 nodeBase = 0;
    uint32 nodeCount = 0;           // 0 without a BLAS (the node pool is off)
    uint32 depth = 0;
    uint32 triangleBase = 0;
    uint32 triangleCount = 0;
    uint32 lightmapUVBase = ~0u;    // ~0u for a mesh without lightmap UVs, or when their pool is full
    uint32 texcoordBase = ~0u;      // ~0u when their pool is full
    
    BoundingBox localBounds;

    Vec3f gridOrigin = Vec3f::Zero();
    Vec3f gridScale = Vec3f::One();
};

enum class GlimmerTriangleRequestResult : uint8
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

    Array<Range, GlimmerAllocator> m_freeRanges; // sorted by offset, never adjacent
    
    uint32 m_capacity = 0;
    uint32 m_numUsed = 0;
};

struct GlimmerTriangleCacheStats
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
    uint32 texcoordsUsed = 0;
    uint32 texcoordsCapacity = 0;
};

class GlimmerTriangleCache final
{
public:
    GlimmerTriangleCache();

    GlimmerTriangleCache(const GlimmerTriangleCache& other) = delete;
    GlimmerTriangleCache& operator=(const GlimmerTriangleCache& other) = delete;

    ~GlimmerTriangleCache();

    static uint64 MakeKey(const Mesh* mesh, uint8 lodIndex);

    static SharedPtr<GlimmerTriangleCache> AcquireShared();

    ///\p priority  how far (m) the nearest instance asking for it is from the viewer.
    ///             nearer meshes are built and given pool space first,
    ///             and push out the triangles of meshes much farther away when the pool is full
    GlimmerTriangleRequestResult Request(
        Mesh* mesh,
        uint8 lodIndex,
        float priority,
        GlimmerTriangleRef& outRef);

    /// Try to get a triangle ref for the given params. Will not create one if none exists.
    bool TryGetResident(
        Mesh* mesh,
        uint8 lodIndex,
        float priority,
        GlimmerTriangleRef& outRef);

    void AddReferences(Span<const uint64> keys);
    void RemoveReferences(Span<const uint64> keys);

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

    /// Bumped when the scene instances have to gather again.
    HYP_FORCE_INLINE uint32 GetEvictionGeneration() const
    {
        return m_evictionGeneration;
    }

    /// Scales the geometric error the scene instance gather allows a mesh's LOD.
    /// above 1 while the pool can't hold everything in range.
    HYP_FORCE_INLINE float GetLodErrorScale() const
    {
        return m_lodErrorScale;
    }

    HYP_FORCE_INLINE const StructuredBuffer& GetNodesBuffer() const
    {
        return m_nodesBuffer;
    }

    HYP_FORCE_INLINE bool HasBLAS() const
    {
        return m_nodesBuffer.IsInitialized();
    }

    HYP_FORCE_INLINE const StructuredBuffer& GetTrianglesBuffer() const
    {
        return m_trianglesBuffer;
    }

    HYP_FORCE_INLINE const StructuredBuffer& GetTexcoordsBuffer() const
    {
        return m_texcoordsBuffer;
    }

    HYP_FORCE_INLINE const StructuredBuffer& GetLightmapUVsBuffer() const
    {
        return m_lightmapUVsBuffer;
    }

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_trianglesBuffer.IsInitialized();
    }

    GlimmerTriangleCacheStats GetStats() const;

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
        Array<GlimmerBLASNode, GlimmerAllocator> nodes;                     // empty when the node pool is off
        Array<GlimmerTriangle, GlimmerAllocator> triangles;                 // in BLAS leaf order when there are nodes
        Array<GlimmerTriangleLightmapUVs, GlimmerAllocator> lightmapUVs;    // one per triangle, or empty
        Array<GlimmerTriangleTexcoords, GlimmerAllocator> texcoords;        // one per triangle

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

        GlimmerTriangleRef ref;

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
        uint32 texcoordOffset;
        uint32 texcoordCount;
    };

    static BuildResult BuildMeshTriangles(const Handle<Mesh>& mesh, uint8 lodIndex, bool withBLAS);

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

    Map<uint64, UniquePtr<Entry, GlimmerAllocator>, GlimmerAllocator> m_entries;

    uint32 m_numBuildThreads;
    UniquePtr<TaskThreadPool> m_buildPool;

    StructuredBuffer m_nodesBuffer;
    StructuredBuffer m_trianglesBuffer;
    StructuredBuffer m_lightmapUVsBuffer;
    StructuredBuffer m_texcoordsBuffer;

    GlimmerPoolAllocator m_nodeAllocator;
    GlimmerPoolAllocator m_triangleAllocator;
    GlimmerPoolAllocator m_lightmapUVAllocator;
    GlimmerPoolAllocator m_texcoordAllocator;

    Array<DeferredFree, GlimmerAllocator> m_deferredFrees;
    Array<uint64, GlimmerAllocator> m_keysToErase; // entries to drop after the Update() loop, which may be iterating them

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
