/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/math/Quat4f.hpp>
#include <Core/math/Mat4f.hpp>

#include <float.h>

#ifndef HYP_TOOL
#include <Quat4f.generated.inl>
#endif

#if !HYP_ARM && (defined(__SSE4_1__) || (HYP_MSVC && defined(_M_X64)))
#include <immintrin.h>
#define HYP_QUATERNION_USE_SSE 1
#else
#define HYP_QUATERNION_USE_SSE 0
#endif

namespace {

#if HYP_QUATERNION_USE_SSE
HYP_FORCE_INLINE __m128 LoadQuatf(const Hyperion::Quat4f& q)
{
    return _mm_setr_ps(q.x, q.y, q.z, q.w);
}

HYP_FORCE_INLINE Hyperion::Quat4f StoreQuatf(__m128 value)
{
    alignas(16) float data[4];
    _mm_store_ps(data, value);
    return Hyperion::Quat4f(data[0], data[1], data[2], data[3]);
}
#endif

} // namespace

namespace Hyperion {

Quat4f::Quat4f()
    : x(0.0),
      y(0.0),
      z(0.0),
      w(1.0)
{
}

Quat4f::Quat4f(float x, float y, float z, float w)
    : x(x),
      y(y),
      z(z),
      w(w)
{
}

Quat4f::Quat4f(const Mat4f& m)
{
    Vec3f m0 = Vec3f(m[0][0], m[0][1], m[0][2]),
        m1 = Vec3f(m[1][0], m[1][1], m[1][2]),
        m2 = Vec3f(m[2][0], m[2][1], m[2][2]);

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

Quat4f::Quat4f(const Vec3f& euler)
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

Quat4f::Quat4f(const Vec3f& axis, float radians)
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
        (*this) = Quat4f::Identity();
    }
}

Quat4f Quat4f::operator*(const Quat4f& other) const
{
    float x1 = x * other.w + y * other.z - z * other.y + w * other.x;
    float y1 = -x * other.z + y * other.w + z * other.x + w * other.y;
    float z1 = x * other.y - y * other.x + z * other.w + w * other.z;
    float w1 = -x * other.x - y * other.y - z * other.z + w * other.w;
    return Quat4f(x1, y1, z1, w1);
}

Quat4f& Quat4f::operator*=(const Quat4f& other)
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

Quat4f& Quat4f::operator+=(const Vec3f& vec)
{
    Quat4f q(vec.x, vec.y, vec.z, 0.0);
    q *= *this;
    x += q.x * 0.5f;
    y += q.y * 0.5f;
    z += q.z * 0.5f;
    w += q.w * 0.5f;
    return *this;
}

Vec3f Quat4f::operator*(const Vec3f& vec) const
{
    Vec3f result;
    result.x = w * w * vec.x + 2 * y * w * vec.z - 2 * z * w * vec.y + x * x * vec.x
        + 2 * y * x * vec.y + 2 * z * x * vec.z - z * z * vec.x - y * y * vec.x;
    result.y = 2 * x * y * vec.x + y * y * vec.y + 2 * z * y * vec.z + 2 * w * z * vec.x - z * z * vec.y + w * w * vec.y - 2 * x * w * vec.z - x * x * vec.y;

    result.z = 2 * x * z * vec.x + 2 * y * z * vec.y + z * z * vec.z - 2 * w * y * vec.x
        - y * y * vec.z + 2 * w * x * vec.y - x * x * vec.z + w * w * vec.z;
    return result;
}

float Quat4f::Length() const
{
    return sqrt(LengthSquared());
}

float Quat4f::LengthSquared() const
{
#if HYP_QUATERNION_USE_SSE
    // Adapted from Foxtrot SIMD quaternion paths:
    // Math/Impl/Quat4f/FxQuat_AVX.inl
    const __m128 v = LoadQuatf(*this);
    return _mm_cvtss_f32(_mm_dp_ps(v, v, 0xFF));
#else
    return w * w + x * x + y * y + z * z;
#endif
}

