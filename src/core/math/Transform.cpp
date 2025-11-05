/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/Transform.hpp>

#ifndef HYP_BUILDTOOL
#include <Transform.generated.inl>
#endif

namespace hyperion {

const Transform Transform::identity {};

Transform::Transform()
    : translation(Vec3f::Zero()),
      scale(Vec3f::One()),
      rotation(Quaternion::Identity()),
      matrix(Mat4f::Identity())
{
}

Transform::Transform(const Vec3f& translation, const Vec3f& scale)
    : translation(translation),
      scale(scale),
      rotation(Quaternion::Identity())
{
    UpdateMatrix();
}

Transform::Transform(const Vec3f& translation, const Vec3f& scale, const Quaternion& rotation)
    : translation(translation),
      scale(scale),
      rotation(rotation)
{
    UpdateMatrix();
}

Transform::Transform(const Vec3f& translation)
    : Transform(translation, Vec3f::One(), Quaternion::Identity())
{
}

void Transform::UpdateMatrix()
{
    const Mat4f t = Mat4f::Translation(translation);
    const Mat4f r = Mat4f::Rotation(rotation);
    const Mat4f s = Mat4f::Scaling(scale);

    matrix = t * r * s;
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
        translation + ((scale * other.translation).Rotate(rotation)),
        scale * other.scale,
        rotation * other.rotation
    };
}

Transform& Transform::operator*=(const Transform& other)
{
    return *this = *this * other;
}

} // namespace hyperion
