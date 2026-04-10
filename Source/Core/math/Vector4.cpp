/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/math/Vector4.hpp>
#include <Core/math/MathUtil.hpp>
#include <Core/math/Vector3.hpp>
#include <Core/math/Vector2.hpp>
#include <Core/math/Mat4f.hpp>

#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#if !HYP_ARM && (defined(__SSE2__) || (HYP_MSVC && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))))
#include <immintrin.h>
#define HYP_VECTOR4_USE_SSE 0//1
#else
#define HYP_VECTOR4_USE_SSE 0
#endif

namespace {

#if HYP_VECTOR4_USE_SSE
HYP_FORCE_INLINE __m128 LoadVec4f(const Hyperion::math::Vec4<float>& value)
{
    return _mm_setr_ps(value.x, value.y, value.z, value.w);
}

HYP_FORCE_INLINE Hyperion::math::Vec4<float> StoreVec4f(__m128 value)
{
    alignas(16) float data[4];
    _mm_store_ps(data, value);

    return { data[0], data[1], data[2], data[3] };
}
#endif

} // namespace

namespace Hyperion {

HYP_API const Class* g_clsVec4f = nullptr;
HYP_API const Class* g_clsVec4i = nullptr;
HYP_API const Class* g_clsVec4u = nullptr;

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

float math::Vec4<float>::DistanceSquared(const Vec4& other) const
{
#if HYP_VECTOR4_USE_SSE
    const __m128 diff = _mm_sub_ps(LoadVec4f(*this), LoadVec4f(other));
    const __m128 sq = _mm_mul_ps(diff, diff);
    const Vec4<float> packed = StoreVec4f(sq);

    return packed.x + packed.y + packed.z + packed.w;
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

    return StoreVec4f(_mm_mul_ps(LoadVec4f(*this), invLen));
#else
    return *this / MathUtil::Max(Length(), MathUtil::epsilonF);
#endif
}

Vec4<float>& math::Vec4<float>::Normalize()
{
#if HYP_VECTOR4_USE_SSE
    const float len = MathUtil::Max(Length(), MathUtil::epsilonF);
    const __m128 invLen = _mm_set1_ps(1.0f / len);
    *this = StoreVec4f(_mm_mul_ps(LoadVec4f(*this), invLen));

    return *this;
#else
    return *this /= MathUtil::Max(Length(), MathUtil::epsilonF);
#endif
}

Vec4<float>& math::Vec4<float>::Rotate(const Vec3<float>& axis, float radians)
{
    return (*this) = Mat4f::Rotation(axis, radians) * (*this);
}

Vec4<float>& math::Vec4<float>::Lerp(const Vec4<float>& to, float amt)
{
#if HYP_VECTOR4_USE_SSE
    // a + f * (b - a);
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec4_AVX.inl
    const __m128 a = LoadVec4f(*this);
    const __m128 b = LoadVec4f(to);
    const __m128 f = _mm_set1_ps(amt);
    *this = StoreVec4f(_mm_add_ps(a, _mm_mul_ps(_mm_sub_ps(b, a), f)));
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
    const __m128 product = _mm_mul_ps(LoadVec4f(*this), LoadVec4f(other));
    const Vec4<float> packed = StoreVec4f(product);

    return packed.x + packed.y + packed.z + packed.w;
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
    return StoreVec4f(_mm_andnot_ps(signMask, LoadVec4f(vec)));
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
    return StoreVec4f(_mm_min_ps(LoadVec4f(a), LoadVec4f(b)));
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
    return StoreVec4f(_mm_max_ps(LoadVec4f(a), LoadVec4f(b)));
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

} // namespace Hyperion
