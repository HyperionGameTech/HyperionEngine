/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TRANSFORM_HPP
#define HYPERION_TRANSFORM_HPP

#include <math/Vector3.hpp>
#include <math/Quaternion.hpp>
#include <math/Matrix4.hpp>

#include <HashCode.hpp>

namespace hyperion {
class HYP_API Transform
{
public:
    static const Transform identity;

    Transform();
    explicit Transform(const Vec3f &translation);
    Transform(const Vec3f &translation, const Vec3f &scale);
    Transform(const Vec3f &translation, const Vec3f &scale, const Quaternion &rotation);
    Transform(const Transform &other);

    const Vec3f &GetTranslation() const
        { return m_translation; }

    /** returns a reference to the translation - if modified, you must call UpdateMatrix(). */
    Vec3f &GetTranslation()
        { return m_translation; }

    void SetTranslation(const Vec3f &translation)
        { m_translation = translation; UpdateMatrix(); }

    const Vec3f &GetScale() const
        { return m_scale; }

    /** returns a reference to the scale - if modified, you must call UpdateMatrix(). */
    Vec3f &GetScale()
        { return m_scale; }

    void SetScale(const Vec3f &scale)
        { m_scale = scale; UpdateMatrix(); }

    const Quaternion &GetRotation() const
        { return m_rotation; }

    /** returns a reference to the rotation - if modified, you must call UpdateMatrix(). */
    Quaternion &GetRotation()
        { return m_rotation; }

    void SetRotation(const Quaternion &rotation)
        { m_rotation = rotation; UpdateMatrix(); }

    void UpdateMatrix();
    const Matrix4 &GetMatrix() const
        { return m_matrix; }

    Transform GetInverse() const;

    Transform operator*(const Transform &other) const;
    Transform &operator*=(const Transform &other);

    bool operator==(const Transform &other) const
        { return m_matrix == other.m_matrix; }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_matrix.GetHashCode());

        return hc;
    }

private:
    Vec3f       m_translation;
    Vec3f       m_scale;
    Quaternion  m_rotation;
    Matrix4     m_matrix;
};

static_assert(sizeof(Transform) == 112, "Expected sizeof(Transform) to equal 112 bytes to match C# size");

} // namespace hyperion

#endif
