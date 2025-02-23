/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BOUNDING_SPHERE_HPP
#define HYPERION_BOUNDING_SPHERE_HPP

#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/Ray.hpp>

#include <HashCode.hpp>

namespace hyperion {

HYP_STRUCT(Size=32)
struct HYP_API BoundingSphere
{
public:
    static const BoundingSphere empty;
    static const BoundingSphere infinity;

    BoundingSphere();
    BoundingSphere(const Vec3f &center, float radius);
    BoundingSphere(const BoundingSphere &other);
    BoundingSphere &operator=(const BoundingSphere &other);
    BoundingSphere(BoundingSphere &&other) noexcept;
    BoundingSphere &operator=(BoundingSphere &&other) noexcept;
    BoundingSphere(const BoundingBox &box);

    HYP_FORCE_INLINE const Vec3f &GetCenter() const
        { return center; }

    HYP_FORCE_INLINE void SetCenter(const Vec3f &center)
        { this->center = center; }

    HYP_FORCE_INLINE float GetRadius() const
        { return radius; }

    HYP_FORCE_INLINE void SetRadius(float radius)
        { this->radius = radius; }

    HYP_FORCE_INLINE bool operator==(const BoundingSphere &other) const
        { return center == other.center && radius == other.radius; }

    HYP_FORCE_INLINE bool operator!=(const BoundingSphere &other) const
        { return !operator==(other); }

    BoundingSphere &Extend(const BoundingBox &box);

    /*! \brief Convert the BoundingSphere to an AABB. */
    HYP_FORCE_INLINE operator BoundingBox() const
        { return BoundingBox(center - Vec3f(radius), center + Vec3f(radius)); }

    /*! \brief Store the BoundingSphere in a Vector4.
        x,y,z components will be the center of the sphere,
        w will be the radius. */
    Vec4f ToVector4() const;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(center.GetHashCode());
        hc.Add(radius);

        return hc;
    }

    HYP_FIELD(Property="Center", Serialize=true)
    Vec3f   center;

    HYP_FIELD(Property="Radius", Serialize=true)
    float   radius;
};

} // namespace hyperion

#endif
