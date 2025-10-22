/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Triangle.hpp>
#include <core/math/Vertex.hpp>
#include <core/math/Ray.hpp>

#include <core/Defines.hpp>

namespace hyperion {

/// reference: https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/bvh.html

HYP_ENUM()
enum BvhFlags : uint32
{
    BF_NONE = 0x0,
    BF_IS_LEAF_NODE = 0x1 //<! Is a leaf node
};

HYP_MAKE_ENUM_FLAGS(BvhFlags)

HYP_STRUCT()
struct HYP_API BVHNode
{
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
        Span<const Vertex> vertices,
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
        Span<const Vertex> vertices,
        Span<const uint32> indices) const
    {
        RayTestResults results;

        if (!ray.TestAABB(aabb))
        {
            return results;
        }

        if (IsLeafNode())
        {
            for (SizeType t = 0; t < triangleIds.Size(); ++t)
            {
                const uint32 triangleId = triangleIds[t];
                const uint32 i0 = indices[triangleId * 3 + 0];
                const uint32 i1 = indices[triangleId * 3 + 1];
                const uint32 i2 = indices[triangleId * 3 + 2];

                const Triangle tri {
                    vertices[i0].position,
                    vertices[i1].position,
                    vertices[i2].position
                };

                ray.TestTriangle(tri, triangleId, this, results);
            }
        }
        else
        {
            for (const BVHNode& node : children)
            {
                results.Merge(node.TestRay(ray, vertices, indices));
            }
        }

        return results;
    }

    HYP_NODISCARD RayTestResults TestRay(
        const Ray& ray,
        Span<const Vec3f> positions,
        Span<const uint32> indices) const
    {
        RayTestResults results;

        if (!ray.TestAABB(aabb))
        {
            return results;
        }

        if (IsLeafNode())
        {
            for (SizeType t = 0; t < triangleIds.Size(); ++t)
            {
                const uint32 triangleId = triangleIds[t];
                const uint32 i0 = indices[triangleId * 3 + 0];
                const uint32 i1 = indices[triangleId * 3 + 1];
                const uint32 i2 = indices[triangleId * 3 + 2];

                const Triangle tri {
                    positions[i0],
                    positions[i1],
                    positions[i2]
                };

                ray.TestTriangle(tri, triangleId, this, results);
            }
        }
        else
        {
            for (const BVHNode& node : children)
            {
                results.Merge(node.TestRay(ray, positions, indices));
            }
        }

        return results;
    }

private:
    static bool AabbOverlapsTriangleId(const BoundingBox& box,
        Span<const Vertex> vertices,
        Span<const uint32> indices,
        uint32 triId)
    {
        const uint32 i0 = indices[triId * 3 + 0];
        const uint32 i1 = indices[triId * 3 + 1];
        const uint32 i2 = indices[triId * 3 + 2];

        const Triangle tri {
            vertices[i0].position,
            vertices[i1].position,
            vertices[i2].position
        };

        return box.OverlapsTriangle(tri);
    }

    static void QuantizeTriangleData(
        Span<const Vertex> vertexData,
        Span<const uint32> indexData,
        ByteBuffer& outQuantizedVertexData,
        ByteBuffer& outQuantizedIndexData);

    void Split_Internal(int depth, int maxDepth,
        Span<const Vertex> vertices,
        Span<const uint32> indices)
    {
        if (IsLeafNode() && triangleIds.Any() && depth < maxDepth)
        {
            const Vec3f center = aabb.GetCenter();
            const Vec3f extent = aabb.GetExtent();

            const Vec3f& min = aabb.GetMin();
            const Vec3f& max = aabb.GetMax();

            // 8 children (octants), same as your original
            for (int i = 0; i < 2; i++)
            {
                for (int j = 0; j < 2; j++)
                {
                    for (int k = 0; k < 2; k++)
                    {
                        const Vec3f newMin(
                            i == 0 ? min.x : center.x,
                            j == 0 ? min.y : center.y,
                            k == 0 ? min.z : center.z);

                        const Vec3f newMax(
                            i == 0 ? center.x : max.x,
                            j == 0 ? center.y : max.y,
                            k == 0 ? center.z : max.z);

                        children.EmplaceBack(BoundingBox(newMin, newMax));
                    }
                }
            }

            for (const uint32 triId : triangleIds)
            {
                for (BVHNode& node : children)
                {
                    if (AabbOverlapsTriangleId(node.aabb, vertices, indices, triId))
                    {
                        node.triangleIds.PushBack(triId);
                    }
                }
            }

            triangleIds.Clear();
            triangleIds.Refit();

            flags[BF_IS_LEAF_NODE] = false;
        }

        for (BVHNode& node : children)
        {
            node.Split_Internal(depth + 1, maxDepth, vertices, indices);
        }
    }

    void Shake_Internal()
    {
        if (IsLeafNode())
        {
            return;
        }

        for (auto it = children.Begin(); it != children.End();)
        {
            BVHNode& node = *it;

            if (node.IsLeafNode())
            {
                if (node.triangleIds.Empty())
                {
                    it = children.Erase(it);
                    continue;
                }
            }
            else
            {
                node.Shake_Internal();
            }

            ++it;
        }

        if (children.Empty())
        {
            flags[BF_IS_LEAF_NODE] = true;
        }
    }
};

} // namespace hyperion
