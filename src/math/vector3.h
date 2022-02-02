#ifndef VECTOR3_H
#define VECTOR3_H

#include <cmath>
#include <iostream>

#include "matrix3.h"
#include "matrix4.h"
#include "../hash_code.h"
#include "../util.h"

namespace hyperion {

class Quaternion;

class Vector3 {
    friend std::ostream &operator<<(std::ostream &out, const Vector3 &vec);
public:
    float x, y, z;

    Vector3();
    Vector3(float x, float y, float z);
    Vector3(float xyz);
    Vector3(const Vector3 &other);

    inline float GetX() const { return x; }
    inline float &GetX() { return x; }
    inline Vector3 &SetX(float x) { this->x = x; return *this; }
    inline float GetY() const { return y; }
    inline float &GetY() { return y; }
    inline Vector3 &SetY(float y) { this->y = y; return *this; }
    inline float GetZ() const { return z; }
    inline float &GetZ() { return z; }
    inline Vector3 &SetZ(float z) { this->z = z; return *this; }


    constexpr inline float operator[](size_t index) const
    {
        if (index == 0) return x;
        if (index == 1) return y;
        if (index == 2) return z;

        ex_assert_msg(false, "Index out of bounds");
    }

    constexpr inline float &operator[](size_t index)
    {
        if (index == 0) return x;
        if (index == 1) return y;
        if (index == 2) return z;

        ex_assert_msg(false, "Index out of bounds");
    }

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
    inline Vector3 operator-() const { return operator*(-1.0f); }

    float Length() const;
    float LengthSquared() const;

    float DistanceSquared(const Vector3 &other) const;
    float Distance(const Vector3 &other) const;

    Vector3 &Normalize();
    Vector3 &Cross(const Vector3 &other);
    Vector3 &Rotate(const Vector3 &axis, float radians);
    Vector3 &Lerp(const Vector3 &to, const float amt);
    float Dot(const Vector3 &other) const;

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

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(x);
        hc.Add(y);
        hc.Add(z);

        return hc;
    }
};

} // namespace hyperion

#endif
