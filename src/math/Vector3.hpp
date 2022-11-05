#ifndef VECTOR3_H
#define VECTOR3_H

#include <cmath>
#include <tuple>
#include <iostream>

#include "Vector4.hpp"
#include "../HashCode.hpp"
#include "../Util.hpp"
#include "intrinsics/Intrinsics.hpp"

#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion {

class Quaternion;
class Vector2;
class Matrix3;
class Matrix4;

class Vector3
{
    friend std::ostream &operator<<(std::ostream &out, const Vector3 &vec);
public:
    static constexpr UInt size = 3;

    static const Vector3 zero;
    static const Vector3 one;

    union {
        struct { float x, y, z; };
        float values[3];
    };

    Vector3();
    Vector3(float x, float y, float z);
    Vector3(float xyz);
    explicit Vector3(intrinsics::Float128 vec);
    explicit Vector3(const Vector2 &xy, float z);
    /*! \brief Convert a Vector4 to Vector3, discarding w component. */
    explicit Vector3(const Vector4 &vec);
    Vector3(const Vector3 &other);

    float GetX() const     { return x; }
    float &GetX()          { return x; }
    Vector3 &SetX(float x) { this->x = x; return *this; }
    float GetY() const     { return y; }
    float &GetY()          { return y; }
    Vector3 &SetY(float y) { this->y = y; return *this; }
    float GetZ() const     { return z; }
    float &GetZ()          { return z; }
    Vector3 &SetZ(float z) { this->z = z; return *this; }

    constexpr float operator[](UInt index) const
        { return values[index]; }

    constexpr float &operator[](UInt index)
        { return values[index]; }

    explicit operator bool() const
        { return Sum() != 0.0f; }

    Vector3 &operator=(const Vector3 &other);
    Vector3 operator+(const Vector3 &other) const;
    Vector3 &operator+=(const Vector3 &other);
    Vector3 operator-(const Vector3 &other) const;
    Vector3 &operator-=(const Vector3 &other);
    Vector3 operator*(const Vector3 &other) const;
    Vector3 &operator*=(const Vector3 &other);
    Vector3 operator*(const Matrix3 &mat) const;
    Vector3 &operator*=(const Matrix3 &mat);
    Vector3 operator*(const Matrix4 &mat) const;
    Vector3 &operator*=(const Matrix4 &mat);
    Vector3 operator*(const Quaternion &quat) const;
    Vector3 &operator*=(const Quaternion &quat);
    Vector3 operator/(const Vector3 &other) const;
    Vector3 &operator/=(const Vector3 &other);
    bool operator==(const Vector3 &other) const;
    bool operator!=(const Vector3 &other) const;
    Vector3 operator-() const { return operator*(-1.0f); }

    bool operator<(const Vector3 &other) const
        { return std::tie(x, y, z) < std::tie(other.x, other.y, other.z); }

    constexpr float LengthSquared() const { return x * x + y * y + z * z; }
    float Length() const { return std::sqrt(LengthSquared()); }

    constexpr float Avg() const { return (x + y + z) / 3.0f; }
    constexpr float Sum() const { return x + y + z; }
    float Max() const;
    float Min() const;

    float DistanceSquared(const Vector3 &other) const;
    float Distance(const Vector3 &other) const;
    
    Vector3 Normalized() const;
    Vector3 &Normalize();

    Vector3 Cross(const Vector3 &other) const;

    Vector3 &Rotate(const Vector3 &axis, float radians);
    Vector3 &Lerp(const Vector3 &to, const float amt);
    float Dot(const Vector3 &other) const;
    float AngleBetween(const Vector3 &other) const;

    Vector4 ToVector4(float w = 1.0f) const
        { return Vector4(*this, w); }

    static Vector3 Abs(const Vector3 &);
    static Vector3 Round(const Vector3 &);
    static Vector3 Clamp(const Vector3 &, float min, float max);
    static Vector3 Min(const Vector3 &a, const Vector3 &b);
    static Vector3 Max(const Vector3 &a, const Vector3 &b);

    static Vector3 Zero();
    static Vector3 One();
    static Vector3 UnitX();
    static Vector3 UnitY();
    static Vector3 UnitZ();

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(x);
        hc.Add(y);
        hc.Add(z);

        return hc;
    }
};

static_assert(sizeof(Vector3) == sizeof(float) * 3, "sizeof(Vector3) must be equal to sizeof(float) * 3");

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::Vector3);

#endif
