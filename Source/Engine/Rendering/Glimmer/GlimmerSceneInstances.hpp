/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RawBuffer.hpp>

#include <Rendering/Glimmer/GlimmerMemory.hpp>

#include <Rendering/Util/SAHBVHBuilder.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Threading/Task.hpp>

#include <Core/Math/BoundingBox.hpp>

namespace Hyperion {

class RenderProxyList;
class GlimmerTriangleCache;

enum GlimmerInstanceFlags : uint32
{
    GIF_NONE = 0x0,
    GIF_DOUBLE_SIDED = 0x1,
    GIF_MIRRORED = 0x2,         //!< negative determinant
    GIF_ALPHA_TESTED = 0x4,
    GIF_FOLIAGE = 0x8           //!< span instances only: splatted as canopy rather than solid
};

struct GlimmerSpanInstanceShaderData
{
    float objectToWorld[12];

    uint32 triangleBase;
    uint32 triangleCount;
    uint32 materialIndex;
    uint32 flags;
    uint32 lightmapUVBase;      // ~0u when it has no lightmap to take its indirect light from
    uint32 lightmapRectOffset;
    uint32 lightmapRectSize;
    uint32 texcoordBase;        // ~0u when the triangle pool had no room for its UV0
};

static_assert(sizeof(GlimmerSpanInstanceShaderData) == 80);

struct GlimmerTracedInstanceShaderData
{
    float worldToObject[12];

    uint32 blasNodeBase;
    uint32 blasTriangleBase;

    uint32 materialIndex;
    
    uint32 flags;
};

static_assert(sizeof(GlimmerTracedInstanceShaderData) == 64);

static constexpr uint32 GlimmerSpanChunkTriangles = 256;

struct GlimmerSpanChunkShaderData
{
    float boundsMin[3]; // min bounds of the instance, so a splat can skip every chunk of an instance outside what it's filling
    uint32 spanInstance;
    float boundsMax[3];
    uint32 firstTriangle; // local to the instance
};

static_assert(sizeof(GlimmerSpanChunkShaderData) == 32);

static constexpr uint32 GlimmerSpanMaxDirtyBounds = 4;

struct GlimmerSceneChange
{
    BoundingBox bounds;
    bool isLodOnly = false; // same instance, transform and material. only the LOD its triangles were built from differs
};

struct GlimmerSceneChanges
{
    using Iterator = typename Array<GlimmerSceneChange, GlimmerAllocator>::Iterator;
    using ConstIterator = typename Array<GlimmerSceneChange, GlimmerAllocator>::ConstIterator;

    Array<GlimmerSceneChange, GlimmerAllocator> changes;

    void Quantize(size_t maxChanges);

    HYP_FORCE_INLINE size_t Size() const
    {
        return changes.Size();
    }

    HYP_FORCE_INLINE void Add(const GlimmerSceneChange& change)
    {
        changes.PushBack(change);
    }

    HYP_FORCE_INLINE void Add(GlimmerSceneChange&& change)
    {
        changes.PushBack(std::move(change));
    }

    HYP_DEF_STL_BEGIN_END(changes.Begin(), changes.End());
};

struct GlimmerSceneInstancesStats
{
    uint32 numWaitingForTriangles = 0;
    uint32 numBuilds = 0;
    double lastBuildMs = 0.0;
    uint32 numSpanInstances = 0;
    uint32 numSpanTriangles = 0;
    uint32 numSpanChunks = 0;
    uint32 numChanges = 0;        // num changes of the last build, before merging
    uint32 numLodOnlyChanges = 0;
    uint32 numEverywhereBuilds = 0;
    uint32 numTLASBuilds = 0;
};

class GlimmerSceneInstances final
{
public:
    struct SpanKey
    {
        uint64 hash;
        BoundingBox bounds;

        bool operator<(const SpanKey& other) const
        {
            return hash < other.hash;
        }
    };

    GlimmerSceneInstances();

