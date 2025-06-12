/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TRANSFORM_HPP
#define HYPERION_TRANSFORM_HPP

#include <core/math/Vector3.hpp>
#include <core/math/Quaternion.hpp>
#include <core/math/Matrix4.hpp>

#include <HashCode.hpp>

namespace hyperion {

HYP_STRUCT(Size = 112, Serialize = "bitwise")
struct alignas(16) HYP_API Transform
{
    static const Transform identity;

    HYP_FIELD()
    Vec3f translation;

    HYP_FIELD()
    Vec3f scale;

    HYP_FIELD()
    Quaternion rotation;

    HYP_FIELD()
    Matrix4 matrix;

    Transform();
    explicit Transform(const Vec3f& translation);
    Transform(const Vec3f& translation, const Vec3f& scale);
    Transform(const Vec3f& translation, const Vec3f& scale, const Quaternion& rotation);

    Transform(const Transform& other) = default;
    Transform& operator=(const Transform& other) = default;

    HYP_FORCE_INLINE const Vec3f& GetTranslation() const
    {
        return translation;
    }

    /** returns a reference to the translation - if modified, you must call UpdateMatrix(). */
    HYP_FORCE_INLINE Vec3f& GetTranslation()
    {
        return translation;
    }

    HYP_FORCE_INLINE void SetTranslation(const Vec3f& translation)
    {
        this->translation = translation;
        UpdateMatrix();
    }

    HYP_FORCE_INLINE const Vec3f& GetScale() const
    {
        return scale;
    }

    /** returns a reference to the scale - if modified, you must call UpdateMatrix(). */
    HYP_FORCE_INLINE Vec3f& GetScale()
    {
        return scale;
    }

    HYP_FORCE_INLINE void SetScale(const Vec3f& scale)
    {
        this->scale = scale;
        UpdateMatrix();
    }

    HYP_FORCE_INLINE const Quaternion& GetRotation() const
    {
        return rotation;
    }

    /** returns a reference to the rotation - if modified, you must call UpdateMatrix(). */
    HYP_FORCE_INLINE Quaternion& GetRotation()
    {
        return rotation;
    }

    HYP_FORCE_INLINE void SetRotation(const Quaternion& rotation)
    {
        this->rotation = rotation;
        UpdateMatrix();
    }

    void UpdateMatrix();

    HYP_FORCE_INLINE const Matrix4& GetMatrix() const
    {
        return this->matrix;
    }

    Transform GetInverse() const;

    Transform operator*(const Transform& other) const;
    Transform& operator*=(const Transform& other);

    HYP_FORCE_INLINE bool operator==(const Transform& other) const
    {
        return matrix == other.matrix;
    }

    HYP_FORCE_INLINE bool operator!=(const Transform& other) const
    {
        return matrix != other.matrix;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(matrix.GetHashCode());

        return hc;
    }
};

} // namespace hyperion

#endif
