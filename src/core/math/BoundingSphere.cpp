/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/BoundingSphere.hpp>
#include <core/math/MathUtil.hpp>

namespace hyperion {

const BoundingSphere BoundingSphere::empty = BoundingSphere();
const BoundingSphere BoundingSphere::infinity = BoundingSphere(Vec3f::Zero(), MathUtil::Infinity<float>());

BoundingSphere::BoundingSphere()
    : center(Vec3f::Zero()),
      radius(0.0f)
{
}

BoundingSphere::BoundingSphere(const Vec3f& center, float radius)
    : center(center),
      radius(radius)
{
}

BoundingSphere::BoundingSphere(const BoundingSphere& other)
    : center(other.center),
      radius(other.radius)
{
}

BoundingSphere& BoundingSphere::operator=(const BoundingSphere& other)
{
    center = other.center;
    radius = other.radius;

    return *this;
}

BoundingSphere::BoundingSphere(BoundingSphere&& other) noexcept
    : center(other.center),
      radius(other.radius)
{
    other.center = Vec3f::Zero();
    other.radius = 0.0f;
}

BoundingSphere& BoundingSphere::operator=(BoundingSphere&& other) noexcept
{
    center = other.center;
    radius = other.radius;

    other.center = Vec3f::Zero();
    other.radius = 0.0f;

    return *this;
}

BoundingSphere::BoundingSphere(const BoundingBox& box)
    : BoundingSphere()
{
    if (box.IsValid())
    {
        center = box.GetCenter();
        radius = box.GetRadius();
    }
}

BoundingSphere& BoundingSphere::Extend(const BoundingBox& box)
{
    // https://github.com/openscenegraph/OpenSceneGraph/blob/master/include/osg/BoundingSphere

    BoundingBox newAabb(box);

    Vec3f directionVector;

    for (const Vec3f& corner : box.GetCorners())
    {
        directionVector = (corner - center).Normalized();
        directionVector *= -radius;
        directionVector += center;

        newAabb = newAabb.Union(directionVector);
    }

    center = newAabb.GetCenter();
    radius = newAabb.GetRadius();

    return *this;
}

bool BoundingSphere::Overlaps(const BoundingSphere& other) const
{
    float distanceSquared = (other.center - center).LengthSquared();
    float radiusSum = radius + other.radius;

    return distanceSquared <= (radiusSum * radiusSum);
}

bool BoundingSphere::Overlaps(const BoundingBox& box) const
{
    Vec3f closestPoint = MathUtil::Clamp(center, box.min, box.max);
    float distanceSquared = (center - closestPoint).LengthSquared();
    float radiusSquared = radius * radius;

    return distanceSquared <= radiusSquared;
}

bool BoundingSphere::ContainsPoint(const Vec3f& point) const
{
    return (point - center).LengthSquared() <= radius * radius;
}

Vec4f BoundingSphere::ToVector4() const
{
    return Vec4f(center, radius);
}

} // namespace hyperion
