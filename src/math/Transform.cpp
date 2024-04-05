#include "Transform.hpp"

namespace hyperion {

const Transform Transform::identity{};

Transform::Transform()
    : m_translation(Vector3::zero),
      m_scale(Vector3::one),
      m_rotation(Quaternion::identity)
{
    UpdateMatrix();
}

Transform::Transform(const Vector3 &translation, const Vector3 &scale)
    : m_translation(translation),
      m_scale(scale),
      m_rotation(Quaternion::identity)
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
    : Transform(translation, Vector3::one, Quaternion::identity)
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
    const auto t = Matrix4::Translation(m_translation);
    const auto r = Matrix4::Rotation(m_rotation);
    const auto s = Matrix4::Scaling(m_scale);

    m_matrix = t * r * s;
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
