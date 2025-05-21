/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BVH_HPP
#define HYPERION_BVH_HPP

#include <core/containers/Array.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Triangle.hpp>
#include <core/math/Vertex.hpp>
#include <core/math/Ray.hpp>

#include <core/Defines.hpp>

namespace hyperion {

/// reference: https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/bvh.html
class BVHNode
{
public:
    BVHNode()
        : m_aabb(BoundingBox::Empty()),
          m_is_leaf_node(false)
    {
    }

    BVHNode(const BoundingBox &aabb)
        : m_aabb(aabb),
          m_is_leaf_node(true)
    {
    }

    BVHNode(const BVHNode &other)                   = default;
    BVHNode &operator=(const BVHNode &other)        = default;

    BVHNode(BVHNode &&other) noexcept               = default;
    BVHNode &operator=(BVHNode &&other) noexcept    = default;

    ~BVHNode()                                      = default;

    HYP_FORCE_INLINE bool IsValid() const
        { return m_aabb.IsValid() && m_aabb.IsFinite(); }

    HYP_FORCE_INLINE const BoundingBox &GetAABB() const
        { return m_aabb; }

    HYP_FORCE_INLINE const LinkedList<BVHNode> &GetChildren() const
        { return m_children; }

    HYP_FORCE_INLINE const Array<Triangle> &GetTriangles() const
        { return m_triangles; }

    HYP_FORCE_INLINE void AddTriangle(const Triangle &triangle)
        { m_triangles.PushBack(triangle); }

    HYP_FORCE_INLINE bool IsLeafNode() const
        { return m_is_leaf_node; }

    void Split(int max_depth = 3)
    {
        Split_Internal(0, max_depth);
    }

    HYP_NODISCARD RayTestResults TestRay(const Ray &ray) const
    {
        RayTestResults results;

        if (ray.TestAABB(m_aabb)) {
            if (IsLeafNode()) {
                for (SizeType triangle_index = 0; triangle_index < m_triangles.Size(); triangle_index++) {
                    const Triangle &triangle = m_triangles[triangle_index];

                    ray.TestTriangle(triangle, triangle_index, this, results);
                }
            } else {
                for (const BVHNode &node : m_children) {
                    results.Merge(node.TestRay(ray));
                }
            }
        }

        return results;
    }

private:
    void Split_Internal(int depth, int max_depth)
    {
        if (m_is_leaf_node) {
            if (m_triangles.Any()) {
                if (depth < max_depth) {
                    const Vec3f center = m_aabb.GetCenter();
                    const Vec3f extent = m_aabb.GetExtent();

                    const Vec3f &min = m_aabb.GetMin();
                    const Vec3f &max = m_aabb.GetMax();

                    for (int i = 0; i < 2; i++) {
                        for (int j = 0; j < 2; j++) {
                            for (int k = 0; k < 2; k++) {
                                const Vec3f new_min = Vec3f(
                                    i == 0 ? min.x : center.x,
                                    j == 0 ? min.y : center.y,
                                    k == 0 ? min.z : center.z
                                );

                                const Vec3f new_max = Vec3f(
                                    i == 0 ? center.x : max.x,
                                    j == 0 ? center.y : max.y,
                                    k == 0 ? center.z : max.z
                                );

                                m_children.EmplaceBack(BoundingBox(new_min, new_max));
                            }
                        }
                    }

                    for (const Triangle &triangle : m_triangles) {
                        for (BVHNode &node : m_children) {
                            if (node.GetAABB().ContainsTriangle(triangle)) {
                                node.m_triangles.PushBack(triangle);
                            }
                        }
                    }

                    m_triangles.Clear();

                    m_is_leaf_node = false;
                }
            }
        }

        for (BVHNode &node : m_children) {
            node.Split_Internal(depth + 1, max_depth);
        }
    }

    // @TODO Refactor to use a span or pointer to MeshData indices and vertices rather than
    // copying Triangle data

    BoundingBox         m_aabb;
    LinkedList<BVHNode> m_children;
    Array<Triangle>     m_triangles;
    bool                m_is_leaf_node;
};

} // namespace hyperion

#endif
