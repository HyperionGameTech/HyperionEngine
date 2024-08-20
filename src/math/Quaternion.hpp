/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_QUATERNION_HPP
#define HYPERION_QUATERNION_HPP

#include <math/MathUtil.hpp>
#include <math/Vector3.hpp>

#include <HashCode.hpp>

namespace hyperion {

class Matrix4;

HYP_STRUCT()
struct alignas(16) HYP_API Quaternion
{
    HYP_PROPERTY(X, &Quaternion::x)
    HYP_PROPERTY(Y, &Quaternion::y)
    HYP_PROPERTY(Z, &Quaternion::z)
    HYP_PROPERTY(W, &Quaternion::w)

    friend std::ostream &operator<<(std::ostream &out, const Quaternion &rot);

    HYP_FIELD()
    float x;

    HYP_FIELD()
    float y;

    HYP_FIELD()
    float z;

    HYP_FIELD()
    float w;

    Quaternion();
    Quaternion(float x, float y, float z, float w);
    explicit Quaternion(const Matrix4 &mat);
    explicit Quaternion(const Vec3f &euler);
    Quaternion(const Vec3f &axis, float radians);
    Quaternion(const Quaternion &other);

    HYP_FORCE_INLINE float GetX() const
        { return x; }

    HYP_FORCE_INLINE void SetX(float x)
        { this->x = x; }

    HYP_FORCE_INLINE float GetY() const
        { return y; }

    HYP_FORCE_INLINE void SetY(float y)
        { this->y = y; }

    HYP_FORCE_INLINE float GetZ() const
        { return z; }

    HYP_FORCE_INLINE void SetZ(float z)
        { this->z = z; }

    HYP_FORCE_INLINE float GetW() const
        { return w; }

    HYP_FORCE_INLINE void SetW(float w)
        { this->w = w; }

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
