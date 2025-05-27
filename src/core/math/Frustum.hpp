/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FRUSTUM_HPP
#define HYPERION_FRUSTUM_HPP

#include <core/math/Matrix4.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/BoundingSphere.hpp>

#include <core/containers/FixedArray.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_STRUCT(Size = 224, Serialize = "bitwise")

struct HYP_API Frustum
{
    HYP_FIELD()
    FixedArray<Vec4f, 6> planes;

    HYP_FIELD()
    FixedArray<Vec3f, 8> corners;

    Frustum();

    Frustum(const Frustum& other) = default;
    Frustum& operator=(const Frustum& other) = default;

    Frustum(const Matrix4& view_proj);

    HYP_FORCE_INLINE FixedArray<Vec4f, 6>& GetPlanes()
    {
        return planes;
    }

    HYP_FORCE_INLINE const FixedArray<Vec4f, 6>& GetPlanes() const
    {
        return planes;
    }

    HYP_FORCE_INLINE Vec4f& GetPlane(uint32 index)
    {
        return planes[index];
    }

    HYP_FORCE_INLINE const Vec4f& GetPlane(uint32 index) const
    {
        return planes[index];
    }

    HYP_FORCE_INLINE const Vec3f& GetCorner(uint32 index) const
    {
        return corners[index];
    }

    HYP_FORCE_INLINE const FixedArray<Vec3f, 8>& GetCorners() const
    {
        return corners;
    }

    bool ContainsAABB(const BoundingBox& aabb) const;
    bool ContainsBoundingSphere(const BoundingSphere& sphere) const;

    Frustum& SetFromViewProjectionMatrix(const Matrix4& view_proj);
    Vec3f GetIntersectionPoint(uint32 plane_index_0, uint32 plane_index_1, uint32 plane_index_2) const;
};

} // namespace hyperion

#endif
