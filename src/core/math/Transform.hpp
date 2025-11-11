/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/Vector3.hpp>
#include <core/math/Quaternion.hpp>
#include <core/math/Mat4f.hpp>

#include <core/reflection/ObjectMacros.hpp>

#include <core/HashCode.hpp>

namespace hyperion {

HYP_STRUCT(Size = 48, Serialize = "bitwise")
struct alignas(16) HYP_API Transform
{
    HYP_STRUCT_BODY(Transform);

    static const Transform identity;

    HYP_FIELD()
    Vec3f translation;

    HYP_FIELD()
    Vec3f scale;

    HYP_FIELD()
    Quaternion rotation;

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
    }

    Mat4f GetMatrix() const
    {
        const Mat4f t = Mat4f::Translation(translation);
        const Mat4f r = Mat4f::Rotation(rotation);
        const Mat4f s = Mat4f::Scaling(scale);

        return t * r * s;
    }

    Transform GetInverse() const;

    Transform operator*(const Transform& other) const;
    Transform& operator*=(const Transform& other);

    HYP_FORCE_INLINE bool operator==(const Transform& other) const = default;
    HYP_FORCE_INLINE bool operator!=(const Transform& other) const = default;

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return translation.GetHashCode()
            .Combine(scale.GetHashCode())
            .Combine(rotation.GetHashCode());
    }
};

} // namespace hyperion
