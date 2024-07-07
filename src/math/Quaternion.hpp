/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_QUATERNION_HPP
#define HYPERION_QUATERNION_HPP

#include <math/MathUtil.hpp>
#include <math/Vector3.hpp>

#include <HashCode.hpp>

namespace hyperion {

class Matrix4;

struct alignas(16) HYP_API Quaternion
{
    friend std::ostream &operator<<(std::ostream &out, const Quaternion &rot);

    float x, y, z, w;

    Quaternion();
    Quaternion(float x, float y, float z, float w);
    explicit Quaternion(const Matrix4 &mat);
    explicit Quaternion(const Vec3f &euler);
    Quaternion(const Vec3f &axis, float radians);
    Quaternion(const Quaternion &other);

    float GetX() const { return x; }
    float &GetX() { return x; }
    void SetX(float x) { this->x = x; }
    float GetY() const { return y; }
    float &GetY() { return y; }
    void SetY(float y) { this->y = y; }
    float GetZ() const { return z; }
    float &GetZ() { return z; }
    void SetZ(float z) { this->z = z; }
    float GetW() const { return w; }
    float &GetW() { return w; }
    void SetW(float w) { this->w = w; }

    Quaternion &operator=(const Quaternion &other);
    Quaternion operator*(const Quaternion &other) const;
    Quaternion &operator*=(const Quaternion &other);
    Quaternion &operator+=(const Vec3f &vec);
    Vec3f operator*(const Vector3 &vec) const;

    float Length() const;
    float LengthSquared() const;
    Quaternion &Normalize();

    Quaternion &Invert();
    Quaternion Inverse() const;

    Quaternion &Slerp(const Quaternion &to, float amt);

    int GimbalPole() const;
    float Roll() const;
    float Pitch() const;
    float Yaw() const;

    static Quaternion Identity();
    static Quaternion LookAt(const Vec3f &direction, const Vec3f &up);
    static Quaternion AxisAngles(const Vec3f &axis, float radians);

    HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(x);
        hc.Add(y);
        hc.Add(z);
        hc.Add(w);

        return hc;
    }
};
} // namespace hyperion

#endif
