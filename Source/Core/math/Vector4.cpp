/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/math/Vector4.hpp>
#include <Core/math/MathUtil.hpp>
#include <Core/math/Vector3.hpp>
#include <Core/math/Vector2.hpp>
#include <Core/math/Mat3f.hpp>
#include <Core/math/Mat4f.hpp>
#include <Core/math/Quat4f.hpp>

#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#if !HYP_ARM && (defined(__SSE4_1__) || (HYP_MSVC && defined(_M_X64)))
#include <immintrin.h>
#define HYP_VECTOR4_USE_SSE 1
#else
#define HYP_VECTOR4_USE_SSE 0
#endif

namespace {

#if HYP_VECTOR4_USE_SSE
static HYP_FORCE_INLINE Hyperion::math::Vec4<float> StoreVec4f(__m128 value)
{
    Hyperion::math::Vec4<float> result;
    result._value = value;
    return result;
}
#endif

} // namespace

namespace Hyperion {

CORE_API const Class* g_clsVec4f = nullptr;
CORE_API const Class* g_clsVec4i = nullptr;
CORE_API const Class* g_clsVec4u = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(Vec4f, -1, 0, {})
    Field(NAME(HYP_STR(x)), &Type::x, HYP_OFFSET_OF(Type, x)),
    Field(NAME(HYP_STR(y)), &Type::y, HYP_OFFSET_OF(Type, y)),
    Field(NAME(HYP_STR(z)), &Type::z, HYP_OFFSET_OF(Type, z)),
    Field(NAME(HYP_STR(w)), &Type::w, HYP_OFFSET_OF(Type, w))
HYP_END_STRUCT

HYP_REGISTER_STATIC_CLASS(Vec4f);

HYP_BEGIN_STRUCT(Vec4i, -1, 0, {})
    Field(NAME(HYP_STR(x)), &Type::x, HYP_OFFSET_OF(Type, x)),
    Field(NAME(HYP_STR(y)), &Type::y, HYP_OFFSET_OF(Type, y)),
    Field(NAME(HYP_STR(z)), &Type::z, HYP_OFFSET_OF(Type, z)),
    Field(NAME(HYP_STR(w)), &Type::w, HYP_OFFSET_OF(Type, w))
HYP_END_STRUCT

HYP_REGISTER_STATIC_CLASS(Vec4i);

HYP_BEGIN_STRUCT(Vec4u, -1, 0, {})
    Field(NAME(HYP_STR(x)), &Type::x, HYP_OFFSET_OF(Type, x)),
    Field(NAME(HYP_STR(y)), &Type::y, HYP_OFFSET_OF(Type, y)),
    Field(NAME(HYP_STR(z)), &Type::z, HYP_OFFSET_OF(Type, z)),
    Field(NAME(HYP_STR(w)), &Type::w, HYP_OFFSET_OF(Type, w))
HYP_END_STRUCT

HYP_REGISTER_STATIC_CLASS(Vec4u);

// clang-format on

bool math::Vec4<float>::operator==(const Vec4<float>& other) const
{
#if HYP_VECTOR4_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec4_AVX.inl
    __m128i cmp_v = _mm_castps_si128(_mm_cmpeq_ps(_value, other._value));
    // This is frequently done as a comparision against _mm_movemask, but _mm_test_all_ones potentially saves one extra
    // instruction.
    return static_cast<bool>(_mm_test_all_ones(cmp_v));
#else
    return memcmp(this, &other, sizeof(math::Vec4<float>)) == 0;
#endif
}

bool math::Vec4<float>::operator!=(const Vec4<float>& other) const
{
    return !operator==(other);
}

Vec4<float> math::Vec4<float>::operator+(const Vec4<float>& other) const
{
#if HYP_VECTOR4_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec4_AVX.inl
    return StoreVec4f(_mm_add_ps(_value, other._value));
#else
    return {
        x + other.x,
        y + other.y,
        z + other.z,
        w + other.w
    };
#endif
}

Vec4<float>& math::Vec4<float>::operator+=(const Vec4<float>& other)
{
#if HYP_VECTOR4_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec4_AVX.inl
    _value = _mm_add_ps(_value, other._value);

    return *this;
#else
    x += other.x;
    y += other.y;
    z += other.z;
    w += other.w;

    return *this;
#endif
}

Vec4<float> math::Vec4<float>::operator-(const Vec4<float>& other) const
{
#if HYP_VECTOR4_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec4_AVX.inl
    return StoreVec4f(_mm_sub_ps(_value, other._value));
#else
    return {
        x - other.x,
        y - other.y,
        z - other.z,
        w - other.w
    };
#endif
}

Vec4<float>& math::Vec4<float>::operator-=(const Vec4<float>& other)
{
#if HYP_VECTOR4_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec4_AVX.inl
    _value = _mm_sub_ps(_value, other._value);

    return *this;
#else
    x -= other.x;
    y -= other.y;
    z -= other.z;
    w -= other.w;

    return *this;
#endif
}

Vec4<float> math::Vec4<float>::operator*(const Vec4<float>& other) const
{
#if HYP_VECTOR4_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec4_AVX.inl
    return StoreVec4f(_mm_mul_ps(_value, other._value));
#else
    return {
        x * other.x,
        y * other.y,
        z * other.z,
        w * other.w
    };
#endif
}

Vec4<float>& math::Vec4<float>::operator*=(const Vec4<float>& other)
{
#if HYP_VECTOR4_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec4_AVX.inl
    _value = _mm_mul_ps(_value, other._value);

    return *this;
#else
    x *= other.x;
    y *= other.y;
    z *= other.z;
    w *= other.w;

    return *this;
#endif
}

Vec4<float> math::Vec4<float>::operator/(const Vec4<float>& other) const
{
#if HYP_VECTOR4_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec4_AVX.inl
    return StoreVec4f(_mm_div_ps(_value, other._value));
#else
    return {
        x / other.x,
        y / other.y,
        z / other.z,
        w / other.w
    };
#endif
}

Vec4<float> math::Vec4<float>::operator-() const
{
#if HYP_VECTOR4_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec4_AVX.inl
    return StoreVec4f(_mm_sub_ps(_mm_setzero_ps(), _value));
#else
    return {
        -x,
        -y,
        -z,
        -w
    };
#endif
}

float math::Vec4<float>::LengthSquared() const
{
#if HYP_VECTOR4_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec4_AVX.inl
    // Mask 0xFF: Src=1111 (all lanes), Dest=1111 (broadcast to all lanes).
    return _mm_cvtss_f32(_mm_dp_ps(_value, _value, 0xFF));
#else
    return x * x + y * y + z * z + w * w;
#endif
}

float math::Vec4<float>::DistanceSquared(const Vec4& other) const
{
#if HYP_VECTOR4_USE_SSE
    const __m128 diff = _mm_sub_ps(_value, other._value);
    return _mm_cvtss_f32(_mm_dp_ps(diff, diff, 0xFF));
#else
    float dx = x - other.x;
    float dy = y - other.y;
    float dz = z - other.z;
    float dw = w - other.w;
    return dx * dx + dy * dy + dz * dz + dw * dw;
#endif
}

/* Euclidean distance */
float math::Vec4<float>::Distance(const Vec4& other) const
{
    return MathUtil::Sqrt(DistanceSquared(other));
}

Vec4<float> math::Vec4<float>::Normalized() const
{
#if HYP_VECTOR4_USE_SSE
    const float len = MathUtil::Max(Length(), MathUtil::epsilonF);
    const __m128 invLen = _mm_set1_ps(1.0f / len);

    return StoreVec4f(_mm_mul_ps(_value, invLen));
#else
    return *this / MathUtil::Max(Length(), MathUtil::epsilonF);
#endif
}

Vec4<float>& math::Vec4<float>::Normalize()
{
#if HYP_VECTOR4_USE_SSE
    const float len = MathUtil::Max(Length(), MathUtil::epsilonF);
    const __m128 invLen = _mm_set1_ps(1.0f / len);
    _value = _mm_mul_ps(_value, invLen);

    return *this;
#else
    return *this /= MathUtil::Max(Length(), MathUtil::epsilonF);
#endif
}

Vec4<float>& math::Vec4<float>::Rotate(const Vec3<float>& axis, float radians)
{
    return (*this) = Mat4f::Rotation(axis, radians).TransformVector(*this);
}

Vec4<float>& math::Vec4<float>::Lerp(const Vec4<float>& to, float amt)
{
#if HYP_VECTOR4_USE_SSE
    // a + f * (b - a);
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec4_AVX.inl
    const __m128 f = _mm_set1_ps(amt);
    _value = _mm_add_ps(_value, _mm_mul_ps(_mm_sub_ps(to._value, _value), f));
#else
    x = MathUtil::Lerp(x, to.x, amt);
    y = MathUtil::Lerp(y, to.y, amt);
    z = MathUtil::Lerp(z, to.z, amt);
    w = MathUtil::Lerp(w, to.w, amt);
#endif

    return *this;
}

float math::Vec4<float>::Dot(const Vec4<float>& other) const
{
#if HYP_VECTOR4_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/FxSSEUtil.hpp
    // Mask 0xFF: Src=1111 (all lanes), Dest=1111 (broadcast to all lanes).
    return _mm_cvtss_f32(_mm_dp_ps(_value, other._value, 0xFF));
#else
    return x * other.x + y * other.y + z * other.z + w * other.w;
#endif
}

template <>
Vec4<int> math::Vec4<int>::Abs(const Vec4<int>& vec)
{
    return {
        MathUtil::Abs(vec.x),
        MathUtil::Abs(vec.y),
        MathUtil::Abs(vec.z),
        MathUtil::Abs(vec.w)
    };
}

template <>
Vec4<int> math::Vec4<int>::Min(const Vec4<int>& a, const Vec4<int>& b)
{
    return {
        MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z),
        MathUtil::Min(a.w, b.w)
    };
}

