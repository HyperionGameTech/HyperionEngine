/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <math/Transform.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_DEFINE_CLASS(
    Transform,
    HypProperty(NAME("Translation"), static_cast<const Vec3f &(Transform::*)() const>(&Transform::GetTranslation), &Transform::SetTranslation),
    HypProperty(NAME("Rotation"), static_cast<const Quaternion &(Transform::*)() const>(&Transform::GetRotation), &Transform::SetRotation),
    HypProperty(NAME("Scale"), static_cast<const Vec3f &(Transform::*)() const>(&Transform::GetScale), &Transform::SetScale)
);

const Transform Transform::identity{};

Transform::Transform()
    : m_translation(Vec3f::Zero()),
      m_scale(Vec3f::One()),
      m_rotation(Quaternion::Identity())
{
    UpdateMatrix();
}

Transform::Transform(const Vec3f &translation, const Vec3f &scale)
    : m_translation(translation),
      m_scale(scale),
      m_rotation(Quaternion::Identity())
{
    UpdateMatrix();
}

Transform::Transform(const Vec3f &translation, const Vec3f &scale, const Quaternion &rotation)
    : m_translation(translation),
      m_scale(scale),
      m_rotation(rotation)
{
    UpdateMatrix();
}

Transform::Transform(const Vec3f &translation)
    : Transform(translation, Vec3f::One(), Quaternion::Identity())
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
