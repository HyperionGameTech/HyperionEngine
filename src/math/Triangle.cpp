/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <math/Triangle.hpp>
#include <math/MathUtil.hpp>

namespace hyperion {
Triangle::Triangle()
    : points{}
{
}

Triangle::Triangle(const Vertex &v0, const Vertex &v1, const Vertex &v2)
    : points({ v0, v1, v2 })
{
}

Triangle::Triangle(const Vec3f &v0, const Vec3f &v1, const Vec3f &v2)
    : points({ Vertex(v0), Vertex(v1), Vertex(v2) })
{
}

Vertex &Triangle::Closest(const Vec3f &vec)
{
    float distances[3];
    uint shortest_index = 0;

    for (uint i = 0; i < 3; i++) {
        distances[i] = points[i].GetPosition().DistanceSquared(vec);

        if (i != 0) {
            if (distances[i] < distances[shortest_index]) {
                shortest_index = i;
            }
        }
    }

    return points[shortest_index];
}

const Vertex &Triangle::Closest(const Vec3f &vec) const
{
    return const_cast<Triangle *>(this)->Closest(vec);
}

BoundingBox Triangle::GetBoundingBox() const
{
    const Vec3f min = Vec3f::Min(
        Vec3f::Min(points[0].GetPosition(), points[1].GetPosition()),
        points[2].GetPosition()
    );

    const Vec3f max = Vec3f::Max(
        Vec3f::Max(points[0].GetPosition(), points[1].GetPosition()),
        points[2].GetPosition()
    );

    return BoundingBox(min, max);
}

bool Triangle::ContainsPoint(const Vec3f &pt) const
{
    const Vec3f v0 = points[2].GetPosition() - points[0].GetPosition();
    const Vec3f v1 = points[1].GetPosition() - points[0].GetPosition();
    const Vec3f v2 = pt - points[0].GetPosition();

    const float dot00 = v0.Dot(v0);
    const float dot01 = v0.Dot(v1);
    const float dot02 = v0.Dot(v2);
    const float dot11 = v1.Dot(v1);
    const float dot12 = v1.Dot(v2);

    const float inv_denom = 1.0f / (dot00 * dot11 - dot01 * dot01);
    const float u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
    const float v = (dot00 * dot12 - dot01 * dot02) * inv_denom;

    return (u >= 0.0f) && (v >= 0.0f) && (u + v < 1.0f);
}

} // namespace