template <>
Vec4<int> math::Vec4<int>::Max(const Vec4<int>& a, const Vec4<int>& b)
{
    return {
        MathUtil::Max(a.x, b.x),
        MathUtil::Max(a.y, b.y),
        MathUtil::Max(a.z, b.z),
        MathUtil::Max(a.w, b.w)
    };
}

template <>
Vec4<uint32> math::Vec4<uint32>::Abs(const Vec4<uint32>& vec)
{
    return {
        MathUtil::Abs(vec.x),
        MathUtil::Abs(vec.y),
        MathUtil::Abs(vec.z),
        MathUtil::Abs(vec.w)
    };
}

template <>
Vec4<uint32> math::Vec4<uint32>::Min(const Vec4<uint32>& a, const Vec4<uint32>& b)
{
    return {
        MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z),
        MathUtil::Min(a.w, b.w)
    };
}

template <>
Vec4<uint32> math::Vec4<uint32>::Max(const Vec4<uint32>& a, const Vec4<uint32>& b)
{
    return {
        MathUtil::Max(a.x, b.x),
        MathUtil::Max(a.y, b.y),
        MathUtil::Max(a.z, b.z),
        MathUtil::Max(a.w, b.w)
    };
}

Vec4<float> math::Vec4<float>::Abs(const Vec4<float>& vec)
{
#if HYP_VECTOR4_USE_SSE
    const __m128 signMask = _mm_set1_ps(-0.0f);
    return StoreVec4f(_mm_andnot_ps(signMask, vec._value));
#else
    return {
        MathUtil::Abs(vec.x),
        MathUtil::Abs(vec.y),
        MathUtil::Abs(vec.z),
        MathUtil::Abs(vec.w)
    };
#endif
}

