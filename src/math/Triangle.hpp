/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef TRIANGLE_H
#define TRIANGLE_H

#include <math/Vertex.hpp>
#include <math/Transform.hpp>
#include <math/Matrix4.hpp>
#include <math/BoundingBox.hpp>

#include <core/lib/FixedArray.hpp>
#include <Types.hpp>

namespace hyperion {
class HYP_API Triangle
{
public:
    Triangle();
    Triangle(const Vec3f &v0, const Vec3f &v1, const Vec3f &v2);
    Triangle(const Vertex &v0, const Vertex &v1, const Vertex &v2);
    Triangle(const Triangle &other);
    ~Triangle() = default;

    Vertex &operator[](SizeType index) { return m_points[index]; }
    const Vertex &operator[](SizeType index) const { return m_points[index]; }
    Vertex &GetPoint(SizeType index) { return operator[](index); }
    const Vertex &GetPoint(SizeType index) const { return operator[](index); }
    void SetPoint(SizeType index, const Vertex &value) { m_points[index] = value; }

    Vec3f GetPosition() const
        { return (m_points[0].GetPosition() + m_points[1].GetPosition() + m_points[2].GetPosition()) / 3.0f; }

    Vec3f GetNormal() const
        { return (m_points[1].GetPosition() - m_points[0].GetPosition()).Cross(m_points[2].GetPosition() - m_points[0].GetPosition()).Normalize(); }

    Vertex &Closest(const Vec3f &vec);
    const Vertex &Closest(const Vec3f &vec) const;
    // bool IntersectRay(const Ray &ray, RayTestResults &out) const;

    BoundingBox GetBoundingBox() const;

    bool ContainsPoint(const Vec3f &pt) const;

private:
    FixedArray<Vertex, 3> m_points;
};
} // namespace hyperion

#endif
