/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/math/Vector3.hpp>
#include <Core/math/Quaternion.hpp>
#include <Core/math/Mat3f.hpp>
#include <Core/math/Mat4f.hpp>

#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#include <cmath>

#if !HYP_ARM && (defined(__SSE2__) || (HYP_MSVC && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))))
#include <immintrin.h>
#define HYP_VECTOR3_USE_SSE 0//1
#else
#define HYP_VECTOR3_USE_SSE 0
#endif

namespace {

#if HYP_VECTOR3_USE_SSE
HYP_FORCE_INLINE __m128 LoadVec3f(const Hyperion::math::Vec3<float>& value)
{
    return _mm_setr_ps(value.x, value.y, value.z, 0.0f);
}

HYP_FORCE_INLINE Hyperion::math::Vec3<float> StoreVec3f(__m128 value)
{
    alignas(16) float data[4];
    _mm_store_ps(data, value);

    return { data[0], data[1], data[2] };
}
#endif

} // namespace

namespace Hyperion {

HYP_API const Class* g_clsVec3f = nullptr;
HYP_API const Class* g_clsVec3i = nullptr;
HYP_API const Class* g_clsVec3u = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(Vec3f, -1, 0, {})
    Field(NAME(HYP_STR(x)), &Type::x, HYP_OFFSET_OF(Type, x)),
    Field(NAME(HYP_STR(y)), &Type::y, HYP_OFFSET_OF(Type, y)),
    Field(NAME(HYP_STR(z)), &Type::z, HYP_OFFSET_OF(Type, z))
HYP_END_STRUCT

HYP_REGISTER_STATIC_CLASS(Vec3f);

HYP_BEGIN_STRUCT(Vec3i, -1, 0, {})
    Field(NAME(HYP_STR(x)), &Type::x, HYP_OFFSET_OF(Type, x)),
    Field(NAME(HYP_STR(y)), &Type::y, HYP_OFFSET_OF(Type, y)),
    Field(NAME(HYP_STR(z)), &Type::z, HYP_OFFSET_OF(Type, z))
HYP_END_STRUCT

HYP_REGISTER_STATIC_CLASS(Vec3i);

HYP_BEGIN_STRUCT(Vec3u, -1, 0, {})
    Field(NAME(HYP_STR(x)), &Type::x, HYP_OFFSET_OF(Type, x)),
    Field(NAME(HYP_STR(y)), &Type::y, HYP_OFFSET_OF(Type, y)),
    Field(NAME(HYP_STR(z)), &Type::z, HYP_OFFSET_OF(Type, z))
HYP_END_STRUCT

HYP_REGISTER_STATIC_CLASS(Vec3u);

// clang-format on

Vec3<float> math::Vec3<float>::operator*(const Mat3f& mat) const
{
    return {
        x * mat.rows[0][0] + y * mat.rows[1][0] + z * mat.rows[2][0],
        x * mat.rows[0][1] + y * mat.rows[1][1] + z * mat.rows[2][1],
        x * mat.rows[0][2] + y * mat.rows[1][2] + z * mat.rows[2][2]
    };
}

Vec3<float>& math::Vec3<float>::operator*=(const Mat3f& mat)
{
    return operator=(operator*(mat));
}

Vec3<float> math::Vec3<float>::operator*(const Mat4f& mat) const
{
    Vector4 product {
        x * mat.values[0] + y * mat.values[4] + z * mat.values[8] + mat.values[12],
        x * mat.values[1] + y * mat.values[5] + z * mat.values[9] + mat.values[13],
        x * mat.values[2] + y * mat.values[6] + z * mat.values[10] + mat.values[14],
        x * mat.values[3] + y * mat.values[7] + z * mat.values[11] + mat.values[15]
    };

    product /= product.w;

    return {
        product.x,
        product.y,
        product.z
    };
}

Vec3<float>& math::Vec3<float>::operator*=(const Mat4f& mat)
{
    return operator=(operator*(mat));
}

Vec3<float> math::Vec3<float>::operator*(const Quaternion& quat) const
{
    Vec3<float> result;
    result.x = quat.w * quat.w * x + 2 * quat.y * quat.w * z - 2 * quat.z * quat.w * y + quat.x * quat.x * x + 2 * quat.y * quat.x * y + 2 * quat.z * quat.x * z - quat.z * quat.z * x - quat.y * quat.y * x;

    result.y = 2 * quat.x * quat.y * x + quat.y * quat.y * y + 2 * quat.z * quat.y * z + 2 * quat.w * quat.z * x - quat.z * quat.z * y + quat.w * quat.w * y - 2 * quat.x * quat.w * z - quat.x * quat.x * y;

    result.z = 2 * quat.x * quat.z * x + 2 * quat.y * quat.z * y + quat.z * quat.z * z - 2 * quat.w * quat.y * x - quat.y * quat.y * z + 2 * quat.w * quat.x * y - quat.x * quat.x * z + quat.w * quat.w * z;
    return result;
}

Vec3<float>& math::Vec3<float>::operator*=(const Quaternion& quat)
{
    return operator=(operator*(quat));
}

float math::Vec3<float>::DistanceSquared(const Vec3f& other) const
{
#if HYP_VECTOR3_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    const __m128 diff = _mm_sub_ps(LoadVec3f(*this), LoadVec3f(other));
    const __m128 sq = _mm_mul_ps(diff, diff);
    const Vec3<float> packed = StoreVec3f(sq);

    return packed.x + packed.y + packed.z;
#else
    float dx = x - other.x;
    float dy = y - other.y;
    float dz = z - other.z;
    return dx * dx + dy * dy + dz * dz;
#endif
}

/* Euclidean distance */
float math::Vec3<float>::Distance(const Vec3f& other) const
{
    return MathUtil::Sqrt(DistanceSquared(other));
}

Vec3<float> math::Vec3<float>::Normalized() const
{
#if HYP_VECTOR3_USE_SSE
    const float len = MathUtil::Max(Length(), MathUtil::epsilonF);
    const __m128 invLen = _mm_set1_ps(1.0f / len);

    return StoreVec3f(_mm_mul_ps(LoadVec3f(*this), invLen));
#else
    return *this / MathUtil::Max(Length(), MathUtil::epsilonF);
#endif
}

Vec3<float>& math::Vec3<float>::Normalize()
{
#if HYP_VECTOR3_USE_SSE
    const float len = MathUtil::Max(Length(), MathUtil::epsilonF);
    const __m128 invLen = _mm_set1_ps(1.0f / len);
    *this = StoreVec3f(_mm_mul_ps(LoadVec3f(*this), invLen));

    return *this;
#else
    return *this /= MathUtil::Max(Length(), MathUtil::epsilonF);
#endif
}

Vec3<float> math::Vec3<float>::Cross(const Vec3<float>& other) const
{
    return {
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
    };
}

Vec3<float> math::Vec3<float>::Reflect(const Vec3<float>& normal) const
{
    const Vec3& incident = *this;
    return incident - Vec3(2.0f) * Dot(normal) * normal;
}

Vec3<float>& math::Vec3<float>::Rotate(const Vec3<float>& axis, float radians)
{
    return (*this) = Mat4f::Rotation(axis, radians) * (*this);
}

Vec3<float>& math::Vec3<float>::Rotate(const Quaternion& quaternion)
{
    return (*this) = Mat4f::Rotation(quaternion) * (*this);
}

Vec3<float>& math::Vec3<float>::Lerp(const Vec3<float>& to, const float amt)
{
#if HYP_VECTOR3_USE_SSE
    // a + f * (b - a);
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    const __m128 a = LoadVec3f(*this);
    const __m128 b = LoadVec3f(to);
    const __m128 f = _mm_set1_ps(amt);
    *this = StoreVec3f(_mm_add_ps(a, _mm_mul_ps(_mm_sub_ps(b, a), f)));
#else
    x = MathUtil::Lerp(x, to.x, amt);
    y = MathUtil::Lerp(y, to.y, amt);
    z = MathUtil::Lerp(z, to.z, amt);
#endif

    return *this;
}

float math::Vec3<float>::Dot(const Vec3<float>& other) const
{
#if HYP_VECTOR3_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    const __m128 product = _mm_mul_ps(LoadVec3f(*this), LoadVec3f(other));
    const Vec3<float> packed = StoreVec3f(product);

    return packed.x + packed.y + packed.z;
#else
    return x * other.x + y * other.y + z * other.z;
#endif
}

float math::Vec3<float>::AngleBetween(const Vector3& other) const
{
    const float dotProduct = x * other.x + y * other.y + z * other.z;
    const float arcCos = MathUtil::Arccos(dotProduct);

    return arcCos / (Length() * other.Length());
}

Vec3<float> math::Vec3<float>::Abs(const Vec3<float>& vec)
{
#if HYP_VECTOR3_USE_SSE
    const __m128 signMask = _mm_set1_ps(-0.0f);
    return StoreVec3f(_mm_andnot_ps(signMask, LoadVec3f(vec)));
#else
    return {
        MathUtil::Abs(vec.x),
        MathUtil::Abs(vec.y),
        MathUtil::Abs(vec.z)
    };
#endif
}

Vec3<float> math::Vec3<float>::Round(const Vec3<float>& vec)
{
    return {
        MathUtil::Round(vec.x),
        MathUtil::Round(vec.y),
        MathUtil::Round(vec.z)
    };
}

Vec3<float> math::Vec3<float>::Clamp(const Vec3<float>& vec, float minValue, float maxValue)
{
    return Max(minValue, Min(vec, maxValue));
}

Vec3<float> math::Vec3<float>::Min(const Vec3<float>& a, const Vec3<float>& b)
{
#if HYP_VECTOR3_USE_SSE
    return StoreVec3f(_mm_min_ps(LoadVec3f(a), LoadVec3f(b)));
#else
    return {
        MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z)
    };
#endif
}

Vec3<float> math::Vec3<float>::Max(const Vec3<float>& a, const Vec3<float>& b)
{
#if HYP_VECTOR3_USE_SSE
    return StoreVec3f(_mm_max_ps(LoadVec3f(a), LoadVec3f(b)));
#else
    return {
        MathUtil::Max(a.x, b.x),
        MathUtil::Max(a.y, b.y),
        MathUtil::Max(a.z, b.z)
    };
#endif
}

} // namespace Hyperion
