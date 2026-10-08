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

#include <Core/Threading/Task.hpp>

#include <Core/Math/BoundingBox.hpp>

namespace Hyperion {

class RenderProxyList;
class GlimmerBLASCache;

enum GlimmerInstanceFlags : uint32
{
    GIF_NONE = 0x0,
    GIF_DOUBLE_SIDED = 0x1,
    GIF_MIRRORED = 0x2,         //!< negative determinant
    GIF_ALPHA_TESTED = 0x4,
    GIF_FOLIAGE = 0x8           //!< span instances only: splatted as canopy rather than solid
};

struct GlimmerInstanceShaderData
{
    float worldToObject[12]; // world to the BLAS's quantization grid space, first three rows, row major
    uint32 blasNodeBase;
    uint32 blasTriangleBase;
    uint32 materialIndex;
    uint32 flags;
};

static_assert(sizeof(GlimmerInstanceShaderData) == 64);

struct GlimmerInstanceBoundsShaderData
{
    Vec4f min;
    Vec4f max;
};

static_assert(sizeof(GlimmerInstanceBoundsShaderData) == 32);

struct GlimmerSpanInstanceShaderData
{
    float objectToWorld[12]; // BLAS quantization grid space to world, first three rows, row major
    uint32 blasTriangleBase;
    uint32 triangleCount;
    uint32 materialIndex;
    uint32 flags;
    uint32 lightmapUVBase; // ~0u when it has no lightmap to take its indirect light from
    uint32 lightmapRectOffset;
    uint32 lightmapRectSize;
    uint32 _pad0;
};

static_assert(sizeof(GlimmerSpanInstanceShaderData) == 80);

static constexpr uint32 GlimmerSpanChunkTriangles = 256;

struct GlimmerSpanChunkShaderData
{
    float boundsMin[3]; // of the instance, so a splat can skip every chunk of an instance outside what it's filling
    uint32 spanInstance;
    float boundsMax[3];
    uint32 firstTriangle; // local to the instance
};

static_assert(sizeof(GlimmerSpanChunkShaderData) == 32);

static constexpr uint32 GlimmerSpanMaxDirtyBounds = 4;

struct GlimmerTLASStats
{
    uint32 numInstances = 0;
    uint32 numNodes = 0;
    uint32 depth = 0;
    uint32 numWaitingForBLAS = 0;
    uint32 numBuilds = 0;
    double lastBuildMs = 0.0;
    uint32 numSpanInstances = 0;
    uint32 numSpanTriangles = 0;
    uint32 numSpanChunks = 0;
};

class GlimmerTLAS final
{
public:
    GlimmerTLAS();

    GlimmerTLAS(const GlimmerTLAS& other) = delete;
    GlimmerTLAS& operator=(const GlimmerTLAS& other) = delete;

    ~GlimmerTLAS();

    bool Update(
        Frame* frame,
        RenderProxyList& rpl,
        const BoundingBox& region,
        const BoundingBox& tracedRegion,
        const Vec3f& viewerPosition,
        GlimmerBLASCache& blasCache);
        
    void Release(GlimmerBLASCache& blasCache);

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_nodesBuffer.IsValid();
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetNodesBuffer() const
    {
        return m_nodesBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetInstancesBuffer() const
    {
        return m_instancesBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetInstanceBoundsBuffer() const
    {
        return m_instanceBoundsBuffer;
    }

    HYP_FORCE_INLINE uint32 GetNumInstances() const
    {
        return m_stats.numInstances;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetSpanInstancesBuffer() const
    {
        return m_spanInstancesBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetSpanChunksBuffer() const
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

    HYP_FORCE_INLINE const Array<BoundingBox>& GetSpanDirtyBounds() const
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

    HYP_FORCE_INLINE const BoundingBox& GetActiveRegion() const
    {
        return m_activeRegion;
    }

    HYP_FORCE_INLINE const GlimmerTLASStats& GetStats() const
    {
        return m_stats;
    }

private:
    struct SpanKey
    {
        uint64 hash;
        BoundingBox bounds;

        bool operator<(const SpanKey& other) const
        {
            return hash < other.hash;
        }
    };

    struct BuildInput
    {
        BoundingBox region;
        Vec3f regionCenter;
        Array<SpanKey> previousSpanKeys; // sorted; empty for the first build
        bool hasPrevious = false;
        Array<GlimmerInstanceShaderData> instances;
        Array<BoundingBox> instanceBounds;
        Array<GlimmerSpanInstanceShaderData> spanInstances;
        Array<BoundingBox> spanInstanceBounds;
        Array<uint64> blasKeys; // unique
    };

    struct BuildResult
    {
        BoundingBox region;
        Array<GlimmerBVHNode> nodes;
        Array<GlimmerInstanceShaderData> instances;
        Array<GlimmerInstanceBoundsShaderData> instanceBounds;
        Array<GlimmerSpanInstanceShaderData> spanInstances;
        Array<GlimmerSpanChunkShaderData> spanChunks;
        uint32 numSpanTriangles = 0;
        Array<uint64> blasKeys;
        uint32 depth = 0;
        double buildMs = 0.0;
        uint64 inputHash = 0; // of the gathered instances, so a rebuild that changed nothing can be dropped
        Array<SpanKey> spanKeys; // sorted
        Array<BoundingBox> spanDirtyBounds;
        bool spanFullyDirty = true;
    };

    void Gather(
        RenderProxyList& rpl,
        const BoundingBox& region,
        const BoundingBox& tracedRegion,
        const Vec3f& viewerPosition,
        GlimmerBLASCache& blasCache,
        BuildInput& outInput,
        uint32& outNumWaitingForBLAS);

    static BuildResult Build(BuildInput&& input);

    void Upload(Frame* frame, BuildResult& result);

    GpuBufferRef m_nodesBuffer;
    GpuBufferRef m_instancesBuffer;
    GpuBufferRef m_instanceBoundsBuffer;
    GpuBufferRef m_spanInstancesBuffer;
    GpuBufferRef m_spanChunksBuffer;

    Array<uint64> m_activeBlasKeys;

    Task<BuildResult> m_buildTask;
    Array<uint64> m_pendingBlasKeys; // referenced while the build is in flight so the BLASes stay resident

    BoundingBox m_lastRegion;
    BoundingBox m_lastTracedRegion;
    BoundingBox m_activeRegion;
    uint64 m_lastBuildStartTime;
    uint32 m_blasGenerationAtGather;
    uint32 m_blasEvictionGenerationAtGather;
    Vec3f m_viewerPositionAtGather;

    Map<uint64, uint8> m_instanceLods; // the LOD each instance (entity id << 32 | instance index) was given at the last gather
    bool m_dirty;
    bool m_waitingForBLAS;
    bool m_usesLightmaps;
    uint64 m_activeInputHash;

    Array<SpanKey> m_spanKeys;
    Array<BoundingBox> m_spanDirtyBounds;
    bool m_spanFullyDirty;

    GlimmerTLASStats m_stats;
};

} // namespace Hyperion
