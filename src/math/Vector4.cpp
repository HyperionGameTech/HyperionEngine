#include "Vector4.hpp"
#include "MathUtil.hpp"
#include "Vector3.hpp"
#include "Vector2.hpp"
#include "Matrix4.hpp"

namespace hyperion {

const Vector4 Vector4::zero = Vector4(0.0f);
const Vector4 Vector4::one = Vector4(1.0f);

Vector4::Vector4()
    : x(0.0f), 
      y(0.0f), 
      z(0.0f), 
      w(0.0f)
{
}

Vector4::Vector4(float x, float y, float z, float w)
    : x(x), 
      y(y),
      z(z), 
      w(w)
{
}

Vector4::Vector4(float xyzw)
    : x(xyzw), 
      y(xyzw), 
      z(xyzw), 
      w(xyzw)
{
}

Vector4::Vector4(const Vector2 &xy, float z, float w)
    : x(xy.x),
      y(xy.y),
      z(z),
      w(w)
{
}

Vector4::Vector4(const Vector2 &xy, const Vector2 &zw)
    : x(xy.x),
      y(xy.y),
      z(zw.x),
      w(zw.y)
{
}

Vector4::Vector4(const Vector3 &xyz, float w)
    : x(xyz.x),
      y(xyz.y),
      z(xyz.z),
      w(w)
{
}

Vector4::Vector4(const Vector4 &other)
    : x(other.x), 
      y(other.y), 
      z(other.z), 
      w(other.w)
{
}

Vector4 &Vector4::operator=(const Vector4 &other)
{
    x = other.x;
    y = other.y;
    z = other.z;
    w = other.w;
    return *this;
}

Vector4 Vector4::operator+(const Vector4 &other) const
{
    return Vector4(x + other.x, y + other.y, z + other.z, w + other.w);
}

Vector4 &Vector4::operator+=(const Vector4 &other)
{
    x += other.x;
    y += other.y;
    z += other.z;
    w += other.w;
    return *this;
}

Vector4 Vector4::operator-(const Vector4 &other) const
{
    return Vector4(x - other.x, y - other.y, z - other.z, w - other.w);
}

Vector4 &Vector4::operator-=(const Vector4 &other)
{
    x -= other.x;
    y -= other.y;
    z -= other.z;
    w -= other.w;
    return *this;
}

Vector4 Vector4::operator*(const Vector4 &other) const
{
    return Vector4(x * other.x, y * other.y, z * other.z, w * other.w);
}

Vector4 &Vector4::operator*=(const Vector4 &other)
{
    x *= other.x;
    y *= other.y;
    z *= other.z;
    w *= other.w;
    return *this;
}

Vector4 Vector4::operator*(const Matrix4 &mat) const
{
    return {
        x * mat.values[0] + y * mat.values[4] + z * mat.values[8]  + w * mat.values[12],
        x * mat.values[1] + y * mat.values[5] + z * mat.values[9]  + w * mat.values[13],
        x * mat.values[2] + y * mat.values[6] + z * mat.values[10] + w * mat.values[14],
        x * mat.values[3] + y * mat.values[7] + z * mat.values[11] + w * mat.values[15]
    };
}

Vector4 &Vector4::operator*=(const Matrix4 &mat)
{
    return operator=(operator*(mat));
}

Vector4 Vector4::operator/(const Vector4 &other) const
{
    return Vector4(x / other.x, y / other.y, z / other.z, w / other.w);
}

Vector4 &Vector4::operator/=(const Vector4 &other)
{
    x /= other.x;
    y /= other.y;
    z /= other.z;
    w /= other.w;
    return *this;
}

bool Vector4::operator==(const Vector4 &other) const
{
    return MathUtil::ApproxEqual(x, other.x)
        && MathUtil::ApproxEqual(y, other.y)
        && MathUtil::ApproxEqual(z, other.z)
        && MathUtil::ApproxEqual(w, other.w);

    // return x == other.x && y == other.y && z == other.z && w == other.w;
}

bool Vector4::operator!=(const Vector4 &other) const
{
    return !((*this) == other);
}

float Vector4::Max() const
{
    return MathUtil::Max(x, MathUtil::Max(y, MathUtil::Max(z, w)));
}

float Vector4::Min() const
{
    return MathUtil::Min(x, MathUtil::Min(y, MathUtil::Min(z, w)));
}

float Vector4::DistanceSquared(const Vector4 &other) const
{
    float dx = x - other.x;
    float dy = y - other.y;
    float dz = z - other.z;
    float dw = w - other.w;
    return dx * dx + dy * dy + dz * dz + dw * dw;
}

float Vector4::Distance(const Vector4 &other) const
{
    return MathUtil::Sqrt(DistanceSquared(other));
}

Vector4 Vector4::Normalized() const
{
    return *this / MathUtil::Max(Length(), MathUtil::epsilon<float>);
}

Vector4 &Vector4::Normalize()
{
    return *this /= MathUtil::Max(Length(), MathUtil::epsilon<float>);
}

Vector4 &Vector4::Rotate(const Vector3 &axis, float radians)
{
    return (*this) *= Matrix4::Rotation(axis, radians);
}

Vector4 &Vector4::Lerp(const Vector4 &to, float amt)
{
    x = MathUtil::Lerp(x, to.x, amt);
    y = MathUtil::Lerp(y, to.y, amt);
    z = MathUtil::Lerp(z, to.z, amt);
    w = MathUtil::Lerp(w, to.w, amt);
    return *this;
}

float Vector4::Dot(const Vector4 &other) const
{
    return x * other.x + y * other.y + z * other.z + w * other.w;
}

Vector4 Vector4::Abs(const Vector4 &vec)
{
    return {
        MathUtil::Abs(vec.x),
        MathUtil::Abs(vec.y),
        MathUtil::Abs(vec.z),
        MathUtil::Abs(vec.w)
    };
}

Vector4 Vector4::Round(const Vector4 &vec)
{
    return {
        MathUtil::Round(vec.x),
        MathUtil::Round(vec.y),
        MathUtil::Round(vec.z),
        MathUtil::Round(vec.w)
    };
}

Vector4 Vector4::Clamp(const Vector4 &vec, float min_value, float max_value)
{
    return Max(min_value, Min(vec, max_value));
}

Vector4 Vector4::Min(const Vector4 &a, const Vector4 &b)
{
    return Vector4(MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z),
        MathUtil::Min(a.w, b.w));
}

Vector4 Vector4::Max(const Vector4 &a, const Vector4 &b)
{
    return Vector4(MathUtil::Max(a.x, b.x),
        MathUtil::Max(a.y, b.y),
        MathUtil::Max(a.z, b.z),
        MathUtil::Max(a.w, b.w));
}

Vector4 Vector4::Zero()
{
    return Vector4(0, 0, 0, 0);
}

Vector4 Vector4::One()
{
    return Vector4(1, 1, 1, 1);
}

Vector4 Vector4::UnitX()
{
    return Vector4(1, 0, 0, 0);
}

Vector4 Vector4::UnitY()
{
    return Vector4(0, 1, 0, 0);
}

Vector4 Vector4::UnitZ()
{
    return Vector4(0, 0, 1, 0);
}

Vector4 Vector4::UnitW()
{
    return Vector4(0, 0, 0, 1);
}

std::ostream &operator<<(std::ostream &out, const Vector4 &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << "]";
    return out;
}

} // namespace hyperion
