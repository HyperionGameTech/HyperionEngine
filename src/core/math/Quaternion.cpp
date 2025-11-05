/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/Quaternion.hpp>
#include <core/math/Mat4f.hpp>

#include <float.h>

#ifndef HYP_BUILDTOOL
#include <Quaternion.generated.inl>
#endif

namespace hyperion {

Quaternion::Quaternion()
    : x(0.0),
      y(0.0),
      z(0.0),
      w(1.0)
{
}

Quaternion::Quaternion(float x, float y, float z, float w)
    : x(x),
      y(y),
      z(z),
      w(w)
{
}

Quaternion::Quaternion(const Mat4f& m)
{
    Vec3f m0 = m[0].GetXYZ(),
          m1 = m[1].GetXYZ(),
          m2 = m[2].GetXYZ();

    float lengthSqr = m0[0] * m0[0] + m1[0] * m1[0] + m2[0] * m2[0];

    if (lengthSqr != 1.0f && lengthSqr != 0.0f)
    {
        lengthSqr = 1.0f / MathUtil::Sqrt(lengthSqr);
        m0[0] *= lengthSqr;
        m1[0] *= lengthSqr;
        m2[0] *= lengthSqr;
    }

    lengthSqr = m0[1] * m0[1] + m1[1] * m1[1] + m2[1] * m2[1];

    if (lengthSqr != 1.0f && lengthSqr != 0.0f)
    {
        lengthSqr = 1.0f / MathUtil::Sqrt(lengthSqr);
        m0[1] *= lengthSqr;
        m1[1] *= lengthSqr;
        m2[1] *= lengthSqr;
    }

    lengthSqr = m0[2] * m0[2] + m1[2] * m1[2] + m2[2] * m2[2];

    if (lengthSqr != 1.0f && lengthSqr != 0.0f)
    {
        lengthSqr = 1.0f / MathUtil::Sqrt(lengthSqr);
        m0[2] *= lengthSqr;
        m1[2] *= lengthSqr;
        m2[2] *= lengthSqr;
    }

    const float tr = m0[0] + m1[1] + m2[2];

    if (tr > 0.0f)
    {
        float s = sqrt(tr + 1.0f) * 2.0f; // S=4*qw

        x = (m2[1] - m1[2]) / s;
        y = (m0[2] - m2[0]) / s;
        z = (m1[0] - m0[1]) / s;
        w = 0.25f * s;
    }
    else if ((m0[0] > m1[1]) && (m0[0] > m2[2]))
    {
        float s = sqrt(1.0f + m0[0] - m1[1] - m2[2]) * 2.0f; // S=4*qx

        x = 0.25f * s;
        y = (m0[1] + m1[0]) / s;
        z = (m0[2] + m2[0]) / s;
        w = (m2[1] - m1[2]) / s;
    }
    else if (m1[1] > m2[2])
    {
        float s = sqrt(1.0f + m1[1] - m0[0] - m2[2]) * 2.0f; // S=4*qy

        x = (m0[1] + m1[0]) / s;
        y = 0.25f * s;
        z = (m1[2] + m2[1]) / s;
        w = (m0[2] - m2[0]) / s;
    }
    else
    {
        float s = sqrt(1.0f + m2[2] - m0[0] - m1[1]) * 2.0f; // S=4*qz

        x = (m0[2] + m2[0]) / s;
        y = (m1[2] + m2[1]) / s;
        z = 0.25f * s;
        w = (m1[0] - m0[1]) / s;
    }
}

Quaternion::Quaternion(const Vec3f& euler)
{
    const float xOver2 = euler.x * 0.5f; // roll
    const float yOver2 = euler.y * 0.5f; // pitch
    const float zOver2 = euler.z * 0.5f; // yaw

    const float sx = MathUtil::Sin(xOver2), cx = MathUtil::Cos(xOver2);
    const float sy = MathUtil::Sin(yOver2), cy = MathUtil::Cos(yOver2);
    const float sz = MathUtil::Sin(zOver2), cz = MathUtil::Cos(zOver2);

    x = cy * sx * cz - sy * cx * sz;
    y = sy * cx * cz + cy * sx * sz;
    z = cy * cx * sz - sy * sx * cz;
    w = cy * cx * cz + sy * sx * sz;
}

Quaternion::Quaternion(const Vec3f& axis, float radians)
{
    Vec3f tmp(axis);

    if (tmp.Length() != 1)
    {
        tmp.Normalize();
    }

    if (tmp != Vec3f::Zero())
    {
        float halfAngle = radians * 0.5f;
        float sinHalfAngle = sin(halfAngle);

        x = sinHalfAngle * tmp.x;
        y = sinHalfAngle * tmp.y;
        z = sinHalfAngle * tmp.z;
        w = cos(halfAngle);
    }
    else
    {
        (*this) = Quaternion::Identity();
    }
}

Quaternion Quaternion::operator*(const Quaternion& other) const
{
    float x1 = x * other.w + y * other.z - z * other.y + w * other.x;
    float y1 = -x * other.z + y * other.w + z * other.x + w * other.y;
    float z1 = x * other.y - y * other.x + z * other.w + w * other.z;
    float w1 = -x * other.x - y * other.y - z * other.z + w * other.w;
    return Quaternion(x1, y1, z1, w1);
}

Quaternion& Quaternion::operator*=(const Quaternion& other)
{
    float x1 = x * other.w + y * other.z - z * other.y + w * other.x;
    float y1 = -x * other.z + y * other.w + z * other.x + w * other.y;
    float z1 = x * other.y - y * other.x + z * other.w + w * other.z;
    float w1 = -x * other.x - y * other.y - z * other.z + w * other.w;
    x = x1;
    y = y1;
    z = z1;
    w = w1;
    return *this;
}

Quaternion& Quaternion::operator+=(const Vec3f& vec)
{
    Quaternion q(vec.x, vec.y, vec.z, 0.0);
    q *= *this;
    x += q.x * 0.5f;
    y += q.y * 0.5f;
    z += q.z * 0.5f;
    w += q.w * 0.5f;
    return *this;
}

Vec3f Quaternion::operator*(const Vec3f& vec) const
{
    Vec3f result;
    result.x = w * w * vec.x + 2 * y * w * vec.z - 2 * z * w * vec.y + x * x * vec.x
        + 2 * y * x * vec.y + 2 * z * x * vec.z - z * z * vec.x - y * y * vec.x;
    result.y = 2 * x * y * vec.x + y * y * vec.y + 2 * z * y * vec.z + 2 * w * z * vec.x - z * z * vec.y + w * w * vec.y - 2 * x * w * vec.z - x * x * vec.y;

    result.z = 2 * x * z * vec.x + 2 * y * z * vec.y + z * z * vec.z - 2 * w * y * vec.x
        - y * y * vec.z + 2 * w * x * vec.y - x * x * vec.z + w * w * vec.z;
    return result;
}

float Quaternion::Length() const
{
    return sqrt(LengthSquared());
}

float Quaternion::LengthSquared() const
{
    return w * w + x * x + y * y + z * z;
}

Quaternion& Quaternion::Normalize()
{
    float d = LengthSquared();
    if (d < FLT_EPSILON)
    {
        w = 1.0;
        return *this;
    }
    d = 1.0f / sqrt(d);
    w *= d;
    x *= d;
    y *= d;
    z *= d;

    return *this;
}

Quaternion& Quaternion::Invert()
{
    float len2 = LengthSquared();
    if (len2 > 0.0)
    {
        float invLen2 = 1.0f / len2;
        w = w * invLen2;
        x = -x * invLen2;
        y = -y * invLen2;
        z = -z * invLen2;
    }
    return *this;
}

Quaternion Quaternion::Inverse() const
{
    return Quaternion(*this).Invert();
}

Quaternion& Quaternion::Slerp(const Quaternion& to, float amt)
{
    float cosHalfTheta = w * to.w + x * to.x + y * to.y + z * to.z;

    if (abs(cosHalfTheta) >= 1.0f)
    {
        return *this;
    }

    float halfTheta = acos(cosHalfTheta);
    float sinHalfTheta = sqrt(1.0f - cosHalfTheta * cosHalfTheta);

    if (abs(sinHalfTheta) < 0.001f)
    {
        w = w * 0.5f + to.w * 0.5f;
        x = x * 0.5f + to.x * 0.5f;
        y = y * 0.5f + to.y * 0.5f;
        z = z * 0.5f + to.z * 0.5f;
        return *this;
    }

    float ratioA = sin((1.0f - amt) * halfTheta) / sinHalfTheta;
    float ratioB = sin(amt * halfTheta) / sinHalfTheta;

    w = w * ratioA + to.w * ratioB;
    x = x * ratioA + to.x * ratioB;
    y = y * ratioA + to.y * ratioB;
    z = z * ratioA + to.z * ratioB;
    return *this;
}

int Quaternion::GimbalPole() const
{
    const float qx = x, qy = y, qz = z, qw = w;
    const float t = qx * qy + qz * qw;
    return t > 0.499f ? 1 : (t < -0.499f ? -1 : 0);
}

float Quaternion::Roll() const
{
    const int pole = GimbalPole();
    if (pole == 0)
    {
        const float qx = x, qy = y, qz = z, qw = w;
        return std::atan2(2.0f * (qw * qx + qy * qz), 1.0f - 2.0f * (qx * qx + qy * qy));
    }
    return 0.0f;
}

float Quaternion::Pitch() const
{
    const int pole = GimbalPole();
    if (pole == 0)
    {
        const float qx = x, qy = y, qz = z, qw = w;
        float s = 2.0f * (qw * qy - qz * qx);
        if (s > 1.0f)
            s = 1.0f;
        if (s < -1.0f)
            s = -1.0f;
        return std::asin(s);
    }
    return float(pole) * MathUtil::pi<float> * 0.5f;
}

float Quaternion::Yaw() const
{
    const int pole = GimbalPole();
    if (pole == 0)
    {
        const float qx = x, qy = y, qz = z, qw = w;
        return std::atan2(2.0f * (qw * qz + qx * qy), 1.0f - 2.0f * (qy * qy + qz * qz));
    }
    return float(pole) * 2.0f * std::atan2(x, w);
}

Quaternion Quaternion::Identity()
{
    return Quaternion(0.0, 0.0, 0.0, 1.0);
}

Quaternion Quaternion::LookAt(const Vec3f& direction, const Vec3f& up)
{
    const Vec3f z = direction.Normalized();
    const Vec3f x = up.Cross(direction).Normalized();
    const Vec3f y = direction.Cross(x).Normalized();

    Vec4f rows[] = {
        Vec4f(x, 0.0f),
        Vec4f(y, 0.0f),
        Vec4f(z, 0.0f),
        Vec4f::UnitW()
    };

    return Quaternion(Mat4f(rows));
}

Quaternion Quaternion::AxisAngles(const Vec3f& axis, float radians)
{
    return Quaternion(axis, radians);
}

} // namespace hyperion
