/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/math/Vector2.hpp>
#include <Core/math/Vector4.hpp>
#include <Core/math/MathUtil.hpp>

#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#if !HYP_ARM && (defined(__SSE2__) || (HYP_MSVC && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))))
#include <immintrin.h>
#define HYP_VECTOR2_USE_SSE 0//1
#else
#define HYP_VECTOR2_USE_SSE 0
#endif

namespace {

#if HYP_VECTOR2_USE_SSE
HYP_FORCE_INLINE __m128 LoadVec2f(const Hyperion::math::Vec2<float>& value)
{
    return _mm_setr_ps(value.x, value.y, 0.0f, 0.0f);
}

HYP_FORCE_INLINE Hyperion::math::Vec2<float> StoreVec2f(__m128 value)
{
    alignas(16) float data[4];
    _mm_store_ps(data, value);

    return { data[0], data[1] };
}
#endif

} // namespace

namespace Hyperion {

HYP_API const Class* g_clsVec2f = nullptr;
HYP_API const Class* g_clsVec2i = nullptr;
HYP_API const Class* g_clsVec2u = nullptr;

// clang-format off
HYP_BEGIN_STRUCT(Vec2f, -1, 0, {})
    Field(NAME(HYP_STR(x)), &Type::x, HYP_OFFSET_OF(Type, x)),
    Field(NAME(HYP_STR(y)), &Type::y, HYP_OFFSET_OF(Type, y))
HYP_END_STRUCT

HYP_REGISTER_STATIC_CLASS(Vec2f);

HYP_BEGIN_STRUCT(Vec2i, -1, 0, {})
    Field(NAME(HYP_STR(x)), &Type::x, HYP_OFFSET_OF(Type, x)),
    Field(NAME(HYP_STR(y)), &Type::y, HYP_OFFSET_OF(Type, y))
HYP_END_STRUCT

HYP_REGISTER_STATIC_CLASS(Vec2i);

HYP_BEGIN_STRUCT(Vec2u, -1, 0, {})
    Field(NAME(HYP_STR(x)), &Type::x, HYP_OFFSET_OF(Type, x)),
    Field(NAME(HYP_STR(y)), &Type::y, HYP_OFFSET_OF(Type, y))
HYP_END_STRUCT

HYP_REGISTER_STATIC_CLASS(Vec2u);
// clang-format on

namespace math {
float Vec2<float>::Distance(const Vec2<float>& other) const
{
    return MathUtil::Sqrt(DistanceSquared(other));
}

float Vec2<float>::DistanceSquared(const Vec2<float>& other) const
{
#if HYP_VECTOR2_USE_SSE
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    const __m128 diff = _mm_sub_ps(LoadVec2f(*this), LoadVec2f(other));
    const __m128 sq = _mm_mul_ps(diff, diff);
    const Vec2<float> packed = StoreVec2f(sq);

    return packed.x + packed.y;
#else
    float dx = x - other.x;
    float dy = y - other.y;
    return dx * dx + dy * dy;
#endif
}

Vec2<float>& Vec2<float>::Normalize()
{
    float len = Length();
    float lenSqr = len * len;
    if (lenSqr == 0 || lenSqr == 1)
    {
        return *this;
    }

#if HYP_VECTOR2_USE_SSE
    const __m128 invLen = _mm_set1_ps(1.0f / len);
    const __m128 normalized = _mm_mul_ps(LoadVec2f(*this), invLen);
    *this = StoreVec2f(normalized);
#else
    (*this) *= (1.0f / len);
#endif

    return *this;
}

Vec2<float>& Vec2<float>::Lerp(const Vec2<float>& to, const float amt)
{
#if HYP_VECTOR2_USE_SSE
    // a + f * (b - a);
    // Adapted from Foxtrot SIMD vector paths:
    // Math/Impl/Vector/FxVec3_AVX.inl
    const __m128 a = LoadVec2f(*this);
    const __m128 b = LoadVec2f(to);
    const __m128 f = _mm_set1_ps(amt);
    const __m128 result = _mm_add_ps(a, _mm_mul_ps(_mm_sub_ps(b, a), f));
    *this = StoreVec2f(result);
#else
    x = MathUtil::Lerp(x, to.x, amt);
    y = MathUtil::Lerp(y, to.y, amt);
#endif

    return *this;
}

float Vec2<float>::Dot(const Vec2<float>& other) const
{
#if HYP_VECTOR2_USE_SSE
    const __m128 product = _mm_mul_ps(LoadVec2f(*this), LoadVec2f(other));
    const Vec2<float> packed = StoreVec2f(product);

    return packed.x + packed.y;
#else
    return x * other.x + y * other.y;
#endif
}

Vec2<float> Vec2<float>::Abs(const Vec2<float>& vec)
{
#if HYP_VECTOR2_USE_SSE
    const __m128 signMask = _mm_set1_ps(-0.0f);
    return StoreVec2f(_mm_andnot_ps(signMask, LoadVec2f(vec)));
#else
    return Vector2(abs(vec.x), abs(vec.y));
#endif
}

Vec2<float> Vec2<float>::Round(const Vec2<float>& vec)
{
    return Vector2(std::round(vec.x), std::round(vec.y));
}

Vec2<float> Vec2<float>::Clamp(const Vec2<float>& vec, float minValue, float maxValue)
{
    return Max(minValue, Min(vec, maxValue));
}

Vec2<float> Vec2<float>::Min(const Vec2<float>& a, const Vec2<float>& b)
{
#if HYP_VECTOR2_USE_SSE
    return StoreVec2f(_mm_min_ps(LoadVec2f(a), LoadVec2f(b)));
#else
    return Vec2<float>(MathUtil::Min(a.x, b.x), MathUtil::Min(a.y, b.y));
#endif
}

Vec2<float> Vec2<float>::Max(const Vec2<float>& a, const Vec2<float>& b)
{
#if HYP_VECTOR2_USE_SSE
    return StoreVec2f(_mm_max_ps(LoadVec2f(a), LoadVec2f(b)));
#else
    return Vec2<float>(MathUtil::Max(a.x, b.x), MathUtil::Max(a.y, b.y));
#endif
}

} // namespace math
} // namespace Hyperion
