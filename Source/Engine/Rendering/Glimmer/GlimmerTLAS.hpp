/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/GlimmerBVHBuilder.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Threading/Task.hpp>

#include <Core/Math/BoundingBox.hpp>

namespace Hyperion {

class RenderProxyList;
class GlimmerBLASCache;

enum GlimmerInstanceFlags : uint32
{
    GIF_NONE = 0x0,
    GIF_DOUBLE_SIDED = 0x1,
    GIF_MIRRORED = 0x2, //!< negative determinant, so object space winding is flipped
    GIF_ALPHA_TESTED = 0x4,
    GIF_FOLIAGE = 0x8 //!< span instances only: splatted as canopy rather than solid
};

// Must match GlimmerInstance in Shaders/Glimmer/SWRT/GlimmerSWRTCommon.hlsli
struct GlimmerInstanceShaderData
{
    float worldToObject[12]; // first three rows, row major
    uint32 blasNodeBase;
    uint32 blasTriangleBase;
    uint32 materialIndex;
    uint32 flags;
};

static_assert(sizeof(GlimmerInstanceShaderData) == 64);

// Must match GlimmerInstanceBounds in Shaders/Glimmer/SWRT/GlimmerSWRTCommon.hlsli
struct GlimmerInstanceBoundsShaderData
{
    Vec4f min;
    Vec4f max;
};

static_assert(sizeof(GlimmerInstanceBoundsShaderData) == 32);

// Must match GlimmerSpanInstance in Shaders/Glimmer/GlimmerCommon.hlsli
struct GlimmerSpanInstanceShaderData
{
    float objectToWorld[12]; // first three rows, row major
    uint32 blasTriangleBase;
    uint32 triangleCount;
    uint32 materialIndex;
    uint32 flags;
};

static_assert(sizeof(GlimmerSpanInstanceShaderData) == 64);

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
};

/*! \brief The instances of one world's Glimmer region: every static solid and foliage instance as a span instance, for splatting into the
 *  heightfield, plus an instance level BVH over the solids inside the traced region for techniques that ray trace them (none if it's empty).
 *  Gathers from the Glimmer scene view's proxy list on the render thread and builds on a background thread, swapping in once ready.
 *  Render thread only. */
class GlimmerTLAS
{
public:
    GlimmerTLAS();
    GlimmerTLAS(const GlimmerTLAS& other) = delete;
    GlimmerTLAS& operator=(const GlimmerTLAS& other) = delete;
    ~GlimmerTLAS();

    /*! \brief Latches changes, starts a rebuild when due and swaps in a finished one.
     *  \return true on the frame a new TLAS was swapped in. */
    bool Update(Frame* frame, RenderProxyList& rpl, const BoundingBox& region, const BoundingBox& tracedRegion, GlimmerBLASCache& blasCache);

    /*! \brief Releases the BLAS references held by the active TLAS. Must be called before the BLAS cache goes away. */
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

    /*! \brief Every static solid and foliage instance in the (wider) span region, for splatting into the heightfield. */
    HYP_FORCE_INLINE const GpuBufferRef& GetSpanInstancesBuffer() const
    {
        return m_spanInstancesBuffer;
    }

    /*! \brief Exclusive prefix sum of the span instances' triangle counts, so a thread can find its instance from its triangle. */
    HYP_FORCE_INLINE const GpuBufferRef& GetSpanTriangleOffsetsBuffer() const
    {
        return m_spanTriangleOffsetsBuffer;
    }

    HYP_FORCE_INLINE uint32 GetNumSpanInstances() const
    {
        return m_stats.numSpanInstances;
    }

    HYP_FORCE_INLINE uint32 GetNumSpanTriangles() const
    {
        return m_stats.numSpanTriangles;
    }

    /*! \brief Bumped whenever a new TLAS is swapped in. */
    HYP_FORCE_INLINE uint32 GetGeneration() const
    {
        return m_stats.numBuilds;
    }

    HYP_FORCE_INLINE const GlimmerTLASStats& GetStats() const
    {
        return m_stats;
    }

private:
    struct BuildInput
    {
        Array<GlimmerInstanceShaderData> instances;
        Array<BoundingBox> instanceBounds;
        Array<GlimmerSpanInstanceShaderData> spanInstances;
        Array<uint64> blasKeys; // unique
    };

    struct BuildResult
    {
        Array<GlimmerBVHNode> nodes;
        Array<GlimmerInstanceShaderData> instances;
        Array<GlimmerInstanceBoundsShaderData> instanceBounds;
        Array<GlimmerSpanInstanceShaderData> spanInstances;
        Array<uint32> spanTriangleOffsets;
        uint32 numSpanTriangles = 0;
        Array<uint64> blasKeys;
        uint32 depth = 0;
        double buildMs = 0.0;
    };

    void Gather(RenderProxyList& rpl, const BoundingBox& region, const BoundingBox& tracedRegion, GlimmerBLASCache& blasCache, BuildInput& outInput, uint32& outNumWaitingForBLAS);

    static BuildResult Build(BuildInput&& input);

    void Upload(Frame* frame, BuildResult& result);

    GpuBufferRef m_nodesBuffer;
    GpuBufferRef m_instancesBuffer;
    GpuBufferRef m_instanceBoundsBuffer;
    GpuBufferRef m_spanInstancesBuffer;
    GpuBufferRef m_spanTriangleOffsetsBuffer;

    Array<uint64> m_activeBlasKeys;

    Task<BuildResult> m_buildTask;
    Array<uint64> m_pendingBlasKeys; // referenced while the build is in flight so the BLASes stay resident

    BoundingBox m_lastRegion;
    BoundingBox m_lastTracedRegion;
    uint64 m_lastBuildStartTime;
    uint32 m_blasGenerationAtGather;
    bool m_dirty;
    bool m_waitingForBLAS;

    GlimmerTLASStats m_stats;
};

} // namespace Hyperion
