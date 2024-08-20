/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <math/Transform.hpp>

namespace hyperion {

const Transform Transform::identity { };

Transform::Transform()
    : translation(Vec3f::Zero()),
      scale(Vec3f::One()),
      rotation(Quaternion::Identity())
{
    UpdateMatrix();
}

Transform::Transform(const Vec3f &translation, const Vec3f &scale)
    : translation(translation),
      scale(scale),
      rotation(Quaternion::Identity())
{
    UpdateMatrix();
}

Transform::Transform(const Vec3f &translation, const Vec3f &scale, const Quaternion &rotation)
    : translation(translation),
      scale(scale),
      rotation(rotation)
{
    UpdateMatrix();
}

Transform::Transform(const Vec3f &translation)
    : Transform(translation, Vec3f::One(), Quaternion::Identity())
{
}

Transform::Transform(const Transform &other)
    : translation(other.translation),
      scale(other.scale),
      rotation(other.rotation),
      matrix(other.matrix)
{
}

void Transform::UpdateMatrix()
{
    const Matrix4 t = Matrix4::Translation(translation);
    const Matrix4 r = Matrix4::Rotation(rotation);
    const Matrix4 s = Matrix4::Scaling(scale);

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

Transform Transform::operator*(const Transform &other) const
{
    return {
        other.translation + translation,
        other.scale * scale,
        other.rotation * rotation
    };
}

Transform &Transform::operator*=(const Transform &other)
{
    translation += other.translation;
    scale *= other.scale;
    rotation = other.rotation * rotation;

    UpdateMatrix();

    return *this;
}

} // namespace hyperion
