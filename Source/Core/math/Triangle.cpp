/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/math/Triangle.hpp>
#include <Core/math/MathUtil.hpp>

#ifndef HYP_TOOL
#include <Triangle.generated.inl>
#endif

namespace Hyperion {

Triangle::Triangle()
{
    Memory::Fill(points.Data(), 0, sizeof(points));
}

Triangle::Triangle(const float (&pts)[9])
{
    Memory::Fill(points.Data(), 0, sizeof(points));
    Memory::Copy(points[0].values, pts + 0, sizeof(float) * 3);
    Memory::Copy(points[1].values, pts + 3, sizeof(float) * 3);
    Memory::Copy(points[2].values, pts + 6, sizeof(float) * 3);
}

Vec3f& Triangle::Closest(const Vec3f& vec)
{
    float distances[3] {};
    uint32 shortestIndex = 0;

    for (uint32 i = 0; i < 3; i++)
    {
        distances[i] = points[i].DistanceSquared(vec);

        if (i != 0)
        {
            if (distances[i] < distances[shortestIndex])
            {
                shortestIndex = i;
            }
        }
    }

    return points[shortestIndex];
}

const Vec3f& Triangle::Closest(const Vec3f& vec) const
{
    return const_cast<Triangle*>(this)->Closest(vec);
}

BoundingBox Triangle::GetBoundingBox() const
{
    return BoundingBox()
        .Union(points[0])
        .Union(points[1])
        .Union(points[2]);
}

bool Triangle::ContainsPoint(const Vec3f& pt) const
{
    const Vec3f v0 = points[2] - points[0];
    const Vec3f v1 = points[1] - points[0];
    const Vec3f v2 = pt - points[0];

    const float dot00 = v0.Dot(v0);
    const float dot01 = v0.Dot(v1);
    const float dot02 = v0.Dot(v2);
    const float dot11 = v1.Dot(v1);
    const float dot12 = v1.Dot(v2);

    const float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
    const float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    const float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

    return (u >= 0.0f) && (v >= 0.0f) && (u + v < 1.0f);
}

Triangle operator*(const Mat4f& transform, const Triangle& triangle)
{
    Triangle result;

    for (uint8 i = 0; i < 3; i++)
    {
        result.points[i] = transform.TransformVector(triangle.points[i]);
    }

    return result;
}

} // namespace Hyperion
