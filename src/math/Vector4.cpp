/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <math/Vector4.hpp>
#include <math/MathUtil.hpp>
#include <math/Vector3.hpp>
#include <math/Vector2.hpp>
#include <math/Matrix4.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

// HYP_DEFINE_CLASS(
//     Vec4f,
//     HypProperty(NAME("X"), static_cast<float (Vec4f::*)() const>(&Vec4f::GetX), &Vec4f::SetX),
//     HypProperty(NAME("Y"), static_cast<float (Vec4f::*)() const>(&Vec4f::GetY), &Vec4f::SetY),
//     HypProperty(NAME("Z"), static_cast<float (Vec4f::*)() const>(&Vec4f::GetZ), &Vec4f::SetZ),
//     HypProperty(NAME("W"), static_cast<float (Vec4f::*)() const>(&Vec4f::GetW), &Vec4f::SetW)
// );

// HYP_DEFINE_CLASS(
//     Vec4i,
//     HypProperty(NAME("X"), static_cast<int (Vec4i::*)() const>(&Vec4i::GetX), &Vec4i::SetX),
//     HypProperty(NAME("Y"), static_cast<int (Vec4i::*)() const>(&Vec4i::GetY), &Vec4i::SetY),
//     HypProperty(NAME("Z"), static_cast<int (Vec4i::*)() const>(&Vec4i::GetZ), &Vec4i::SetZ),
//     HypProperty(NAME("W"), static_cast<int (Vec4i::*)() const>(&Vec4i::GetW), &Vec4i::SetW)
// );

// HYP_DEFINE_CLASS(
//     Vec4u,
//     HypProperty(NAME("X"), static_cast<uint (Vec4u::*)() const>(&Vec4u::GetX), &Vec4u::SetX),
//     HypProperty(NAME("Y"), static_cast<uint (Vec4u::*)() const>(&Vec4u::GetY), &Vec4u::SetY),
//     HypProperty(NAME("Z"), static_cast<uint (Vec4u::*)() const>(&Vec4u::GetZ), &Vec4u::SetZ),
//     HypProperty(NAME("W"), static_cast<uint (Vec4u::*)() const>(&Vec4u::GetW), &Vec4u::SetW)
// );

template<>
int math::detail::Vec4<int>::Max() const
{
    return MathUtil::Max(x, MathUtil::Max(y, MathUtil::Max(z, w)));
}

template<>
int math::detail::Vec4<int>::Min() const
{
    return MathUtil::Min(x, MathUtil::Min(y, MathUtil::Min(z, w)));
}

template<>
uint math::detail::Vec4<uint>::Max() const
{
    return MathUtil::Max(x, MathUtil::Max(y, MathUtil::Max(z, w)));
}

template<>
uint math::detail::Vec4<uint>::Min() const
{
    return MathUtil::Min(x, MathUtil::Min(y, MathUtil::Min(z, w)));
}

float math::detail::Vec4<float>::Max() const
{
    return MathUtil::Max(x, MathUtil::Max(y, MathUtil::Max(z, w)));
}

float math::detail::Vec4<float>::Min() const
{
    return MathUtil::Min(x, MathUtil::Min(y, MathUtil::Min(z, w)));
}

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
Vec4<uint> math::detail::Vec4<uint>::Abs(const Vec4<uint> &vec)
{
    return {
        MathUtil::Abs(vec.x),
        MathUtil::Abs(vec.y),
        MathUtil::Abs(vec.z),
        MathUtil::Abs(vec.w)
    };
}

template <>
Vec4<uint> math::detail::Vec4<uint>::Min(const Vec4<uint> &a, const Vec4<uint> &b)
{
    return {
        MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z),
        MathUtil::Min(a.w, b.w)
    };
}

template <>
Vec4<uint> math::detail::Vec4<uint>::Max(const Vec4<uint> &a, const Vec4<uint> &b)
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

std::ostream &operator<<(std::ostream &out, const Vec4<uint> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << "]";
    return out;
}

} // namespace hyperion
