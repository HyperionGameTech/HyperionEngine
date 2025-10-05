/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Triangle.hpp>
#include <core/math/Vertex.hpp>
#include <core/math/Ray.hpp>

#include <core/Defines.hpp>

namespace hyperion {

/// reference: https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/bvh.html

HYP_STRUCT()
struct HYP_API BVHNode
{
    HYP_STRUCT_BODY(BVHNode);

    HYP_FIELD(Serialize)
    BoundingBox aabb;

    HYP_FIELD(Serialize)
    LinkedList<BVHNode> children;

    HYP_FIELD(Serialize)
    Array<Triangle, DynamicAllocator> triangles; // temp; replace with quantized data

    HYP_FIELD(Serialize, Compressed)
    ByteBuffer vertexData;

    HYP_FIELD(Serialize, Compressed)
    ByteBuffer indexData;

    HYP_FIELD(Serialize)
    bool isLeafNode = false;

    BVHNode() = default;

    BVHNode(const BoundingBox& aabb)
        : aabb(aabb),
          isLeafNode(true)
    {
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return aabb.IsValid() && aabb.IsFinite();
    }

    HYP_FORCE_INLINE void AddTriangle(const Triangle& triangle)
    {
        triangles.PushBack(triangle);
    }

    void Split(int maxDepth = 3)
    {
        Split_Internal(0, maxDepth);
    }

    void Shake()
    {
        Shake_Internal();
    }

    HYP_NODISCARD RayTestResults TestRay(const Ray& ray) const
    {
        RayTestResults results;

        if (ray.TestAABB(aabb))
        {
            if (isLeafNode)
            {
                for (SizeType triangleIndex = 0; triangleIndex < triangles.Size(); triangleIndex++)
                {
                    const Triangle& triangle = triangles[triangleIndex];

                    ray.TestTriangle(triangle, triangleIndex, this, results);
                }
            }
            else
            {
                for (const BVHNode& node : children)
                {
                    results.Merge(node.TestRay(ray));
                }
            }
        }

        return results;
    }

private:
    static void QuantizeTriangleData(
        Span<const Vertex> vertexData,
        Span<const uint32> indexData,
        ByteBuffer& outQuantizedVertexData,
        ByteBuffer& outQuantizedIndexData);

    void Split_Internal(int depth, int maxDepth)
    {
        if (isLeafNode)
        {
            if (triangles.Any())
            {
                if (depth < maxDepth)
                {
                    const Vec3f center = aabb.GetCenter();
                    const Vec3f extent = aabb.GetExtent();

                    const Vec3f& min = aabb.GetMin();
                    const Vec3f& max = aabb.GetMax();

                    for (int i = 0; i < 2; i++)
                    {
                        for (int j = 0; j < 2; j++)
                        {
                            for (int k = 0; k < 2; k++)
                            {
                                const Vec3f newMin = Vec3f(
                                    i == 0 ? min.x : center.x,
                                    j == 0 ? min.y : center.y,
                                    k == 0 ? min.z : center.z);

                                const Vec3f newMax = Vec3f(
                                    i == 0 ? center.x : max.x,
                                    j == 0 ? center.y : max.y,
                                    k == 0 ? center.z : max.z);

                                children.EmplaceBack(BoundingBox(newMin, newMax));
                            }
                        }
                    }

                    for (const Triangle& triangle : triangles)
                    {
                        for (BVHNode& node : children)
                        {
                            if (node.aabb.OverlapsTriangle(triangle))
                            {
                                node.triangles.PushBack(triangle);
                            }
                        }
                    }

                    triangles.Clear();
                    triangles.Refit();

                    isLeafNode = false;
                }
            }
        }

        for (BVHNode& node : children)
        {
            node.Split_Internal(depth + 1, maxDepth);
        }
    }

    void Shake_Internal()
    {
        if (isLeafNode)
        {
            return;
        }

        for (auto it = children.Begin(); it != children.End();)
        {
            BVHNode& node = *it;

            if (node.isLeafNode)
            {
                if (node.triangles.Empty())
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
            isLeafNode = true;
        }
    }
};

} // namespace hyperion
