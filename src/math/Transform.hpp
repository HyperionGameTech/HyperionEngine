#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "../HashCode.hpp"
#include "Vector3.hpp"
#include "Quaternion.hpp"
#include "Matrix4.hpp"

namespace hyperion {
class Transform
{
public:
    static const Transform identity;

    Transform();
    explicit Transform(const Vector3 &translation);
    Transform(const Vector3 &translation, const Vector3 &scale);
    Transform(const Vector3 &translation, const Vector3 &scale, const Quaternion &rotation);
    Transform(const Transform &other);

    const Vector3 &GetTranslation() const { return m_translation; }
    /** returns a reference to the translation - if modified, you must call UpdateMatrix(). */
    Vector3 &GetTranslation() { return m_translation; }
    void SetTranslation(const Vector3 &translation) { m_translation = translation; UpdateMatrix(); }

    const Vector3 &GetScale() const { return m_scale; }
    /** returns a reference to the scale - if modified, you must call UpdateMatrix(). */
    Vector3 &GetScale() { return m_scale; }
    void SetScale(const Vector3 &scale) { m_scale = scale; UpdateMatrix(); }

    const Quaternion &GetRotation() const { return m_rotation; }
    /** returns a reference to the rotation - if modified, you must call UpdateMatrix(). */
    Quaternion &GetRotation() { return m_rotation; }
    void SetRotation(const Quaternion &rotation) { m_rotation = rotation; UpdateMatrix(); }

    void UpdateMatrix();
    const Matrix4 &GetMatrix() const { return m_matrix; }

    Transform operator*(const Transform &other) const;
    Transform &operator*=(const Transform &other);

    bool operator==(const Transform &other) const
        { return m_matrix == other.m_matrix; }

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_matrix.GetHashCode());

        return hc;
    }

private:
    Vector3     m_translation;
    Vector3     m_scale;
    Quaternion  m_rotation;
    Matrix4     m_matrix;
};

} // namespace hyperion

#endif
