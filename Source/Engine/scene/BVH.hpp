/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

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

namespace Hyperion {

/// reference: https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/bvh.html

HYP_ENUM()
enum BvhFlags : uint32
{
    BF_NONE = 0x0,
    BF_IS_LEAF_NODE = 0x1 //<! Is a leaf node
};

HYP_MAKE_ENUM_FLAGS(BvhFlags)

HYP_STRUCT()
class HYP_API BVHNode
{
public:
    HYP_STRUCT_BODY(BVHNode);

    HYP_FIELD(Serialize)
    BoundingBox aabb;

    HYP_FIELD(Serialize)
    Array<BVHNode, DynamicAllocator> children;

    HYP_FIELD(Serialize)
    Array<uint32, DynamicAllocator> triangleIds;

    HYP_FIELD(Serialize)
    EnumFlags<BvhFlags> flags = BF_NONE;

    BVHNode() = default;

    BVHNode(const BoundingBox& aabb)
        : aabb(aabb),
          flags(BF_IS_LEAF_NODE)
    {
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return aabb.IsValid() && aabb.IsFinite();
    }

    HYP_FORCE_INLINE bool IsLeafNode() const
    {
        return flags[BF_IS_LEAF_NODE];
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
