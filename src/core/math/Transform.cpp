/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/Transform.hpp>

#ifndef HYP_TOOL
#include <Transform.generated.inl>
#endif

namespace Hyperion {

const Transform Transform::identity {};

Transform::Transform()
    : translation(Vec3f::Zero()),
      scale(Vec3f::One()),
      rotation(Quaternion::Identity())
{
}

Transform::Transform(const Vec3f& translation, const Vec3f& scale)
    : translation(translation),
      scale(scale),
      rotation(Quaternion::Identity())
{
}

Transform::Transform(const Vec3f& translation, const Vec3f& scale, const Quaternion& rotation)
    : translation(translation),
      scale(scale),
      rotation(rotation)
{
}

Transform::Transform(const Vec3f& translation)
    : Transform(translation, Vec3f::One(), Quaternion::Identity())
{
}

Transform Transform::GetInverse() const
{
    return {
        -translation,
        Vec3f(1.0f) / scale,
        rotation.Inverse()
    };
}

Transform Transform::operator*(const Transform& other) const
{
    return {
        translation + (rotation * (scale * other.translation)),
        scale * other.scale,
        rotation * other.rotation
    };
}

Transform& Transform::operator*=(const Transform& other)
{
    return *this = *this * other;
}

} // namespace Hyperion
