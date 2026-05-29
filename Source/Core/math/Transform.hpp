/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/math/Vector3.hpp>
#include <Core/math/Quat4f.hpp>
#include <Core/math/Mat4f.hpp>

#include <Core/reflection/ObjectMacros.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {

HYP_STRUCT(Size = 48, Serialize = "bitwise")
struct alignas(16) CORE_API Transform
{
    HYP_STRUCT_BODY(Transform);

    static const Transform identity;

    HYP_FIELD()
    Vec3f translation;

    HYP_FIELD()
    Vec3f scale;

    HYP_FIELD()
    Quat4f rotation;

    Transform();
    explicit Transform(const Vec3f& translation);
    Transform(const Vec3f& translation, const Vec3f& scale);
    Transform(const Vec3f& translation, const Vec3f& scale, const Quat4f& rotation);

    Transform(const Transform& other) = default;
    Transform& operator=(const Transform& other) = default;

    HYP_FORCE_INLINE const Vec3f& GetTranslation() const
    {
        return translation;
    }

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

    HYP_FORCE_INLINE Vec3f& GetScale()
    {
        return scale;
    }

    HYP_FORCE_INLINE void SetScale(const Vec3f& scale)
    {
        this->scale = scale;
    }

    HYP_FORCE_INLINE const Quat4f& GetRotation() const
    {
        return rotation;
    }

    HYP_FORCE_INLINE Quat4f& GetRotation()
    {
        return rotation;
    }

    HYP_FORCE_INLINE void SetRotation(const Quat4f& rotation)
    {
        this->rotation = rotation;
    }

    HYP_METHOD()
    Mat4f GetMatrix() const
    {
        const Mat4f t = Mat4f::Translation(translation);
        const Mat4f r = Mat4f::Rotation(rotation);
        const Mat4f s = Mat4f::Scaling(scale);

        return t * r * s;
    }

    HYP_FORCE_INLINE explicit operator Mat4f() const
    {
        return GetMatrix();
    }

    Transform GetInverse() const;

    HYP_METHOD()
    Transform operator*(const Transform& other) const;
    Transform& operator*=(const Transform& other);

    HYP_METHOD()
    HYP_FORCE_INLINE bool operator==(const Transform& other) const
    {
        return translation == other.translation
            && scale == other.scale
            && rotation == other.rotation;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE bool operator!=(const Transform& other) const
    {
        return translation != other.translation
            || scale != other.scale
            || rotation != other.rotation;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return translation.GetHashCode()
            .Combine(scale.GetHashCode())
            .Combine(rotation.GetHashCode());
    }
};

} // namespace Hyperion