Vec4<float> math::Vec4<float>::Round(const Vec4<float>& vec)
{
    return {
        MathUtil::Round(vec.x),
        MathUtil::Round(vec.y),
        MathUtil::Round(vec.z),
        MathUtil::Round(vec.w)
    };
}

Vec4<float> math::Vec4<float>::Clamp(const Vec4<float>& vec, float minValue, float maxValue)
{
    return Max(minValue, Min(vec, maxValue));
}

Vec4<float> math::Vec4<float>::Min(const Vec4<float>& a, const Vec4<float>& b)
{
#if HYP_VECTOR4_USE_SSE
    return StoreVec4f(_mm_min_ps(a._value, b._value));
#else
    return {
        MathUtil::Min(a.x, b.x),
        MathUtil::Min(a.y, b.y),
        MathUtil::Min(a.z, b.z),
        MathUtil::Min(a.w, b.w)
    };
#endif
}

Vec4<float> math::Vec4<float>::Max(const Vec4<float>& a, const Vec4<float>& b)
{
#if HYP_VECTOR4_USE_SSE
    return StoreVec4f(_mm_max_ps(a._value, b._value));
#else
    return {
        MathUtil::Max(a.x, b.x),
        MathUtil::Max(a.y, b.y),
        MathUtil::Max(a.z, b.z),
        MathUtil::Max(a.w, b.w)
    };
#endif
}

Vec4<float> math::Vec4<float>::operator*(const Mat4f& mat) const
{
    return {
        x * mat.values[0] + y * mat.values[4] + z * mat.values[8] + w * mat.values[12],
        x * mat.values[1] + y * mat.values[5] + z * mat.values[9] + w * mat.values[13],
        x * mat.values[2] + y * mat.values[6] + z * mat.values[10] + w * mat.values[14],
        x * mat.values[3] + y * mat.values[7] + z * mat.values[11] + w * mat.values[15]
    };
}

