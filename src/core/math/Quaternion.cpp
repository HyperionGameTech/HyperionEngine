/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/math/Quaternion.hpp>
#include <core/math/Matrix4.hpp>

#include <float.h>

namespace hyperion {

Quaternion::Quaternion()
    : x(0.0), y(0.0), z(0.0), w(1.0)
{
}

Quaternion::Quaternion(float x, float y, float z, float w)
    : x(x), y(y), z(z), w(w)
{
}

Quaternion::Quaternion(const Matrix4 &m)
{
    Vec3f m0 = m[0].GetXYZ(),
        m1 = m[1].GetXYZ(),
        m2 = m[2].GetXYZ();

    float length_sqr = m0[0] * m0[0] + m1[0] * m1[0] + m2[0] * m2[0];

    if (length_sqr != 1.0f && length_sqr != 0.0f) {
        length_sqr = 1.0f / MathUtil::Sqrt(length_sqr);
        m0[0] *= length_sqr;
        m1[0] *= length_sqr;
        m2[0] *= length_sqr;
    }

    length_sqr = m0[1] * m0[1] + m1[1] * m1[1] + m2[1] * m2[1];

    if (length_sqr != 1.0f && length_sqr != 0.0f) {
        length_sqr = 1.0f / MathUtil::Sqrt(length_sqr);
        m0[1] *= length_sqr;
        m1[1] *= length_sqr;
        m2[1] *= length_sqr;
    }

    length_sqr = m0[2] * m0[2] + m1[2] * m1[2] + m2[2] * m2[2];

    if (length_sqr != 1.0f && length_sqr != 0.0f) {
        length_sqr = 1.0f / MathUtil::Sqrt(length_sqr);
        m0[2] *= length_sqr;
        m1[2] *= length_sqr;
        m2[2] *= length_sqr;
    }

    const float tr = m0[0] + m1[1] + m2[2];

    if (tr > 0.0f) { 
        float S = sqrt(tr + 1.0f) * 2.0f; // S=4*qw

        x = (m2[1] - m1[2]) / S;
        y = (m0[2] - m2[0]) / S; 
        z = (m1[0] - m0[1]) / S; 
        w = 0.25f * S;
    } else if ((m0[0] > m1[1]) && (m0[0] > m2[2])) { 
        float S = sqrt(1.0f + m0[0] - m1[1] - m2[2]) * 2.0f; // S=4*qx

        x = 0.25f * S;
        y = (m0[1] + m1[0]) / S; 
        z = (m0[2] + m2[0]) / S; 
        w = (m2[1] - m1[2]) / S;
    } else if (m1[1] > m2[2]) { 
        float S = sqrt(1.0f + m1[1] - m0[0] - m2[2]) * 2.0f; // S=4*qy

        x = (m0[1] + m1[0]) / S; 
        y = 0.25f * S;
        z = (m1[2] + m2[1]) / S; 
        w = (m0[2] - m2[0]) / S;
    } else { 
        float S = sqrt(1.0f + m2[2] - m0[0] - m1[1]) * 2.0f; // S=4*qz

        x = (m0[2] + m2[0]) / S;
        y = (m1[2] + m2[1]) / S;
        z = 0.25f * S;
        w = (m1[0] - m0[1]) / S;
    }
}

Quaternion::Quaternion(const Vec3f &euler)
{
    const float x_over2 = MathUtil::DegToRad(euler.x) * 0.5f;
    const float y_over2 = MathUtil::DegToRad(euler.y) * 0.5f;
    const float z_over2 = MathUtil::DegToRad(euler.z) * 0.5f;

    const float sin_x_over2 = MathUtil::Sin(x_over2);
    const float cos_x_over2 = MathUtil::Cos(x_over2);
    const float sin_y_over2 = MathUtil::Sin(y_over2);
    const float cos_y_over2 = MathUtil::Cos(y_over2);
    const float sin_z_over2 = MathUtil::Sin(z_over2);
    const float cos_z_over2 = MathUtil::Cos(z_over2);
    
    x = cos_y_over2 * sin_x_over2 * cos_z_over2 + sin_y_over2 * cos_x_over2 * sin_z_over2;
    y = sin_y_over2 * cos_x_over2 * cos_z_over2 - cos_y_over2 * sin_x_over2 * sin_z_over2;
    z = cos_y_over2 * cos_x_over2 * sin_z_over2 - sin_y_over2 * sin_x_over2 * cos_z_over2;
    w = cos_y_over2 * cos_x_over2 * cos_z_over2 + sin_y_over2 * sin_x_over2 * sin_z_over2;
}

Quaternion::Quaternion(const Vec3f &axis, float radians)
{
    Vec3f tmp(axis);

    if (tmp.Length() != 1) {
        tmp.Normalize();
    }

    if (tmp != Vec3f::Zero()) {
        float half_angle = radians * 0.5f;
        float sin_half_angle = sin(half_angle);

        x = sin_half_angle * tmp.x;
        y = sin_half_angle * tmp.y;
        z = sin_half_angle * tmp.z;
        w = cos(half_angle);
    } else {
        (*this) = Quaternion::Identity();
    }
}

Quaternion Quaternion::operator*(const Quaternion &other) const
{
    float x1 = x * other.w + y * other.z - z * other.y + w * other.x;
    float y1 = -x * other.z + y * other.w + z * other.x + w * other.y;
    float z1 = x * other.y - y * other.x + z * other.w + w * other.z;
    float w1 = -x * other.x - y * other.y - z * other.z + w * other.w;
    return Quaternion(x1, y1, z1, w1);
}

Quaternion &Quaternion::operator*=(const Quaternion &other)
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

Quaternion &Quaternion::operator+=(const Vec3f &vec)
{
    Quaternion q(vec.x, vec.y, vec.z, 0.0);
    q *= *this;
    x += q.x * 0.5f;
    y += q.y * 0.5f;
    z += q.z * 0.5f;
    w += q.w * 0.5f;
    return *this;
}

Vec3f Quaternion::operator*(const Vec3f &vec) const
{
    Vec3f result;
    result.x = w * w * vec.x + 2 * y * w * vec.z - 2 * z * w * vec.y + x * x * vec.x
        + 2 * y * x * vec.y + 2 * z * x * vec.z - z * z * vec.x - y * y * vec.x;
    result.y = 2 * x * y * vec.x + y * y * vec.y + 2 * z * y * vec.z + 2 * w * z
        * vec.x - z * z * vec.y + w * w * vec.y - 2 * x * w * vec.z - x * x
        * vec.y;

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

Quaternion &Quaternion::Normalize()
{
    float d = LengthSquared();
    if (d < FLT_EPSILON) {
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

Quaternion &Quaternion::Invert()
{
    float len2 = LengthSquared();
    if (len2 > 0.0) {
        float inv_len2 = 1.0f / len2;
        w = w * inv_len2;
        x = -x * inv_len2;
        y = -y * inv_len2;
        z = -z * inv_len2;
    }
    return *this;
}

Quaternion Quaternion::Inverse() const
{
    return Quaternion(*this).Invert();
}

Quaternion &Quaternion::Slerp(const Quaternion &to, float amt)
{
    float cos_half_theta = w * to.w + x * to.x + y * to.y + z * to.z;

    if (abs(cos_half_theta) >= 1.0f) {
        return *this;
    }

    float half_theta = acos(cos_half_theta);
    float sin_half_theta = sqrt(1.0f - cos_half_theta * cos_half_theta);

    if (abs(sin_half_theta) < 0.001f) {
        w = w * 0.5f + to.w * 0.5f;
        x = x * 0.5f + to.x * 0.5f;
        y = y * 0.5f + to.y * 0.5f;
        z = z * 0.5f + to.z * 0.5f;
        return *this;
    }

    float ratio_a = sin((1.0f - amt) * half_theta) / sin_half_theta;
    float ratio_b = sin(amt * half_theta) / sin_half_theta;

    w = w * ratio_a + to.w * ratio_b;
    x = x * ratio_a + to.x * ratio_b;
    y = y * ratio_a + to.y * ratio_b;
    z = z * ratio_a + to.z * ratio_b;
    return *this;
}

int Quaternion::GimbalPole() const
{
    const float amt = y * x + z * w;

    return amt > 0.499f ? 1 : (amt < -0.499f ? -1 : 0);
}

float Quaternion::Roll() const
{
    const int pole = GimbalPole();

    return pole == 0
        ? atan2(2.0f * (w * z + y * x), 1.0f - 2.0f * (x * x + z * z))
        : float(pole) * 2.0f * atan2(y, w);
}

float Quaternion::Pitch() const
{
    const int pole = GimbalPole();

    return pole == 0
        ? MathUtil::Arcsin(MathUtil::Clamp(2.0f * (w * x - z * y), -1.0f, 1.0f))
        : float(pole) * MathUtil::pi<float> * 0.5f;
}

float Quaternion::Yaw() const
{
    int pole = GimbalPole();
    return pole == 0
        ? atan2(2.0f * (y * w + x * z), 1.0f - 2.0f * (y * y + x * x))
        : 0.0f;
}

Quaternion Quaternion::Identity()
{
    return Quaternion(0.0, 0.0, 0.0, 1.0);
}

Quaternion Quaternion::LookAt(const Vec3f &direction, const Vec3f &up)
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

    return Quaternion(Matrix4(rows));
}

Quaternion Quaternion::AxisAngles(const Vec3f &axis, float radians)
{
    return Quaternion(axis, radians);
}

std::ostream &operator<<(std::ostream &out, const Quaternion &rot) // output
{
    out << "[" << rot.x << ", " << rot.y << ", " << rot.z << ", " << rot.w << "]";
    return out;
}
} // namespace hyperion
