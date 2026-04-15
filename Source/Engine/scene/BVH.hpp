/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/Array.hpp>
#include <Core/containers/LinkedList.hpp>

#include <Core/utilities/EnumFlags.hpp>

#include <Core/memory/ByteBuffer.hpp>

#include <Core/math/BoundingBox.hpp>
#include <Core/math/Triangle.hpp>
#include <rendering/Vertex.hpp>
#include <Core/math/Ray.hpp>

#include <Core/Defines.hpp>
#include <Core/io/ByteWriter.hpp>
#include <Core/io/ByteReader.hpp>

namespace Hyperion {

/// reference: https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/bvh.html

HYP_STRUCT()
class HYP_API BVHNode
{
public:
    HYP_STRUCT_BODY(BVHNode);

    BoundingBox aabb;
    Array<BVHNode, DynamicAllocator> children;
    Array<uint32, DynamicAllocator> triangleIds;
    bool isLeafNode : 1;

    BVHNode()
        : isLeafNode(true)
    {
    }

    explicit BVHNode(const BoundingBox& aabb)
        : aabb(aabb),
          isLeafNode(true)
    {
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return aabb.IsValid() && aabb.IsFinite();
    }

    HYP_FORCE_INLINE bool IsLeafNode() const
    {
        return isLeafNode;
    }

    HYP_FORCE_INLINE void AddTriangleId(uint32 triId)
    {
        triangleIds.PushBack(triId);
    }

    void Split(int maxDepth,
        const VertexArrayView& vertices,
        Span<const uint32> indices)
    {
        Split_Internal(0, maxDepth, vertices, indices);
    }

    void Shake()
    {
        Shake_Internal();
    }

    HYP_NODISCARD RayTestResults TestRay(
        const Ray& ray,
        const VertexArrayView& vertices,
        Span<const uint32> indices) const;

    HYP_NODISCARD RayTestResults TestRay(
        const Ray& ray,
        Span<const Vec3f> positions,
        Span<const uint32> indices) const;

    /*! \brief Serializes the BVH tree rooted at \p node into a flat byte buffer. */
    static ByteBuffer Serialize(const BVHNode& node);

    /*! \brief Deserializes a BVH tree from \p data into \p outNode.
     *  \returns true on success, false if the data is malformed. */
    static bool Deserialize(BVHNode& outNode, const void* data, size_t size);

private:
    static bool OverlapsTriangle(
        const BoundingBox& box,
        const VertexArrayView& vertices,
        Span<const uint32> indices,
        uint32 triangleId);

    static void QuantizeTriangleData(
        const VertexArrayView& vertices,
        Span<const uint32> indexData,
        ByteBuffer& outQuantizedVertexData,
        ByteBuffer& outQuantizedIndexData);

    void Split_Internal(
        int depth, int maxDepth,
        const VertexArrayView& vertices,
        Span<const uint32> indices);

    void Shake_Internal();
};

} // namespace Hyperion