Vec4<float>& math::Vec4<float>::operator*=(const Mat4f& mat)
{
    return *this = (*this * mat);
}

/* 3-component transform by a 3x3 matrix. The w component is preserved. */
Vec4<float> math::Vec4<float>::operator*(const Mat3f& mat) const
{
    return {
        x * mat.rows[0][0] + y * mat.rows[1][0] + z * mat.rows[2][0],
        x * mat.rows[0][1] + y * mat.rows[1][1] + z * mat.rows[2][1],
        x * mat.rows[0][2] + y * mat.rows[1][2] + z * mat.rows[2][2],
        w
    };
}

Vec4<float>& math::Vec4<float>::operator*=(const Mat3f& mat)
{
    return *this = (*this * mat);
}

/* 3-component rotation by quaternion. The w component is preserved. */
Vec4<float> math::Vec4<float>::operator*(const Quat4f& quat) const
{
    Vec4<float> result;
    result.x = quat.w * quat.w * x + 2 * quat.y * quat.w * z - 2 * quat.z * quat.w * y + quat.x * quat.x * x + 2 * quat.y * quat.x * y + 2 * quat.z * quat.x * z - quat.z * quat.z * x - quat.y * quat.y * x;
    result.y = 2 * quat.x * quat.y * x + quat.y * quat.y * y + 2 * quat.z * quat.y * z + 2 * quat.w * quat.z * x - quat.z * quat.z * y + quat.w * quat.w * y - 2 * quat.x * quat.w * z - quat.x * quat.x * y;
    result.z = 2 * quat.x * quat.z * x + 2 * quat.y * quat.z * y + quat.z * quat.z * z - 2 * quat.w * quat.y * x - quat.y * quat.y * z + 2 * quat.w * quat.x * y - quat.x * quat.x * z + quat.w * quat.w * z;
    result.w = w;
    return result;
}

Vec4<float>& math::Vec4<float>::operator*=(const Quat4f& quat)
{
    return *this = (*this * quat);
}

Vec4<float>& math::Vec4<float>::Rotate(const Quat4f& quaternion)
{
    // 3-component rotation – w is preserved
    return operator*=(quaternion);
}

/* 3-component cross product. The w component of the result is 0. */
Vec4<float> math::Vec4<float>::Cross(const Vec4<float>& other) const
{
#if HYP_VECTOR4_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.cpp
    //
    // Permute both operands to (Y, Z, X, W), compute a*b_yzxw - a_yzxw*b,
    // which yields the cross product in (Z, X, Y, 0) order, then permute
    // once more with the same mask to arrive at (X, Y, Z, 0).
    // The w lane cancels to 0 naturally: a.w*b.w - a.w*b.w = 0.
    const __m128 a_yzxw = _mm_shuffle_ps(_value, _value, _MM_SHUFFLE(3, 0, 2, 1));
    const __m128 b_yzxw = _mm_shuffle_ps(other._value, other._value, _MM_SHUFFLE(3, 0, 2, 1));

    const __m128 result_yzxw = _mm_sub_ps(_mm_mul_ps(_value, b_yzxw), _mm_mul_ps(a_yzxw, other._value));

    return StoreVec4f(_mm_shuffle_ps(result_yzxw, result_yzxw, _MM_SHUFFLE(3, 0, 2, 1)));
#else
    return {
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x,
        0.0f
    };
#endif
}

/* 3-component reflection about a normal. The w component is preserved. */
Vec4<float> math::Vec4<float>::Reflect(const Vec4<float>& normal) const
{
    const float d = x * normal.x + y * normal.y + z * normal.z;
    return {
        x - 2.0f * d * normal.x,
        y - 2.0f * d * normal.y,
        z - 2.0f * d * normal.z,
        w
    };
}

/* 3-component angle between vectors (radians). The w component is ignored. */
float math::Vec4<float>::AngleBetween(const Vec4<float>& other) const
{
    const float dotProduct = x * other.x + y * other.y + z * other.z;
    const float arcCos = MathUtil::Arccos(dotProduct);
    const float lenSelf = MathUtil::Sqrt(x * x + y * y + z * z);
    const float lenOther = MathUtil::Sqrt(other.x * other.x + other.y * other.y + other.z * other.z);
    return arcCos / (lenSelf * lenOther);
}

} // namespace Hyperion
