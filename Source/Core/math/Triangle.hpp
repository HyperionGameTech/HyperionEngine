/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/Types.hpp>

#include <core/reflection/ObjectMacros.hpp>

namespace Hyperion {

HYP_STRUCT(Serialize = "bitwise")
struct HYP_API Triangle
{
    HYP_STRUCT_BODY(Triangle);

    HYP_FIELD()
    FixedArray<Vec3f, 3> points;

    Triangle();
    Triangle(const float (&pts)[9]);

    Triangle(const Triangle& other) = default;
    Triangle& operator=(const Triangle& other) = default;

    ~Triangle() = default;

    HYP_FORCE_INLINE bool operator==(const Triangle& other) const
    {
        return points == other.points;
    }

    HYP_FORCE_INLINE bool operator!=(const Triangle& other) const
    {
        return points != other.points;
    }

    HYP_FORCE_INLINE Vec3f& operator[](SizeType index)
    {
        return points[index];
    }

    HYP_FORCE_INLINE const Vec3f& operator[](SizeType index) const
    {
        return points[index];
    }

    HYP_FORCE_INLINE Vec3f& GetPoint(SizeType index)
    {
        return points[index];
    }

    HYP_FORCE_INLINE const Vec3f& GetPoint(SizeType index) const
    {
        return points[index];
    }

    HYP_FORCE_INLINE void SetPoint(SizeType index, const Vec3f& value)
    {
        points[index] = value;
    }

    HYP_FORCE_INLINE Vec3f GetPosition() const
    {
        return (points[0] + points[1] + points[2]) / 3.0f;
    }

    HYP_FORCE_INLINE Vec3f GetNormal() const
    {
        return (points[1] - points[0]).Cross(points[2] - points[0]).Normalize();
    }

    Vec3f& Closest(const Vec3f& vec);
    const Vec3f& Closest(const Vec3f& vec) const;
    // bool IntersectRay(const Ray &ray, RayTestResults &out) const;

    BoundingBox GetBoundingBox() const;

    bool ContainsPoint(const Vec3f& pt) const;

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        // clang-format off
        return HashCode()
            .Combine(points[0].GetHashCode())
            .Combine(points[1].GetHashCode())
            .Combine(points[2].GetHashCode());
        // clang-format on
    }
};

Triangle operator*(const Mat4f& transform, const Triangle& triangle);

} // namespace Hyperion
