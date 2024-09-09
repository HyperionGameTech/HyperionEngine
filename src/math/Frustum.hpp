/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FRUSTUM_HPP
#define HYPERION_FRUSTUM_HPP

#include <math/Matrix4.hpp>
#include <math/Vector4.hpp>
#include <math/BoundingBox.hpp>
#include <math/BoundingSphere.hpp>

#include <core/containers/FixedArray.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_STRUCT()
struct HYP_API Frustum
{
    HYP_FIELD()
    FixedArray<Vec4f, 6>    planes;

    HYP_FIELD()
    FixedArray<Vec3f, 8>    corners;

    Frustum();
    Frustum(const Frustum &other);
    Frustum(const Matrix4 &view_proj);

    HYP_FORCE_INLINE FixedArray<Vec4f, 6> &GetPlanes()
        { return planes; }

    HYP_FORCE_INLINE const FixedArray<Vec4f, 6> &GetPlanes() const
        { return planes; }

    HYP_FORCE_INLINE Vec4f &GetPlane(uint index)
        { return planes[index]; }

    HYP_FORCE_INLINE const Vec4f &GetPlane(uint index) const
        { return planes[index]; }

    HYP_FORCE_INLINE const Vec3f &GetCorner(uint index) const
        { return corners[index]; }

    HYP_FORCE_INLINE const FixedArray<Vec3f, 8> &GetCorners() const
        { return corners; }

    bool ContainsAABB(const BoundingBox &aabb) const;
    bool ContainsBoundingSphere(const BoundingSphere &sphere) const;

    Frustum &SetFromViewProjectionMatrix(const Matrix4 &view_proj);
    Vec3f GetIntersectionPoint(uint plane_index_0, uint plane_index_1, uint plane_index_2) const;
};

} // namespace hyperion

#endif

