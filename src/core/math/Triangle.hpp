/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TRIANGLE_HPP
#define HYPERION_TRIANGLE_HPP

#include <core/math/Vertex.hpp>
#include <core/math/BoundingBox.hpp>

#include <core/containers/FixedArray.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_STRUCT(Serialize = "bitwise")

struct HYP_API Triangle
{
    HYP_FIELD()
    FixedArray<Vertex, 3> points;

    Triangle();
    Triangle(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2);
    Triangle(const Vertex& v0, const Vertex& v1, const Vertex& v2);
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

    HYP_FORCE_INLINE Vertex& operator[](SizeType index)
    {
        return points[index];
    }

    HYP_FORCE_INLINE const Vertex& operator[](SizeType index) const
    {
        return points[index];
    }

    HYP_FORCE_INLINE Vertex& GetPoint(SizeType index)
    {
        return points[index];
    }

    HYP_FORCE_INLINE const Vertex& GetPoint(SizeType index) const
    {
        return points[index];
    }

    HYP_FORCE_INLINE void SetPoint(SizeType index, const Vertex& value)
    {
        points[index] = value;
    }

    HYP_FORCE_INLINE Vec3f GetPosition() const
    {
        return (points[0].GetPosition() + points[1].GetPosition() + points[2].GetPosition()) / 3.0f;
    }

    HYP_FORCE_INLINE Vec3f GetNormal() const
    {
        return (points[1].GetPosition() - points[0].GetPosition()).Cross(points[2].GetPosition() - points[0].GetPosition()).Normalize();
    }

    Vertex& Closest(const Vec3f& vec);
    const Vertex& Closest(const Vec3f& vec) const;
    // bool IntersectRay(const Ray &ray, RayTestResults &out) const;

    BoundingBox GetBoundingBox() const;

    bool ContainsPoint(const Vec3f& pt) const;
};
} // namespace hyperion

#endif
