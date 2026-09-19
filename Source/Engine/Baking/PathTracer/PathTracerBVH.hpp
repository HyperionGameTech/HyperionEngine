/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Threading/Task.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Mat4f.hpp>

#include <Core/Utilities/Span.hpp>

namespace Hyperion {

class Mesh;
class RenderProxyList;

namespace Baking {

struct PathTracerBVHNode
{
    float leftMin[3];
    uint32 leftIndex; //!< Child node index, or first triangle when the child is a leaf
    float leftMax[3];
    uint32 leftCount; //!< Leaf triangle count, 0 when the child is an interior node
    float rightMin[3];
    uint32 rightIndex;
    float rightMax[3];
    uint32 rightCount;
};

static_assert(sizeof(PathTracerBVHNode) == 64);

struct PathTracerTriangle
{
    float position0[3];
    uint32 padding0;
    float edge1[3];
    uint32 padding1;
    float edge2[3];
    uint32 padding2;
};

static_assert(sizeof(PathTracerTriangle) == 48);

struct PathTracerTriangleAttributes
{
    uint32 packedNormals[3]; //!< World space, octahedral snorm16x2
    uint32 materialIndex;
    float texcoords[6];
    uint32 padding[2];
};

static_assert(sizeof(PathTracerTriangleAttributes) == 48);

class PathTracerBVHBuilder
{
public:
    static void Build(Span<const PathTracerTriangle> triangles, Array<PathTracerBVHNode>& outNodes, Array<uint32>& outTriangleOrder);
};

enum class PathTracerBVHState : uint8
{
    NotBuilt = 0,
    Building,
    Ready,
    Failed
};

class PathTracerBVH final
{
public:
    PathTracerBVH();

    PathTracerBVH(const PathTracerBVH& other) = delete;
    PathTracerBVH& operator=(const PathTracerBVH& other) = delete;

    PathTracerBVH(PathTracerBVH&& other) noexcept = delete;
    PathTracerBVH& operator=(PathTracerBVH&& other) noexcept = delete;

    ~PathTracerBVH();

    HYP_FORCE_INLINE const GpuBufferRef& GetNodesBuffer() const
    {
        return m_nodesBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetTrianglesBuffer() const
    {
        return m_trianglesBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetTriangleAttributesBuffer() const
    {
        return m_triangleAttributesBuffer;
    }

    PathTracerBVHState Prepare(RenderProxyList& rpl);

private:
    struct Instance
    {
        Handle<Mesh> mesh;
        Mat4f transform;
        uint32 materialIndex;
    };

    struct BuildResult
    {
        Array<PathTracerBVHNode> nodes;
        Array<PathTracerTriangle> triangles;
        Array<PathTracerTriangleAttributes> triangleAttributes;
    };

    void GatherInstances(RenderProxyList& rpl);
    void Upload(const BuildResult& buildResult);

    static BuildResult Build(Span<const Instance> instances);

    PathTracerBVHState m_state;

    // Kept alive until destruction, so the mesh handles aren't released from the build or render thread
    Array<Instance> m_instances;
    Task<BuildResult> m_buildTask;

    GpuBufferRef m_nodesBuffer;
    GpuBufferRef m_trianglesBuffer;
    GpuBufferRef m_triangleAttributesBuffer;
};

} // namespace Baking

} // namespace Hyperion
