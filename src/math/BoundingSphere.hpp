#ifndef HYPERION_BOUNDING_SPHERE_H
#define HYPERION_BOUNDING_SPHERE_H

#include "Vector3.hpp"
#include "Matrix4.hpp"
#include "BoundingBox.hpp"
#include "Transform.hpp"
#include "Ray.hpp"
#include "../HashCode.hpp"

#include <Types.hpp>

#include <array>
#include <limits>

namespace hyperion {

struct BoundingSphere
{
public:
    BoundingSphere();
    BoundingSphere(const Vector3 &center, Float radius);
    BoundingSphere(const BoundingSphere &other);
    BoundingSphere(const BoundingBox &box);

    const Vector3 &GetCenter() const
        { return center; }

    void SetCenter(const Vector3 &center)
        { this->center = center; }

    Float GetRadius() const
        { return radius; }

    void SetRadius(Float radius)
        { this->radius = radius; }

    bool operator==(const BoundingSphere &other) const
        { return center == other.center && radius == other.radius; }

    bool operator!=(const BoundingSphere &other) const
        { return !operator==(other); }

    BoundingSphere &Extend(const BoundingBox &box);

    /*! \brief Store the BoundingSphere in a Vector4.
        x,y,z components will be the center of the sphere,
        w will be the radius. */
    Vector4 ToVector4() const;

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(center.GetHashCode());
        hc.Add(radius);

        return hc;
    }

    Vector3 center;
    Float radius;
};

} // namespace hyperion

#endif