    GlimmerSceneInstances(const GlimmerSceneInstances& other) = delete;
    GlimmerSceneInstances& operator=(const GlimmerSceneInstances& other) = delete;

    ~GlimmerSceneInstances();

    void Update(
        Frame* frame,
        RenderProxyList& rpl,
        const BoundingBox& region,
        const Vec3f& viewerPosition,
        GlimmerTriangleCache& triangleCache);

    void Release(GlimmerTriangleCache& triangleCache);

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_spanInstancesBuffer.IsInitialized();
    }

    HYP_FORCE_INLINE const StructuredBuffer& GetSpanInstancesBuffer() const
    {
        return m_spanInstancesBuffer;
    }

    HYP_FORCE_INLINE const StructuredBuffer& GetTLASNodesBuffer() const
    {
        return m_tlasNodesBuffer;
    }

    HYP_FORCE_INLINE const StructuredBuffer& GetTracedInstancesBuffer() const
    {
        return m_tracedInstancesBuffer;
    }

    HYP_FORCE_INLINE bool HasTLAS() const
    {
        return m_tlasNodesBuffer.IsInitialized() && m_numTracedInstances != 0;
    }

    HYP_FORCE_INLINE uint32 GetNumTracedInstances() const
    {
        return m_numTracedInstances;
    }

    HYP_FORCE_INLINE uint32 GetTLASDepth() const
    {
        return m_tlasDepth;
    }

    HYP_FORCE_INLINE const StructuredBuffer& GetSpanChunksBuffer() const
    {
        return m_spanChunksBuffer;
    }

    HYP_FORCE_INLINE uint32 GetNumSpanChunks() const
    {
        return m_stats.numSpanChunks;
    }

    HYP_FORCE_INLINE bool IsSpanFullyDirty() const
    {
        return m_spanFullyDirty;
    }

    HYP_FORCE_INLINE const Array<SpanKey, GlimmerAllocator>& GetSpanKeys() const
    {
        return m_spanKeys;
    }

    HYP_FORCE_INLINE const Array<BoundingBox, GlimmerAllocator>& GetSpanDirtyBounds() const
    {
        return m_spanDirtyBounds;
    }

    HYP_FORCE_INLINE uint32 GetNumSpanInstances() const
    {
        return m_stats.numSpanInstances;
    }

    HYP_FORCE_INLINE uint32 GetNumSpanTriangles() const
    {
        return m_stats.numSpanTriangles;
    }

    HYP_FORCE_INLINE uint32 GetGeneration() const
    {
        return m_stats.numBuilds;
    }

    bool GetChangesSince(
        uint32 seenGeneration,
        GlimmerSceneChanges& outChanges) const;

    HYP_FORCE_INLINE const BoundingBox& GetActiveRegion() const
    {
        return m_activeRegion;
    }

    HYP_FORCE_INLINE const GlimmerSceneInstancesStats& GetStats() const
    {
        return m_stats;
    }

private:
    struct InstanceRecord
    {
        uint64 identity;    // entity id << 32 | a hash of its transform within the entity
        uint64 worldHash;   // of its transform, mesh, material, flags and lightmap rect
        uint64 geometryKey; // the triangles it uses, which change with its LOD
        BoundingBox bounds;

        bool operator<(const InstanceRecord& other) const
        {
            return identity != other.identity ? identity < other.identity : worldHash < other.worldHash;
        }
    };

    struct ChangeSet
    {
        uint32 generation = 0;
        bool isEverywhere = false;
        GlimmerSceneChanges changes;
    };

    struct BuildInput
    {
        BoundingBox region;
        
        Vec3f regionCenter;
        
        Array<SpanKey, GlimmerAllocator> previousSpanKeys; // sorted; empty for the first build
        Array<InstanceRecord, GlimmerAllocator> previousInstanceRecords; // sorted
        
        Array<InstanceRecord, GlimmerAllocator> instanceRecords;
        
        bool hasPrevious = false;
        
