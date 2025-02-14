/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <math/BoundingBox.hpp>
#include <math/Triangle.hpp>
#include <math/MathUtil.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

BoundingBox::BoundingBox()
    : min(MathUtil::MaxSafeValue<Vec3f>()),
      max(MathUtil::MinSafeValue<Vec3f>())
{
}

BoundingBox::BoundingBox(const Vec3f &min, const Vec3f &max)
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

Vec3f BoundingBox::GetCorner(uint32 index) const
{
    const uint32 mask = 1u << index;

    return {
        MathUtil::Lerp(min.x, max.x, int((mask & 1) != 0)),
        MathUtil::Lerp(min.y, max.y, int((mask & 2) != 0)),
        MathUtil::Lerp(min.z, max.z, int((mask & 4) != 0))
    };
}

void BoundingBox::SetCenter(const Vec3f &center)
{
    Vec3f dimensions = GetExtent();

    max = center + dimensions * 0.5f;
    min = center - dimensions * 0.5f;
}

void BoundingBox::SetCorners(const FixedArray<Vec3f, 8> &corners)
{
    min = corners[0];
    max = corners[0];

    for (uint32 i = 1; i < 8; ++i) {
        min = Vec3f::Min(min, corners[i]);
        max = Vec3f::Max(max, corners[i]);
    }
}

void BoundingBox::SetExtent(const Vec3f &dimensions)
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

BoundingBox &BoundingBox::operator*=(float scalar)
{
    if (!IsValid()) {
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

BoundingBox &BoundingBox::operator/=(float scalar)
{
    if (!IsValid()) {
        return *this;
    }

    min /= scalar;
    max /= scalar;

    return *this;
}

BoundingBox BoundingBox::operator+(const Vec3f &vec) const
{
    return BoundingBox(min + vec, max + vec);
}

BoundingBox &BoundingBox::operator+=(const Vec3f &vec)
{
    min += vec;
    max += vec;

    return *this;
}

BoundingBox BoundingBox::operator/(const Vec3f &vec) const
{
    return BoundingBox(min / vec, max / vec);
}

BoundingBox &BoundingBox::operator/=(const Vec3f &vec)
{
    min /= vec;
    max /= vec;

    return *this;
}

BoundingBox BoundingBox::operator*(const Vec3f &scale) const
{
    return BoundingBox(min * scale, max * scale);
}

BoundingBox &BoundingBox::operator*=(const Vec3f &scale)
{
    min *= scale;
    max *= scale;

    return *this;
}

BoundingBox &BoundingBox::operator*=(const Matrix4 &transform)
{
    if (!IsValid()) {
        return *this;
    }

    FixedArray<Vec3f, 8> corners = GetCorners();

    Clear();

    for (Vec3f &corner : corners) {
        corner = transform * corner;

        *this = Union(corner);
    }

    return *this;
}

BoundingBox BoundingBox::operator*(const Matrix4 &transform) const
{
    return BoundingBox(*this) *= transform;
}

BoundingBox &BoundingBox::operator*=(const Transform &transform)
{
    return *this *= transform.GetMatrix();
}

BoundingBox BoundingBox::operator*(const Transform &transform) const
{
    return BoundingBox(*this) *= transform;
}

BoundingBox &BoundingBox::Clear()
{
    min = Vec3f(MathUtil::MaxSafeValue<float>());
    max = Vec3f(MathUtil::MinSafeValue<float>());

    return *this;
}

BoundingBox BoundingBox::Union(const Vec3f &vec) const
{
    return BoundingBox {
        Vec3f::Min(min, vec),
        Vec3f::Max(max, vec)
    };
}

BoundingBox BoundingBox::Union(const BoundingBox &other) const
{
    return BoundingBox {
        Vec3f::Min(min, other.min),
        Vec3f::Max(max, other.max)
    };
}

BoundingBox BoundingBox::Intersection(const BoundingBox &other) const
{
    if (!Overlaps(other)) {
        return Empty();
    }

    return BoundingBox {
        Vec3f::Max(min, other.min),
        Vec3f::Min(max, other.max)
    };
}

bool BoundingBox::Overlaps(const BoundingBox &other) const
{
    if (max.x < other.min.x || min.x > other.max.x) return false;
    if (max.y < other.min.y || min.y > other.max.y) return false;
    if (max.z < other.min.z || min.z > other.max.z) return false;

    return true;
}

bool BoundingBox::Contains(const BoundingBox &other) const
{
    const FixedArray<Vec3f, 8> corners = other.GetCorners();

    if (!ContainsPoint(corners[0])) return false;
    if (!ContainsPoint(corners[1])) return false;
    if (!ContainsPoint(corners[2])) return false;
    if (!ContainsPoint(corners[3])) return false;
    if (!ContainsPoint(corners[4])) return false;
    if (!ContainsPoint(corners[5])) return false;
    if (!ContainsPoint(corners[6])) return false;
    if (!ContainsPoint(corners[7])) return false;

    return true;
}

bool BoundingBox::ContainsTriangle(const Triangle &triangle) const
{
    // Get the axes to test for separation
    static const FixedArray<Vec3f, 3> axes = {
        Vec3f(1.0f, 0.0f, 0.0f),
        Vec3f(0.0f, 1.0f, 0.0f),
        Vec3f(0.0f, 0.0f, 1.0f)
    };

    // Get the corners of the bounding box
    const FixedArray<Vec3f, 8> aabb_corners = GetCorners();

    // Get the corners of the triangle
    const FixedArray<Vec3f, 3> triangle_corners = {
        triangle.GetPoint(0).GetPosition(),
        triangle.GetPoint(1).GetPosition(),
        triangle.GetPoint(2).GetPosition()
    };

    for (const Vec3f &axis : axes) {
        float aabb_min = MathUtil::MaxSafeValue<float>();
        float aabb_max = MathUtil::MinSafeValue<float>();
        
        for (const Vec3f &corner : aabb_corners) {
            const float projection = corner.Dot(axis);

            aabb_min = MathUtil::Min(aabb_min, projection);
            aabb_max = MathUtil::Max(aabb_max, projection);
        }

        float triangle_min = MathUtil::MaxSafeValue<float>();
        float triangle_max = MathUtil::MinSafeValue<float>();

        for (const Vec3f &corner : triangle_corners) {
            const float projection = corner.Dot(axis);

            triangle_min = MathUtil::Min(triangle_min, projection);
            triangle_max = MathUtil::Max(triangle_max, projection);
        }

        if (aabb_max < triangle_min || aabb_min > triangle_max) {
            return false;
        }
    }

    return true;
}

bool BoundingBox::ContainsPoint(const Vec3f &vec) const
{
    if (vec.x < min.x) return false;
    if (vec.y < min.y) return false;
    if (vec.z < min.z) return false;
    if (vec.x > max.x) return false;
    if (vec.y > max.y) return false;
    if (vec.z > max.z) return false;

    return true;
}

float BoundingBox::Area() const
{
    Vec3f dimensions(max - min);
    return dimensions.x * dimensions.y * dimensions.z;
}

std::ostream &operator<<(std::ostream &out, const BoundingBox &bb) // output
{
    out << "BoundingBox [max: " << bb.GetMax() << ", min: " << bb.GetMin() << "]";
    return out;
}

} // namespace hyperion
