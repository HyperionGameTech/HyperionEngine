#include "Intrinsics.hpp"
#include "../Vector3.hpp"
#include "../Quaternion.hpp"
#include "../Matrix3.hpp"
#include "../Matrix4.hpp"

#include <cmath>

#if defined(HYP_FEATURES_INTRINSICS)

namespace hyperion {

using namespace intrinsics;

const Vector3 Vector3::zero = Vector3(0.0f);
const Vector3 Vector3::one = Vector3(1.0f);

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
}

Vector3::Vector3(const Vector3 &other)
    : x(other.x), 
      y(other.y), 
      z(other.z)
{
}

Vector3::Vector3(Float128 vec) {
    Float128StoreVector3(values, vec);
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
    Float128 a = Float128Set(x, y, z, 0.0f);
    const Float128 b = Float128Set(other.x, other.y, other.z, 0.0f);
    a = Float128Add(a, b);
    return Vector3(a);
}

Vector3 &Vector3::operator+=(const Vector3 &other)
{
    Float128 a = Float128Set(x, y, z, 0.0f);
    const Float128 b = Float128Set(other.x, other.y, other.z, 0.0f);
    a = Float128Add(a, b);
    Float128StoreVector3(values, a);
    return *this;
}

Vector3 Vector3::operator-(const Vector3 &other) const
{
    Float128 a = Float128Set(x, y, z, 0.0f);
    const Float128 b = Float128Set(other.x, other.y, other.z, 0.0f);
    a = Float128Sub(a, b);
    return Vector3(a);
}

Vector3 &Vector3::operator-=(const Vector3 &other)
{
    Float128 a = Float128Set(x, y, z, 0.0f);
    const Float128 b = Float128Set(other.x, other.y, other.z, 0.0f);
    a = Float128Sub(a, b);
    Float128StoreVector3(values, a);
    return *this;
}

Vector3 Vector3::operator*(const Vector3 &other) const
{
    Float128 a = Float128Set(x, y, z, 0.0f);
    const Float128 b = Float128Set(other.x, other.y, other.z, 0.0f);
    a = Float128Mul(a, b);
    return Vector3(a);
}

Vector3 &Vector3::operator*=(const Vector3 &other)
{
    Float128 a = Float128Set(x, y, z, 0.0f);
    const Float128 b = Float128Set(other.x, other.y, other.z, 0.0f);
    a = Float128Mul(a, b);
    Float128StoreVector3(values, a);
    return *this;
}

Vector3 Vector3::operator*(const Matrix3 &mat) const
{
    Float vec[] = { 0.0f, 0.0f, 0.0f };
    const Float128 a = Float128Set(x, y, z, 0.0f);

    for (int i = 0; i < 3; i++) {
        // Load each line of the matrix
        const Float128 b = Float128Set(mat.values[i], mat.values[i + 3], mat.values[i + 6], 0.0f);
        // Multiply our vector by matrix row
        const Float128 v = Float128Mul(a, b);
        // Set our result component to the sum of the multiplication
        vec[i] = Float128Sum(v);
    }

    return { vec[0], vec[1], vec[2] };
}

Vector3 &Vector3::operator*=(const Matrix3 &mat)
{
    return operator=(operator*(mat));
}

Vector3 Vector3::operator*(const Matrix4 &mat) const
{
    Vector4 product;
    const Float128 a = Float128Set(x, y, z, 1.0f);
    for (int i = 0; i < 4; i++) {
        const Float128 b = Float128Set(mat.values[i], mat.values[i + 4], mat.values[i + 8], mat.values[i + 12]);
        const Float128 row = Float128Mul(a, b);
        product.values[i] = Float128Sum(row);
    }

    return Vector3(product / product.w);
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
    return MathUtil::ApproxEqual(x, other.x)
        && MathUtil::ApproxEqual(y, other.y)
        && MathUtil::ApproxEqual(z, other.z);

    // return x == other.x && y == other.y && z == other.z;
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
    return MathUtil::Sqrt(DistanceSquared(other));
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
    return (*this) = Matrix4::Rotation(axis, radians) * (*this);
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

float Vector3::AngleBetween(const Vector3 &other) const
{
    const float dot_product = x * other.x + y * other.y + z * other.z;
    const float arc_cos = MathUtil::Arccos(dot_product);

    return arc_cos / (Length() * other.Length());
}

Vector3 Vector3::Abs(const Vector3 &vec)
{
    return Vector3(MathUtil::Abs(vec.x), MathUtil::Abs(vec.y), MathUtil::Abs(vec.z));
}

Vector3 Vector3::Round(const Vector3 &vec)
{
    return Vector3(MathUtil::Round(vec.x), MathUtil::Round(vec.y), MathUtil::Round(vec.z));
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
#endif