Quat4f& Quat4f::Normalize()
{
#if HYP_QUATERNION_USE_SSE
    // Adapted from Foxtrot SIMD quaternion paths:
    // Math/Impl/Quat4f/FxQuat_AVX.inl NLerpIP (normalization step)
    const __m128 v = LoadQuatf(*this);
    const float d = _mm_cvtss_f32(_mm_dp_ps(v, v, 0xFF));
    if (d < FLT_EPSILON)
    {
        w = 1.0f;
        return *this;
    }
    *this = StoreQuatf(_mm_mul_ps(v, _mm_set1_ps(1.0f / sqrtf(d))));
    return *this;
#else
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
#endif
}

Quat4f Quat4f::Inverse() const
{
    Quat4f q(*this);
    float len2 = LengthSquared();
    if (len2 > 0.0)
    {
        float invLen2 = 1.0f / len2;
        q.w = w * invLen2;
        q.x = -x * invLen2;
        q.y = -y * invLen2;
        q.z = -z * invLen2;
    }
    return q;
}

Quat4f& Quat4f::Slerp(const Quat4f& to, float amt)
{
#if HYP_QUATERNION_USE_SSE
    // Adapted from Foxtrot SIMD quaternion paths:
    // Math/Impl/Quat4f/FxQuat_AVX.inl SLerp
    // Note: FMA (_mm_fmadd_ps) replaced with mul+add to require only SSE4.1.
    __m128 a_v = LoadQuatf(*this);
    __m128 b_v = LoadQuatf(to);

    const float cosHalfTheta = _mm_cvtss_f32(_mm_dp_ps(a_v, b_v, 0xFF));

    if (abs(cosHalfTheta) >= 1.0f)
    {
        return *this;
    }

    const float halfTheta = acos(cosHalfTheta);
    const float sinHalfTheta = sqrt(1.0f - cosHalfTheta * cosHalfTheta);

    if (abs(sinHalfTheta) < 0.001f)
    {
        *this = StoreQuatf(_mm_add_ps(_mm_mul_ps(a_v, _mm_set1_ps(0.5f)), _mm_mul_ps(b_v, _mm_set1_ps(0.5f))));
        return *this;
    }

    const float sht_recip = 1.0f / sinHalfTheta;
    const float ratioA = sin((1.0f - amt) * halfTheta) * sht_recip;
    const float ratioB = sin(amt * halfTheta) * sht_recip;

    *this = StoreQuatf(_mm_add_ps(_mm_mul_ps(a_v, _mm_set1_ps(ratioA)), _mm_mul_ps(b_v, _mm_set1_ps(ratioB))));
    return *this;
#else
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
#endif
}

int Quat4f::GimbalPole() const
{
    const float qx = x, qy = y, qz = z, qw = w;
    const float t = qx * qy + qz * qw;
    return t > 0.499f ? 1 : (t < -0.499f ? -1 : 0);
}

float Quat4f::Roll() const
{
    const int pole = GimbalPole();
    if (pole == 0)
    {
        const float qx = x, qy = y, qz = z, qw = w;
        return std::atan2(2.0f * (qw * qx + qy * qz), 1.0f - 2.0f * (qx * qx + qy * qy));
    }
    return 0.0f;
}

float Quat4f::Pitch() const
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

float Quat4f::Yaw() const
{
    const int pole = GimbalPole();
    if (pole == 0)
    {
        const float qx = x, qy = y, qz = z, qw = w;
        return std::atan2(2.0f * (qw * qz + qx * qy), 1.0f - 2.0f * (qy * qy + qz * qz));
    }
    return float(pole) * 2.0f * std::atan2(x, w);
}

Quat4f Quat4f::Identity()
{
    return Quat4f(0.0, 0.0, 0.0, 1.0);
}

Quat4f Quat4f::LookAt(const Vec3f& direction, const Vec3f& up)
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

    return Quat4f(Mat4f(rows));
}

Quat4f Quat4f::AxisAngles(const Vec3f& axis, float radians)
{
    return Quat4f(axis, radians);
}

} // namespace Hyperion
