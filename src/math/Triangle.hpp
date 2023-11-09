#ifndef TRIANGLE_H
#define TRIANGLE_H

#include <math/Vertex.hpp>
#include <math/Transform.hpp>
#include <math/Matrix4.hpp>
#include <math/BoundingBox.hpp>

#include <core/lib/FixedArray.hpp>
#include <Types.hpp>

namespace hyperion {
class Triangle
{
public:
    Triangle();
    Triangle(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2);
    Triangle(const Vertex &v0, const Vertex &v1, const Vertex &v2);
    Triangle(const Triangle &other);
    ~Triangle() = default;

    Vertex &operator[](SizeType index) { return m_points[index]; }
    const Vertex &operator[](SizeType index) const { return m_points[index]; }
    Vertex &GetPoint(SizeType index) { return operator[](index); }
    const Vertex &GetPoint(SizeType index) const { return operator[](index); }
    void SetPoint(SizeType index, const Vertex &value) { m_points[index] = value; }

    Vector3 GetCenter() const
    {
        return (m_points[0].GetPosition() + m_points[1].GetPosition() + m_points[2].GetPosition()) / 3.0f;
    }

    Vertex &Closest(const Vector3 &vec);
    const Vertex &Closest(const Vector3 &vec) const;
    // bool IntersectRay(const Ray &ray, RayTestResults &out) const;

    BoundingBox GetBoundingBox() const;

    Bool ContainsPoint(const Vector3 &pt) const;

private:
    FixedArray<Vertex, 3> m_points;
};
} // namespace hyperion

#endif
