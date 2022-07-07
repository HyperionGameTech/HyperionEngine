#include "Triangle.hpp"
#include "MathUtil.hpp"

namespace hyperion {
Triangle::Triangle()
{
}

Triangle::Triangle(const Vertex &v0, const Vertex &v1, const Vertex &v2)
    : m_points({ v0, v1, v2 })
{
}

Triangle::Triangle(const Triangle &other)
    : m_points(other.m_points)
{
}

Triangle Triangle::operator*(const Matrix4 &mat) const
{
    return Triangle(
        GetPoint(0) * mat,
        GetPoint(1) * mat,
        GetPoint(2) * mat
    );
}

Triangle &Triangle::operator*=(const Matrix4 &mat)
{
    for (auto &pt : m_points) {
        pt *= mat;
    }

    return *this;
}

Triangle Triangle::operator*(const Transform &transform) const
{
    return operator*(transform.GetMatrix());
}

Triangle &Triangle::operator*=(const Transform &transform)
{
    return operator*=(transform.GetMatrix());
}

Vertex &Triangle::Closest(const Vector3 &vec)
{
    float distances[3];
    int shortest_index = 0;

    for (int i = 0; i < 3; i++) {
        distances[i] = m_points[i].GetPosition().DistanceSquared(vec);

        if (i != 0) {
            if (distances[i] < distances[shortest_index]) {
                shortest_index = i;
            }
        }
    }

    return m_points[shortest_index];
}

const Vertex &Triangle::Closest(const Vector3 &vec) const
{
    return const_cast<Triangle *>(this)->Closest(vec);
}
} // namespace
