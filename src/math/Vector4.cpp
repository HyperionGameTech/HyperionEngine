#include "Vector4.hpp"
#include "MathUtil.hpp"
#include "Vector3.hpp"
#include "Vector2.hpp"
#include "Matrix4.hpp"

namespace hyperion {

template <>
const Vec4<Int> math::detail::Vec4<Int>::zero = Vec4<Int>(0);
template <>
const Vec4<Int> math::detail::Vec4<Int>::one = Vec4<Int>(1);

template <>
const Vec4<UInt> math::detail::Vec4<UInt>::zero = Vec4<UInt>(0);
template <>
const Vec4<UInt> math::detail::Vec4<UInt>::one = Vec4<UInt>(1);

const Vec4<Float> math::detail::Vec4<Float>::zero = Vec4<Float>(0.0f);
const Vec4<Float> math::detail::Vec4<Float>::one = Vec4<Float>(1.0f);



template<>
Int math::detail::Vec4<Int>::Max() const
{
    return MathUtil::Max(x, MathUtil::Max(y, MathUtil::Max(z, w)));
}

template<>
Int math::detail::Vec4<Int>::Min() const
{
    return MathUtil::Min(x, MathUtil::Min(y, MathUtil::Min(z, w)));
}

template<>
UInt math::detail::Vec4<UInt>::Max() const
{
    return MathUtil::Max(x, MathUtil::Max(y, MathUtil::Max(z, w)));
}

template<>
UInt math::detail::Vec4<UInt>::Min() const
{
    return MathUtil::Min(x, MathUtil::Min(y, MathUtil::Min(z, w)));
}

Float math::detail::Vec4<Float>::Max() const
{
    return MathUtil::Max(x, MathUtil::Max(y, MathUtil::Max(z, w)));
}

Float math::detail::Vec4<Float>::Min() const
{
    return MathUtil::Min(x, MathUtil::Min(y, MathUtil::Min(z, w)));
}

Float math::detail::Vec4<Float>::DistanceSquared(const Vec4 &other) const
{
    float dx = x - other.x;
    float dy = y - other.y;
    float dz = z - other.z;
    float dw = w - other.w;
    return dx * dx + dy * dy + dz * dz + dw * dw;
}

/* Euclidean distance */
Float math::detail::Vec4<Float>::Distance(const Vec4 &other) const
{
    return MathUtil::Sqrt(DistanceSquared(other));
}

Vec4<Float> math::detail::Vec4<Float>::Normalized() const
{
    return *this / MathUtil::Max(Length(), MathUtil::epsilon_f);
}

Vec4<Float> &math::detail::Vec4<Float>::Normalize()
{
    return *this /= MathUtil::Max(Length(), MathUtil::epsilon_f);
}

Vec4<Float> &math::detail::Vec4<Float>::Rotate(const Vec3<Float> &axis, Float radians)
{
    return (*this) = Matrix4::Rotation(axis, radians) * (*this);
}

Vec4<Float> &math::detail::Vec4<Float>::Lerp(const Vec4<Float> &to, Float amt)
{
    x = MathUtil::Lerp(x, to.x, amt);
    y = MathUtil::Lerp(y, to.y, amt);
    z = MathUtil::Lerp(z, to.z, amt);
    w = MathUtil::Lerp(w, to.w, amt);

    return *this;
}

Float math::detail::Vec4<Float>::Dot(const Vec4<Float> &other) const
{
    return x * other.x + y * other.y + z * other.z + w * other.w;
}

template <>
Vec4<Int> math::detail::Vec4<Int>::Abs(const Vec4<Int> &vec)
{
    return {
        MathUtil::Abs(vec.x),
        MathUtil::Abs(vec.y),
        MathUtil::Abs(vec.z),
        MathUtil::Abs(vec.w)
    };
}

template <>
Vec4<Int> math::detail::Vec4<Int>::Min(const Vec4<Int> &a, const Vec4<Int> &b)
{
    return {
        MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z),
        MathUtil::Min(a.w, b.w)
    };
}

template <>
Vec4<Int> math::detail::Vec4<Int>::Max(const Vec4<Int> &a, const Vec4<Int> &b)
{
    return {
        MathUtil::Max(a.x, b.x),
        MathUtil::Max(a.y, b.y),
        MathUtil::Max(a.z, b.z),
        MathUtil::Max(a.w, b.w)
    };
}

template <>
Vec4<UInt> math::detail::Vec4<UInt>::Abs(const Vec4<UInt> &vec)
{
    return {
        MathUtil::Abs(vec.x),
        MathUtil::Abs(vec.y),
        MathUtil::Abs(vec.z),
        MathUtil::Abs(vec.w)
    };
}

template <>
Vec4<UInt> math::detail::Vec4<UInt>::Min(const Vec4<UInt> &a, const Vec4<UInt> &b)
{
    return {
        MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z),
        MathUtil::Min(a.w, b.w)
    };
}

template <>
Vec4<UInt> math::detail::Vec4<UInt>::Max(const Vec4<UInt> &a, const Vec4<UInt> &b)
{
    return {
        MathUtil::Max(a.x, b.x),
        MathUtil::Max(a.y, b.y),
        MathUtil::Max(a.z, b.z),
        MathUtil::Max(a.w, b.w)
    };
}

Vec4<Float> math::detail::Vec4<Float>::Abs(const Vec4<Float> &vec)
{
    return {
        MathUtil::Abs(vec.x),
        MathUtil::Abs(vec.y),
        MathUtil::Abs(vec.z),
        MathUtil::Abs(vec.w)
    };
}

Vec4<Float> math::detail::Vec4<Float>::Round(const Vec4<Float> &vec)
{
    return {
        MathUtil::Round(vec.x),
        MathUtil::Round(vec.y),
        MathUtil::Round(vec.z),
        MathUtil::Round(vec.w)
    };
}

Vec4<Float> math::detail::Vec4<Float>::Clamp(const Vec4<Float> &vec, Float min_value, Float max_value)
{
    return Max(min_value, Min(vec, max_value));
}

Vec4<Float> math::detail::Vec4<Float>::Min(const Vec4<Float> &a, const Vec4<Float> &b)
{
    return {
        MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z),
        MathUtil::Min(a.w, b.w)
    };
}

Vec4<Float> math::detail::Vec4<Float>::Max(const Vec4<Float> &a, const Vec4<Float> &b)
{
    return {
        MathUtil::Max(a.x, b.x),
        MathUtil::Max(a.y, b.y),
        MathUtil::Max(a.z, b.z),
        MathUtil::Max(a.w, b.w)
    };
}

Vec4<Float> math::detail::Vec4<Float>::operator*(const Matrix4 &mat) const
{
    return {
        x * mat.values[0] + y * mat.values[4] + z * mat.values[8]  + w * mat.values[12],
        x * mat.values[1] + y * mat.values[5] + z * mat.values[9]  + w * mat.values[13],
        x * mat.values[2] + y * mat.values[6] + z * mat.values[10] + w * mat.values[14],
        x * mat.values[3] + y * mat.values[7] + z * mat.values[11] + w * mat.values[15]
    };
}

std::ostream &operator<<(std::ostream &out, const Vec4<Float> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << "]";
    return out;
}

std::ostream &operator<<(std::ostream &out, const Vec4<Int> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << "]";
    return out;
}

std::ostream &operator<<(std::ostream &out, const Vec4<UInt> &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << "]";
    return out;
}

} // namespace hyperion
