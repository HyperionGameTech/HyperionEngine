/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/Mat4f.hpp>
#include <core/math/Vector4.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/BoundingSphere.hpp>

#include <core/reflection/HypObjectFwd.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/Types.hpp>

namespace hyperion {

HYP_STRUCT(Size = 224, Serialize = "bitwise")
struct HYP_API Frustum
{
    HYP_STRUCT_BODY(Frustum);

    HYP_FIELD()
    FixedArray<Vec4f, 6> planes;

    HYP_FIELD()
    FixedArray<Vec3f, 8> corners;

    Frustum();

    Frustum(const Frustum& other) = default;
    Frustum& operator=(const Frustum& other) = default;

    Frustum(const Mat4f& viewProj);

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
    bool ContainsPoint(const Vec3f& point) const;

    Frustum& SetFromViewProjectionMatrix(const Mat4f& viewProj);
    Vec3f GetIntersectionPoint(uint32 planeIndex0, uint32 planeIndex1, uint32 planeIndex2) const;
};

} // namespace hyperion
