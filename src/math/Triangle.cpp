#include "Triangle.hpp"
#include "MathUtil.hpp"

namespace hyperion {
Triangle::Triangle()
    : m_points{}
{
}

Triangle::Triangle(const Vertex &v0, const Vertex &v1, const Vertex &v2)
    : m_points({ v0, v1, v2 })
{
}

Triangle::Triangle(const Vector3 &v0, const Vector3 &v1, const Vector3 &v2)
    : m_points({ Vertex(v0), Vertex(v1), Vertex(v2) })
{
}

Triangle::Triangle(const Triangle &other)
    : m_points(other.m_points)
{
}

Vertex &Triangle::Closest(const Vector3 &vec)
{
    Float distances[3];
    UInt shortest_index = 0;

    for (UInt i = 0; i < 3; i++) {
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

BoundingBox Triangle::GetBoundingBox() const
{
    const Vector3 min = Vector3::Min(
        Vector3::Min(m_points[0].GetPosition(), m_points[1].GetPosition()),
        m_points[2].GetPosition()
    );

    const Vector3 max = Vector3::Max(
        Vector3::Max(m_points[0].GetPosition(), m_points[1].GetPosition()),
        m_points[2].GetPosition()
    );

    return BoundingBox(min, max);
}

Bool Triangle::ContainsPoint(const Vector3 &pt) const
{
    const Vector3 v0 = m_points[2].GetPosition() - m_points[0].GetPosition();
    const Vector3 v1 = m_points[1].GetPosition() - m_points[0].GetPosition();
    const Vector3 v2 = pt - m_points[0].GetPosition();

    const Float dot00 = v0.Dot(v0);
    const Float dot01 = v0.Dot(v1);
    const Float dot02 = v0.Dot(v2);
    const Float dot11 = v1.Dot(v1);
    const Float dot12 = v1.Dot(v2);

    const Float inv_denom = 1.0f / (dot00 * dot11 - dot01 * dot01);
    const Float u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
    const Float v = (dot00 * dot12 - dot01 * dot02) * inv_denom;

    return (u >= 0.0f) && (v >= 0.0f) && (u + v < 1.0f);
}

} // namespace