        Array<GlimmerSpanInstanceShaderData, GlimmerAllocator> spanInstances;
        Array<BoundingBox, GlimmerAllocator> spanInstanceBounds;
        Array<uint64, GlimmerAllocator> spanMaterialHashes; // what GI reads of each span instance's material
        
        Array<GlimmerTracedInstanceShaderData, GlimmerAllocator> tracedInstances;
        Array<BoundingBox, GlimmerAllocator> tracedBounds;

        uint64 previousTLASHash = ~0ull;

        Array<uint64, GlimmerAllocator> meshKeys; // unique
    };

    struct BuildResult
    {
        BoundingBox region;
        
        Array<GlimmerSpanInstanceShaderData, GlimmerAllocator> spanInstances;
        
        Array<GlimmerSpanChunkShaderData, GlimmerAllocator> spanChunks;
        
        uint32 numSpanTriangles = 0;
        
        Array<uint64, GlimmerAllocator> meshKeys;
        
        double buildMs = 0.0;

        uint64 inputHash = 0; // of the gathered instances, so a rebuild that changed nothing can be dropped
        
        Array<SpanKey, GlimmerAllocator> spanKeys; // sorted
        Array<BoundingBox, GlimmerAllocator> spanDirtyBounds;
        bool spanFullyDirty = true;
        
        Array<InstanceRecord, GlimmerAllocator> instanceRecords; // sorted
        
        GlimmerSceneChanges changes;

        uint32 numLodOnlyChanges = 0;
        
        Array<SAHBVHNode, GlimmerAllocator> tlasNodes; // empty without traced instances
        
        Array<GlimmerTracedInstanceShaderData, GlimmerAllocator> tracedInstances; // in TLAS leaf order

        uint32 tlasDepth = 0;
        uint64 tlasHash = 0;        // of the traced instances as gathered
        bool isTLASUnchanged = false; // the uploaded TLAS is already this one, so nothing above was built
    };

    void Gather(
        RenderProxyList& rpl,
        const BoundingBox& region,
        const Vec3f& viewerPosition,
        GlimmerTriangleCache& triangleCache,
        BuildInput& outInput,
        uint32& outNumWaitingForTriangles);

    static BuildResult Build(BuildInput&& input);

    void Upload(Frame* frame, BuildResult& result);
    void UploadTLAS(Frame* frame, BuildResult& result);

    StructuredBuffer m_spanInstancesBuffer;
    StructuredBuffer m_spanChunksBuffer;
    
    StructuredBuffer m_tlasNodesBuffer;
    
    StructuredBuffer m_tracedInstancesBuffer;
    uint32 m_numTracedInstances = 0;

    uint32 m_tlasDepth = 0;
    uint64 m_activeTLASHash = ~0ull;

    Array<uint64, GlimmerAllocator> m_activeMeshKeys;

    Task<BuildResult> m_buildTask;
    Array<uint64, GlimmerAllocator> m_pendingMeshKeys; // referenced while the build is in flight so their triangles stay resident

    BoundingBox m_lastRegion;
    BoundingBox m_activeRegion;
    uint64 m_lastBuildStartTime;
    uint32 m_triangleGenerationAtGather;
    uint32 m_triangleEvictionGenerationAtGather;
    Vec3f m_viewerPositionAtGather;

    Map<uint64, uint8, GlimmerAllocator> m_instanceLods; // the LOD each instance (entity id << 32 | a hash of its transform within the entity) was given at the last gather
    bool m_dirty;
    bool m_waitingForTriangles;
    bool m_usesLightmaps;
    uint64 m_activeInputHash;

    Array<SpanKey, GlimmerAllocator> m_spanKeys;
    Array<BoundingBox, GlimmerAllocator> m_spanDirtyBounds;
    bool m_spanFullyDirty;

    Array<InstanceRecord, GlimmerAllocator> m_instanceRecords;
    Array<ChangeSet, GlimmerAllocator> m_changeJournal; // oldest first, one per generation

    GlimmerSceneInstancesStats m_stats;
};

} // namespace Hyperion
