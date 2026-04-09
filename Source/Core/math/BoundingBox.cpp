/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/math/BoundingBox.hpp>
#include <Core/math/Triangle.hpp>
#include <Core/math/MathUtil.hpp>

#ifndef HYP_TOOL
#include <BoundingBox.generated.inl>
#endif

namespace Hyperion {

HYP_API BoundingBox operator*(const Mat4f& transform, const BoundingBox& aabb)
{
    if (!aabb.IsValid())
    {
        return aabb;
    }

    BoundingBox result;

    for (Vec3f corner : aabb.GetCorners())
    {
        result = result.Union(transform * corner);
    }

    return result;
}

HYP_API BoundingBox operator*(const Transform& transform, const BoundingBox& aabb)
{
    return transform.GetMatrix() * aabb;
}

BoundingBox::BoundingBox()
    : min(MathUtil::MaxSafeValue<Vec3f>()),
      max(MathUtil::MinSafeValue<Vec3f>())
{
}

BoundingBox::BoundingBox(const Vec3f& min, const Vec3f& max)
    : min(min),
      max(max)
{
}

FixedArray<Vec3f, 8> BoundingBox::GetCorners() const
{
    return {
        Vec3f(min.x, min.y, min.z),
        Vec3f(max.x, min.y, min.z),
        Vec3f(max.x, max.y, min.z),
        Vec3f(min.x, max.y, min.z),
        Vec3f(min.x, min.y, max.z),
        Vec3f(min.x, max.y, max.z),
        Vec3f(max.x, max.y, max.z),
        Vec3f(max.x, min.y, max.z)
    };
}

void BoundingBox::SetCenter(const Vec3f& center)
{
    Vec3f dimensions = GetExtent();

    max = center + dimensions * 0.5f;
    min = center - dimensions * 0.5f;
}

void BoundingBox::SetCorners(const FixedArray<Vec3f, 8>& corners)
{
    min = corners[0];
    max = corners[0];

    for (uint32 i = 1; i < 8; ++i)
    {
        min = Vec3f::Min(min, corners[i]);
        max = Vec3f::Max(max, corners[i]);
    }
}

void BoundingBox::SetExtent(const Vec3f& dimensions)
{
    Vec3f center = GetCenter();

    max = center + dimensions * 0.5f;
    min = center - dimensions * 0.5f;
}

// https://github.com/openscenegraph/OpenSceneGraph/blob/master/include/osg/BoundingBox
float BoundingBox::GetRadiusSquared() const
{
    return 0.25f * GetExtent().LengthSquared();
}

float BoundingBox::GetRadius() const
{
    return MathUtil::Sqrt(GetRadiusSquared());
}

BoundingBox BoundingBox::operator*(float scalar) const
{
    BoundingBox other(*this);
    other *= scalar;

    return other;
}

BoundingBox& BoundingBox::operator*=(float scalar)
{
    if (!IsValid())
    {
        return *this;
    }

    min *= scalar;
    max *= scalar;

    return *this;
}

BoundingBox BoundingBox::operator/(float scalar) const
{
    return BoundingBox(*this) /= scalar;
}

BoundingBox& BoundingBox::operator/=(float scalar)
{
    if (!IsValid())
    {
        return *this;
    }

    min /= scalar;
    max /= scalar;

    return *this;
}

BoundingBox BoundingBox::operator+(const Vec3f& vec) const
{
    return BoundingBox(min + vec, max + vec);
}

BoundingBox& BoundingBox::operator+=(const Vec3f& vec)
{
    min += vec;
    max += vec;

    return *this;
}

BoundingBox BoundingBox::operator/(const Vec3f& vec) const
{
    return BoundingBox(min / vec, max / vec);
}

BoundingBox& BoundingBox::operator/=(const Vec3f& vec)
{
    min /= vec;
    max /= vec;

    return *this;
}

BoundingBox BoundingBox::operator*(const Vec3f& scale) const
{
    return BoundingBox(min * scale, max * scale);
}

BoundingBox& BoundingBox::operator*=(const Vec3f& scale)
{
    min *= scale;
    max *= scale;

    return *this;
}

BoundingBox& BoundingBox::Clear()
{
    min = Vec3f(MathUtil::MaxSafeValue<float>());
    max = Vec3f(MathUtil::MinSafeValue<float>());

    return *this;
}

BoundingBox BoundingBox::Expand(const Vec3f& delta) const
{
    return BoundingBox {
        min - delta,
        max + delta
    };
}

BoundingBox BoundingBox::Union(const Vec3f& vec) const
{
    return BoundingBox {
        Vec3f::Min(min, vec),
        Vec3f::Max(max, vec)
    };
}

BoundingBox BoundingBox::Union(const BoundingBox& other) const
{
    return BoundingBox {
        Vec3f::Min(min, other.min),
        Vec3f::Max(max, other.max)
    };
}

BoundingBox BoundingBox::Intersection(const BoundingBox& other) const
{
    if (!Overlaps(other))
    {
        return Empty();
    }

    return BoundingBox {
        Vec3f::Max(min, other.min),
        Vec3f::Min(max, other.max)
    };
}

bool BoundingBox::Overlaps(const BoundingBox& other) const
{
    if (max.x < other.min.x || other.max.x < min.x)
        return false;
    if (max.y < other.min.y || other.max.y < min.y)
        return false;
    if (max.z < other.min.z || other.max.z < min.z)
        return false;

    return true;
}

bool BoundingBox::Contains(const BoundingBox& other) const
{
    const FixedArray<Vec3f, 8> corners = other.GetCorners();

    if (!ContainsPoint(corners[0]))
        return false;
    if (!ContainsPoint(corners[1]))
        return false;
    if (!ContainsPoint(corners[2]))
        return false;
    if (!ContainsPoint(corners[3]))
        return false;
    if (!ContainsPoint(corners[4]))
        return false;
    if (!ContainsPoint(corners[5]))
        return false;
    if (!ContainsPoint(corners[6]))
        return false;
    if (!ContainsPoint(corners[7]))
        return false;

    return true;
}

bool BoundingBox::ContainsTriangle(const Triangle& triangle) const
{
    for (int i = 0; i < 3; ++i)
    {
        Vec3f p = triangle[i];

        if (p.x < min.x || p.x > max.x || p.y < min.y || p.y > max.y || p.z < min.z || p.z > max.z)
        {
            return false;
        }
    }

    return true;
}

bool BoundingBox::OverlapsTriangle(const Triangle& triangle) const
{
    static constexpr Vec3f Axes[3] = { Vec3f { 1, 0, 0 }, Vec3f { 0, 1, 0 }, Vec3f { 0, 0, 1 } };

    // bring triangle into box‐centered coordinates
    Vec3f center = GetCenter();
    Vec3f half = GetExtent() * 0.5f;

    Vec3f v0 = triangle[0] - center;
    Vec3f v1 = triangle[1] - center;
    Vec3f v2 = triangle[2] - center;

    // triangle edges
    Vec3f e0 = v1 - v0;
    Vec3f e1 = v2 - v1;
    Vec3f e2 = v0 - v2;

    for (const Vec3f& e : { e0, e1, e2 })
    {
        for (const Vec3f& a : Axes)
        {
            Vec3f axis = e.Cross(a);

            float r = half.x * fabs(axis.x)
                + half.y * fabs(axis.y)
                + half.z * fabs(axis.z);

            float p0 = axis.Dot(v0);
            float p1 = axis.Dot(v1);
            float p2 = axis.Dot(v2);

            float mn = MathUtil::Min(MathUtil::Min(p0, p1), p2);
            float mx = MathUtil::Max(MathUtil::Max(p0, p1), p2);

            if (mn > r || mx < -r)
            {
                return false;
            }
        }
    }

    for (int i = 0; i < 3; i++)
    {
        float mn = MathUtil::Min(MathUtil::Min(v0[i], v1[i]), v2[i]);
        float mx = MathUtil::Max(MathUtil::Max(v0[i], v1[i]), v2[i]);

        if (mn > half[i] || mx < -half[i])
        {
            return false;
        }
    }

    Vec3f normal = e0.Cross(e1);
    float d = -normal.Dot(v0);

    Vec3f vmin;
    Vec3f vmax;

    for (int i = 0; i < 3; ++i)
    {
        if (normal[i] > 0.0f)
        {
            vmin[i] = -half[i];
            vmax[i] = half[i];
        }
        else
        {
            vmin[i] = half[i];
            vmax[i] = -half[i];
        }
    }

    if (normal.Dot(vmin) + d > 0.0f || normal.Dot(vmax) + d < 0.0f)
    {
        return false;
    }

    return true;
}

bool BoundingBox::ContainsPoint(const Vec3f& vec) const
{
    if (vec.x < min.x)
        return false;
    if (vec.y < min.y)
        return false;
    if (vec.z < min.z)
        return false;
    if (vec.x > max.x)
        return false;
    if (vec.y > max.y)
        return false;
    if (vec.z > max.z)
        return false;

    return true;
}

float BoundingBox::Area() const
{
    Vec3f dimensions(max - min);
    return dimensions.x * dimensions.y * dimensions.z;
}

} // namespace Hyperion
