#include "Vector3.hpp"
#include "Quaternion.hpp"

#include <cmath>

namespace hyperion {

Vector3::Vector3()
    : x(0.0f), 
      y(0.0f), 
      z(0.0f)
{
}

Vector3::Vector3(float x, float y, float z)
    : x(x), 
      y(y), 
      z(z)
{
}

Vector3::Vector3(float xyz)
    : x(xyz), 
      y(xyz), 
      z(xyz)
{
}

Vector3::Vector3(const Vector2 &xy, float z)
    : x(xy.x),
      y(xy.y),
      z(z)
{
}

Vector3::Vector3(const Vector4 &vec)
    : x(vec.x),
      y(vec.y),
      z(vec.z)
{
    const float mag = MathUtil::Max(vec.w, MathUtil::epsilon<float>);

    x /= mag;
    y /= mag;
    z /= mag;
}

Vector3::Vector3(const Vector3 &other)
    : x(other.x), 
      y(other.y), 
      z(other.z)
{
}

Vector3 &Vector3::operator=(const Vector3 &other)
{
    x = other.x;
    y = other.y;
    z = other.z;
    return *this;
}

Vector3 Vector3::operator+(const Vector3 &other) const
{
    return Vector3(x + other.x, y + other.y, z + other.z);
}

Vector3 &Vector3::operator+=(const Vector3 &other)
{
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
}

Vector3 Vector3::operator-(const Vector3 &other) const
{
    return Vector3(x - other.x, y - other.y, z - other.z);
}

Vector3 &Vector3::operator-=(const Vector3 &other)
{
    x -= other.x;
    y -= other.y;
    z -= other.z;
    return *this;
}

Vector3 Vector3::operator*(const Vector3 &other) const
{
    return Vector3(x * other.x, y * other.y, z * other.z);
}

Vector3 &Vector3::operator*=(const Vector3 &other)
{
    x *= other.x;
    y *= other.y;
    z *= other.z;
    return *this;
}

Vector3 Vector3::operator*(const Matrix3 &mat) const
{
    return Vector3(x * mat(0, 0) + y * mat(0, 1) + z * mat(0, 2),
        x * mat(1, 0) + y * mat(1, 1) + z * mat(1, 2),
        x * mat(2, 0) + y * mat(2, 1) + z * mat(2, 2));
}

Vector3 &Vector3::operator*=(const Matrix3 &mat)
{
    return operator=(operator*(mat));
}

Vector3 Vector3::operator*(const Matrix4 &mat) const
{
    return Vector3(x * mat(0, 0) + y * mat(0, 1) + z * mat(0, 2) + 1.0f * mat(0, 3),
        x * mat(1, 0) + y * mat(1, 1) + z * mat(1, 2) + 1.0f * mat(1, 3),
        x * mat(2, 0) + y * mat(2, 1) + z * mat(2, 2) + 1.0f * mat(2, 3));
}

Vector3 &Vector3::operator*=(const Matrix4 &mat)
{
    return operator=(operator*(mat));
}

Vector3 Vector3::operator*(const Quaternion &quat) const
{
    Vector3 result;
    result.x = quat.w * quat.w * x + 2 * 
        quat.y * quat.w * z - 2 * 
        quat.z * quat.w * y + 
        quat.x * quat.x * x + 2 * 
        quat.y * quat.x * y + 2 * 
        quat.z * quat.x * z - 
        quat.z * quat.z * x - quat.y * quat.y * x;

    result.y = 2 * quat.x * quat.y * x + 
        quat.y * quat.y * y + 2 * 
        quat.z * quat.y * z + 2 * 
        quat.w * quat.z * x - 
        quat.z * quat.z * y + 
        quat.w * quat.w * y - 2 * 
        quat.x * quat.w * z - 
        quat.x * quat.x * y;

    result.z = 2 * quat.x * quat.z * x + 2 * 
        quat.y * quat.z * y + 
        quat.z * quat.z * z - 2 * 
        quat.w * quat.y * x - 
        quat.y * quat.y * z + 2 * 
        quat.w * quat.x * y - 
        quat.x * quat.x * z + 
        quat.w * quat.w * z;
    return result;
}

Vector3 &Vector3::operator*=(const Quaternion &quat)
{
    return operator=(operator*(quat));
}

Vector3 Vector3::operator/(const Vector3 &other) const
{
    return Vector3(x / other.x, y / other.y, z / other.z);
}

Vector3 &Vector3::operator/=(const Vector3 &other)
{
    x /= other.x;
    y /= other.y;
    z /= other.z;
    return *this;
}

bool Vector3::operator==(const Vector3 &other) const
{
    return x == other.x && y == other.y && z == other.z;
}

bool Vector3::operator!=(const Vector3 &other) const
{
    return !operator==(other);
}

float Vector3::Max() const
{
    return MathUtil::Max(x, MathUtil::Max(y, z));
}

float Vector3::Min() const
{
    return MathUtil::Min(x, MathUtil::Min(y, z));
}

float Vector3::DistanceSquared(const Vector3 &other) const
{
    float dx = x - other.x;
    float dy = y - other.y;
    float dz = z - other.z;
    return dx * dx + dy * dy + dz * dz;
}

/* Euclidean distance */
float Vector3::Distance(const Vector3 &other) const
{
    return sqrt(DistanceSquared(other));
}

Vector3 Vector3::Normalized() const
{
    return *this / MathUtil::Max(Length(), MathUtil::epsilon<float>);
}

Vector3 &Vector3::Normalize()
{
    return *this /= MathUtil::Max(Length(), MathUtil::epsilon<float>);
}

Vector3 Vector3::Cross(const Vector3 &other) const
{
    return {
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
    };
}

Vector3 &Vector3::Rotate(const Vector3 &axis, float radians)
{
    return (*this) *= Matrix4::Rotation(axis, radians);
}

Vector3 &Vector3::Lerp(const Vector3 &to, const float amt)
{
    x = MathUtil::Lerp(x, to.x, amt);
    y = MathUtil::Lerp(y, to.y, amt);
    z = MathUtil::Lerp(z, to.z, amt);
    return *this;
}

float Vector3::Dot(const Vector3 &other) const
{
    return x * other.x + y * other.y + z * other.z;
}

Vector3 Vector3::Abs(const Vector3 &vec)
{
    return Vector3(std::fabs(vec.x), std::fabs(vec.y), std::fabs(vec.z));
}

Vector3 Vector3::Round(const Vector3 &vec)
{
    return Vector3(std::round(vec.x), std::round(vec.y), std::round(vec.z));
}

Vector3 Vector3::Clamp(const Vector3 &vec, float min_value, float max_value)
{
    return Max(min_value, Min(vec, max_value));
}

Vector3 Vector3::Min(const Vector3 &a, const Vector3 &b)
{
    return Vector3(
        MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z)
    );
}

Vector3 Vector3::Max(const Vector3 &a, const Vector3 &b)
{
    return Vector3(
        MathUtil::Max(a.x, b.x),
        MathUtil::Max(a.y, b.y),
        MathUtil::Max(a.z, b.z)
    );
}

Vector3 Vector3::Zero()
{
    return Vector3(0, 0, 0);
}

Vector3 Vector3::One()
{
    return Vector3(1, 1, 1);
}

Vector3 Vector3::UnitX()
{
    return Vector3(1, 0, 0);
}

Vector3 Vector3::UnitY()
{
    return Vector3(0, 1, 0);
}

Vector3 Vector3::UnitZ()
{
    return Vector3(0, 0, 1);
}

std::ostream &operator<<(std::ostream &out, const Vector3 &vec) // output
{
    out << "[" << vec.x << ", " << vec.y << ", " << vec.z << "]";
    return out;
}

} // namespace hyperion
