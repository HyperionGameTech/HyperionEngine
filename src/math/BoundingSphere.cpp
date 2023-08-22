#include "BoundingSphere.hpp"

#include <limits>
#include <cmath>

namespace hyperion {

BoundingSphere::BoundingSphere()
    : center(Vector3::Zero()),
      radius(0.0f)
{
}

BoundingSphere::BoundingSphere(const Vector3 &center, Float radius)
    : center(center),
      radius(radius)
{
}

BoundingSphere::BoundingSphere(const BoundingSphere &other)
    : center(other.center), 
      radius(other.radius)
{
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

    Vector3 direction_vector;

    for (const Vector3 &corner : box.GetCorners()) {
        direction_vector = (corner - center).Normalized();
        direction_vector *= -radius;
        direction_vector += center;

        new_aabb.Extend(direction_vector);
    }

    center = new_aabb.GetCenter();
    radius = new_aabb.GetRadius();

    return *this;
}

Vector4 BoundingSphere::ToVector4() const
{
    return Vector4(center, radius);
}

} // namespace hyperion
