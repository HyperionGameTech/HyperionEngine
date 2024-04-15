/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef FRUSTUM_H
#define FRUSTUM_H

#include <math/Matrix4.hpp>
#include <math/Vector4.hpp>
#include <math/BoundingBox.hpp>
#include <math/BoundingSphere.hpp>

#include <core/lib/FixedArray.hpp>

#include <Types.hpp>

namespace hyperion {
class HYP_API Frustum
{
public:
    Frustum();
    Frustum(const Frustum &other);
    Frustum(const Matrix4 &view_proj);

    FixedArray<Vec4f, 6> &GetPlanes()
        { return m_planes; }

    const FixedArray<Vec4f, 6> &GetPlanes() const
        { return m_planes; }

    Vec4f &GetPlane(uint index)
        { return m_planes[index]; }

    const Vec4f &GetPlane(uint index) const
        { return m_planes[index]; }

    const Vec3f &GetCorner(uint index) const
        { return m_corners[index]; }

    const FixedArray<Vec3f, 8> &GetCorners() const
        { return m_corners; }

    bool ContainsAABB(const BoundingBox &aabb) const;
    bool ContainsBoundingSphere(const BoundingSphere &sphere) const;

    Frustum &SetFromViewProjectionMatrix(const Matrix4 &view_proj);
    Vec3f GetIntersectionPoint(uint plane_index_0, uint plane_index_1, uint plane_index_2) const;

private:
    FixedArray<Vec4f, 6>    m_planes;
    FixedArray<Vec3f, 8>    m_corners;
};
} // namespace hyperion

#endif

