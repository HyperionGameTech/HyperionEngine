/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Math/BoundingSphere.hpp>
#include <Core/Math/MathUtil.hpp>

#ifndef HYP_TOOL
#include <BoundingSphere.generated.inl>
#endif

namespace Hyperion {

const BoundingSphere BoundingSphere::empty = BoundingSphere();
const BoundingSphere BoundingSphere::infinity = BoundingSphere(Vec3f::Zero(), MathUtil::Infinity<float>());

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

} // namespace Hyperion
