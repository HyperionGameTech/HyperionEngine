#include "BoundingSphere.hpp"

#include <limits>
#include <cmath>

namespace hyperion {

BoundingSphere::BoundingSphere()
    : center(Vec3f::Zero()),
      radius(0.0f)
{
}

BoundingSphere::BoundingSphere(const Vec3f &center, float radius)
    : center(center),
      radius(radius)
{
}

BoundingSphere::BoundingSphere(const BoundingSphere &other)
    : center(other.center),
      radius(other.radius)
{
}

BoundingSphere &BoundingSphere::operator=(const BoundingSphere &other)
{
    center = other.center;
    radius = other.radius;

    return *this;
}

BoundingSphere::BoundingSphere(BoundingSphere &&other) noexcept
    : center(other.center),
      radius(other.radius)
{
    other.center = Vec3f::Zero();
    other.radius = 0.0f;
}

BoundingSphere &BoundingSphere::operator=(BoundingSphere &&other) noexcept
{
    center = other.center;
    radius = other.radius;

    other.center = Vec3f::Zero();
    other.radius = 0.0f;

    return *this;
}

BoundingSphere::BoundingSphere(const BoundingBox &box)
    : BoundingSphere()
{
    if (!box.Empty()) {
        center = box.GetCenter();
        radius = box.GetRadius();
    }
}

BoundingSphere &BoundingSphere::Extend(const BoundingBox &box)
{
    // https://github.com/openscenegraph/OpenSceneGraph/blob/master/include/osg/BoundingSphere

    BoundingBox new_aabb(box);

    Vec3f direction_vector;

    for (const Vec3f &corner : box.GetCorners()) {
        direction_vector = (corner - center).Normalized();
        direction_vector *= -radius;
        direction_vector += center;

        new_aabb.Extend(direction_vector);
    }

    center = new_aabb.GetCenter();
    radius = new_aabb.GetRadius();

    return *this;
}

Vec4f BoundingSphere::ToVector4() const
{
    return Vec4f(center, radius);
}

} // namespace hyperion
