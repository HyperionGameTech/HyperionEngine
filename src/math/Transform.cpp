/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <math/Transform.hpp>

namespace hyperion {

const Transform Transform::identity{};

Transform::Transform()
    : m_translation(Vector3::Zero()),
      m_scale(Vector3::One()),
      m_rotation(Quaternion::Identity())
{
    UpdateMatrix();
}

Transform::Transform(const Vector3 &translation, const Vector3 &scale)
    : m_translation(translation),
      m_scale(scale),
      m_rotation(Quaternion::Identity())
{
    UpdateMatrix();
}

Transform::Transform(const Vector3 &translation, const Vector3 &scale, const Quaternion &rotation)
    : m_translation(translation),
      m_scale(scale),
      m_rotation(rotation)
{
    UpdateMatrix();
}

Transform::Transform(const Vector3 &translation)
    : Transform(translation, Vector3::One(), Quaternion::Identity())
{
}

Transform::Transform(const Transform &other)
    : m_translation(other.m_translation),
      m_scale(other.m_scale),
      m_rotation(other.m_rotation),
      m_matrix(other.m_matrix)
{
}

void Transform::UpdateMatrix()
{
    const Matrix4 t = Matrix4::Translation(m_translation);
    const Matrix4 r = Matrix4::Rotation(m_rotation);
    const Matrix4 s = Matrix4::Scaling(m_scale);

    m_matrix = t * r * s;
}

Transform Transform::GetInverse() const
{
    return {
        -m_translation,
        Vec3f(1.0f) / m_scale,
        m_rotation.Inverse()
    };
}

Transform Transform::operator*(const Transform &other) const
{
    return {
        other.m_translation + m_translation,
        other.m_scale * m_scale,
        other.m_rotation * m_rotation
    };
}

Transform &Transform::operator*=(const Transform &other)
{
    m_translation += other.m_translation;
    m_scale *= other.m_scale;
    m_rotation = other.m_rotation * m_rotation;

    UpdateMatrix();

    return *this;
}

} // namespace hyperion
