/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/Vector4.hpp>
#include <core/math/MathUtil.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector2.hpp>
#include <core/math/Matrix4.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(Vec4f, -1, 0, {})
    HypField(NAME(HYP_STR(x)), &Type::x, offsetof(Type, x)),
    HypField(NAME(HYP_STR(y)), &Type::y, offsetof(Type, y)),
    HypField(NAME(HYP_STR(z)), &Type::z, offsetof(Type, z)),
    HypField(NAME(HYP_STR(w)), &Type::w, offsetof(Type, w))
HYP_END_STRUCT

HYP_BEGIN_STRUCT(Vec4i, -1, 0, {})
    HypField(NAME(HYP_STR(x)), &Type::x, offsetof(Type, x)),
    HypField(NAME(HYP_STR(y)), &Type::y, offsetof(Type, y)),
    HypField(NAME(HYP_STR(z)), &Type::z, offsetof(Type, z)),
    HypField(NAME(HYP_STR(w)), &Type::w, offsetof(Type, w))
HYP_END_STRUCT

HYP_BEGIN_STRUCT(Vec4u, -1, 0, {})
    HypField(NAME(HYP_STR(x)), &Type::x, offsetof(Type, x)),
    HypField(NAME(HYP_STR(y)), &Type::y, offsetof(Type, y)),
    HypField(NAME(HYP_STR(z)), &Type::z, offsetof(Type, z)),
    HypField(NAME(HYP_STR(w)), &Type::w, offsetof(Type, w))
HYP_END_STRUCT

float math::detail::Vec4<float>::DistanceSquared(const Vec4 &other) const
{
    float dx = x - other.x;
    float dy = y - other.y;
    float dz = z - other.z;
    float dw = w - other.w;
    return dx * dx + dy * dy + dz * dz + dw * dw;
}

/* Euclidean distance */
float math::detail::Vec4<float>::Distance(const Vec4 &other) const
{
    return MathUtil::Sqrt(DistanceSquared(other));
}

Vec4<float> math::detail::Vec4<float>::Normalized() const
{
    return *this / MathUtil::Max(Length(), MathUtil::epsilon_f);
}

Vec4<float> &math::detail::Vec4<float>::Normalize()
{
    return *this /= MathUtil::Max(Length(), MathUtil::epsilon_f);
}

Vec4<float> &math::detail::Vec4<float>::Rotate(const Vec3<float> &axis, float radians)
{
    return (*this) = Matrix4::Rotation(axis, radians) * (*this);
}

Vec4<float> &math::detail::Vec4<float>::Lerp(const Vec4<float> &to, float amt)
{
    x = MathUtil::Lerp(x, to.x, amt);
    y = MathUtil::Lerp(y, to.y, amt);
    z = MathUtil::Lerp(z, to.z, amt);
    w = MathUtil::Lerp(w, to.w, amt);

    return *this;
}

float math::detail::Vec4<float>::Dot(const Vec4<float> &other) const
{
    return x * other.x + y * other.y + z * other.z + w * other.w;
}

template <>
Vec4<int> math::detail::Vec4<int>::Abs(const Vec4<int> &vec)
{
    return {
        MathUtil::Abs(vec.x),
        MathUtil::Abs(vec.y),
        MathUtil::Abs(vec.z),
        MathUtil::Abs(vec.w)
    };
}

template <>
Vec4<int> math::detail::Vec4<int>::Min(const Vec4<int> &a, const Vec4<int> &b)
{
    return {
        MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z),
        MathUtil::Min(a.w, b.w)
    };
}

template <>
Vec4<int> math::detail::Vec4<int>::Max(const Vec4<int> &a, const Vec4<int> &b)
{
    return {
        MathUtil::Max(a.x, b.x),
        MathUtil::Max(a.y, b.y),
        MathUtil::Max(a.z, b.z),
        MathUtil::Max(a.w, b.w)
    };
}

template <>
Vec4<uint32> math::detail::Vec4<uint32>::Abs(const Vec4<uint32> &vec)
{
    return {
        MathUtil::Abs(vec.x),
        MathUtil::Abs(vec.y),
        MathUtil::Abs(vec.z),
        MathUtil::Abs(vec.w)
    };
}

template <>
Vec4<uint32> math::detail::Vec4<uint32>::Min(const Vec4<uint32> &a, const Vec4<uint32> &b)
{
    return {
        MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z),
        MathUtil::Min(a.w, b.w)
    };
}

template <>
Vec4<uint32> math::detail::Vec4<uint32>::Max(const Vec4<uint32> &a, const Vec4<uint32> &b)
{
    return {
        MathUtil::Max(a.x, b.x),
        MathUtil::Max(a.y, b.y),
        MathUtil::Max(a.z, b.z),
        MathUtil::Max(a.w, b.w)
    };
}

Vec4<float> math::detail::Vec4<float>::Abs(const Vec4<float> &vec)
{
    return {
        MathUtil::Abs(vec.x),
        MathUtil::Abs(vec.y),
        MathUtil::Abs(vec.z),
        MathUtil::Abs(vec.w)
    };
}

Vec4<float> math::detail::Vec4<float>::Round(const Vec4<float> &vec)
{
    return {
        MathUtil::Round(vec.x),
        MathUtil::Round(vec.y),
        MathUtil::Round(vec.z),
        MathUtil::Round(vec.w)
    };
}

Vec4<float> math::detail::Vec4<float>::Clamp(const Vec4<float> &vec, float min_value, float max_value)
{
    return Max(min_value, Min(vec, max_value));
}

Vec4<float> math::detail::Vec4<float>::Min(const Vec4<float> &a, const Vec4<float> &b)
{
    return {
        MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z),
        MathUtil::Min(a.w, b.w)
    };
}

Vec4<float> math::detail::Vec4<float>::Max(const Vec4<float> &a, const Vec4<float> &b)
{
    return {
        MathUtil::Max(a.x, b.x),
        MathUtil::Max(a.y, b.y),
        MathUtil::Max(a.z, b.z),
        MathUtil::Max(a.w, b.w)
    };
}

Vec4<float> math::detail::Vec4<float>::operator*(const Matrix4 &mat) const
{
    return {
        x * mat.values[0] + y * mat.values[4] + z * mat.values[8]  + w * mat.values[12],
        x * mat.values[1] + y * mat.values[5] + z * mat.values[9]  + w * mat.values[13],
        x * mat.values[2] + y * mat.values[6] + z * mat.values[10] + w * mat.values[14],
        x * mat.values[3] + y * mat.values[7] + z * mat.values[11] + w * mat.values[15]
    };
}

std::ostream &operator<<(std::ostream &out, const Vec4<float> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << "]";
    return out;
}

std::ostream &operator<<(std::ostream &out, const Vec4<int> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << "]";
    return out;
}

std::ostream &operator<<(std::ostream &out, const Vec4<uint32> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << "]";
    return out;
}

} // namespace hyperion
