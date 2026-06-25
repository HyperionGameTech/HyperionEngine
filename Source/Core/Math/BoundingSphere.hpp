/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/Vector3.hpp>
#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/Ray.hpp>

#include <Core/Reflection/ObjectFwd.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

HYP_STRUCT(Size = 32)
struct CORE_API BoundingSphere
{
    HYP_STRUCT_BODY(BoundingSphere);

    static const BoundingSphere empty;
    static const BoundingSphere infinity;
    
    constexpr BoundingSphere()
        : center(0.0f, 0.0f, 0.0f),
          radius(0.0f)
    {
    }

    constexpr BoundingSphere(const Vec3f& center, float radius)
        : center(center),
          radius(radius)
    {
    }

    constexpr BoundingSphere(const BoundingSphere& other)
        : center(other.center),
          radius(other.radius)
    {
    }

    BoundingSphere& operator=(const BoundingSphere& other)
    {
        center = other.center;
        radius = other.radius;

        return *this;
    }

    constexpr BoundingSphere(BoundingSphere&& other) noexcept
        : center(other.center),
          radius(other.radius)
    {
        other.center = Vec3f::Zero();
        other.radius = 0.0f;
    }

    BoundingSphere& operator=(BoundingSphere&& other) noexcept
    {
        center = other.center;
        radius = other.radius;

        other.center = Vec3f::Zero();
        other.radius = 0.0f;

        return *this;
    }

    constexpr BoundingSphere(const BoundingBox& box)
        : BoundingSphere()
    {
        if (box.IsValid())
        {
            center = box.GetCenter();
            radius = box.GetRadius();
        }
    }

    HYP_FORCE_INLINE constexpr const Vec3f& GetCenter() const
    {
        return center;
    }

    HYP_FORCE_INLINE void SetCenter(const Vec3f& center)
    {
        this->center = center;
    }

    HYP_FORCE_INLINE constexpr float GetRadius() const
    {
        return radius;
    }

    HYP_FORCE_INLINE void SetRadius(float radius)
    {
        this->radius = radius;
    }

    HYP_FORCE_INLINE bool operator==(const BoundingSphere& other) const
    {
        return center == other.center && radius == other.radius;
    }

    HYP_FORCE_INLINE bool operator!=(const BoundingSphere& other) const
    {
        return !operator==(other);
    }

    HYP_FORCE_INLINE constexpr bool IsValid() const
    {
        return !MathUtil::IsNaN(center) && !MathUtil::IsNaN(radius);
    }

    HYP_FORCE_INLINE constexpr bool IsFinite() const
    {
        return MathUtil::IsFinite(center) && MathUtil::IsFinite(radius);
    }

    BoundingSphere& Extend(const BoundingBox& box);

    bool Overlaps(const BoundingSphere& other) const;
    bool Overlaps(const BoundingBox& box) const;

    bool ContainsPoint(const Vec3f& point) const;

    /*! \brief Convert the BoundingSphere to an AABB. */
    HYP_FORCE_INLINE operator BoundingBox() const
    {
        return BoundingBox(center - Vec3f(radius), center + Vec3f(radius));
    }

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

    HYP_FIELD(Property = "Center", Serialize = true)
    Vec3f center;

    HYP_FIELD(Property = "Radius", Serialize = true)
    float radius;
};

} // namespace Hyperion
