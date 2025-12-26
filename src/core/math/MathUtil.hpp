/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/Vector2.hpp>
#include <core/math/Vector3.hpp>
#include <core/math/Vector4.hpp>

#include <core/Defines.hpp>

#include <core/Types.hpp>

#include <math.h>
#include <cstdlib>
#include <cmath>
#include <cfloat>
#include <limits>
#include <type_traits>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

namespace Hyperion {

template <class T>
constexpr bool isMathVectorV = isVec2<T> || isVec3<T> || isVec4<T>;

namespace MathUtil {

template <class T>
constexpr T pi = T(3.14159265358979);

constexpr float epsilonF = FLT_EPSILON;
constexpr double epsilonD = DBL_EPSILON;

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(std::is_enum_v<T> && !isMathVectorV<T>, std::underlying_type_t<T>) MaxSafeValue()
{
    return std::numeric_limits<std::underlying_type_t<T>>::max();
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(std::is_enum_v<T> && !isMathVectorV<T>, std::underlying_type_t<T>) MinSafeValue()
{
    return std::numeric_limits<std::underlying_type_t<T>>::lowest();
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(!std::is_enum_v<T> && !isMathVectorV<T>, T) MaxSafeValue()
{
    return std::numeric_limits<T>::max();
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(!std::is_enum_v<T> && !isMathVectorV<T>, T) MinSafeValue()
{
    return std::numeric_limits<T>::lowest();
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(isMathVectorV<T>, T) MaxSafeValue()
{
    return T(MaxSafeValue<std::remove_all_extents_t<decltype(T::values)>>());
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(isMathVectorV<T>, T) MinSafeValue()
{
    return T(MinSafeValue<std::remove_all_extents_t<decltype(T::values)>>());
}

template <class T>
static HYP_FORCE_INLINE constexpr auto MaxSafeValue(T)
{
    return MaxSafeValue<T>();
}

template <class T>
static HYP_FORCE_INLINE constexpr auto MinSafeValue(T)
{
    return MinSafeValue<T>();
}

static HYP_FORCE_INLINE Vec2f SafeValue(const Vec2f& value)
{
    return Vec2f::Max(Vec2f::Min(value, MaxSafeValue<decltype(value[0])>()), MinSafeValue<decltype(value[0])>());
}

static HYP_FORCE_INLINE Vec3f SafeValue(const Vec3f& value)
{
    return Vec3f::Max(Vec3f::Min(value, MaxSafeValue<decltype(value[0])>()), MinSafeValue<decltype(value[0])>());
}

static HYP_FORCE_INLINE Vec4f SafeValue(const Vec4f& value)
{
    return Vec4f::Max(Vector4::Min(value, MaxSafeValue<decltype(value[0])>()), MinSafeValue<decltype(value[0])>());
}

template <class T>
static HYP_FORCE_INLINE T SafeValue(const T& value)
{
    return Max(Min(value, MaxSafeValue<T>()), MinSafeValue<T>());
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(std::is_floating_point_v<T>, T) NaN()
{
    return std::numeric_limits<T>::quiet_NaN();
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(isMathVectorV<T>&& std::is_floating_point_v<NormalizedType<decltype(T::values[0])>>, T) NaN()
{
    using VectorScalarType = NormalizedType<decltype(T::values[0])>;

    T result {}; /* doesn't need initialization but gets rid of annoying warnings */

    for (uint32 i = 0; i < HYP_ARRAY_SIZE(result.values); i++)
    {
        result.values[i] = VectorScalarType(NaN<VectorScalarType>());
    }

    return result;
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(std::is_floating_point_v<T>, bool) IsNaN(T value)
{
    return value != value;
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(isMathVectorV<T>&& std::is_floating_point_v<NormalizedType<decltype(T::values[0])>>, bool) IsNaN(const T& value)
{
    for (uint32 i = 0; i < HYP_ARRAY_SIZE(value.values); i++)
    {
        if (IsNaN(value.values[i]))
        {
            return true;
        }
    }

    return false;
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(std::is_floating_point_v<T>, T) Infinity()
{
    return std::numeric_limits<T>::infinity();
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(isMathVectorV<T>&& std::is_floating_point_v<NormalizedType<decltype(T::values[0])>>, T) Infinity()
{
    using VectorScalarType = NormalizedType<decltype(T::values[0])>;

    T result {}; /* doesn't need initialization but gets rid of annoying warnings */

    for (uint32 i = 0; i < HYP_ARRAY_SIZE(result.values); i++)
    {
        result.values[i] = VectorScalarType(Infinity<VectorScalarType>());
    }

    return result;
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(std::is_floating_point_v<T>, bool) IsFinite(T value)
{
    return value != Infinity<T>() && value != -Infinity<T>();
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(isMathVectorV<T>&& std::is_floating_point_v<NormalizedType<decltype(T::values[0])>>, bool) IsFinite(const T& value)
{
    for (uint32 i = 0; i < HYP_ARRAY_SIZE(value.values); i++)
    {
        if (!IsFinite(value.values[i]))
        {
            return false;
        }
    }

    return true;
}

template <class T>
static HYP_ENABLE_IF(isMathVectorV<T>, T) RandRange(const T& a, const T& b)
{
    T result;

    for (uint32 i = 0; i < HYP_ARRAY_SIZE(result.values); i++)
    {
        result.values[i] = RandRange(a.values[i], b.values[i]);
    }

    return result;
}

template <class T>
static HYP_ENABLE_IF(!isMathVectorV<T>, T) RandRange(T a, T b)
{
    const auto random = T(rand()) / T(RAND_MAX);
    const auto diff = b - a;
    const auto r = random * diff;

    return a + r;
}

static HYP_FORCE_INLINE constexpr uint32 Rand32(uint32& seed)
{
    return (seed = 1664525 * seed + 1013904223);
}

static HYP_FORCE_INLINE constexpr uint64 Rand64(uint64& seed)
{
    // 64-bit linear congruential generator (LCG)
    return (seed = 6364136223846793005ULL * seed + 1442695040888963407ULL);
}

static HYP_FORCE_INLINE constexpr float RandomFloat(uint32& seed)
{
    return (float(Rand32(seed) & 0x00FFFFFF) / float(0x01000000));
}

template <class T>
static HYP_FORCE_INLINE constexpr T RadToDeg(T rad)
{
    return rad * T(180) / pi<T>;
}

template <class T>
static HYP_FORCE_INLINE constexpr T DegToRad(T deg)
{
    return deg * pi<T> / T(180);
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(!isMathVectorV<T>, T) Clamp(T val, T min, T max)
{
    if (val > max)
    {
        return max;
    }
    else if (val < min)
    {
        return min;
    }
    else
    {
        return val;
    }
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(isMathVectorV<T>, T) Clamp(const T& val, const T& min, const T& max)
{
    T result;

    for (uint32 i = 0; i < HYP_ARRAY_SIZE(result.values); i++)
    {
        result.values[i] = Clamp(val.values[i], min.values[i], max.values[i]);
    }

    return result;
}

template <class T, class U, class V>
static HYP_FORCE_INLINE constexpr auto Lerp(const T& from, const U& to, const V& amt) -> std::enable_if_t<!isMathVectorV<T>, decltype(from + amt * (to - from))>
{
    return from + amt * (to - from);
}

template <class T, class U, class V>
static HYP_FORCE_INLINE HYP_ENABLE_IF(isMathVectorV<T>, T) Lerp(const T& from, const U& to, const V& amt)
{
    T result;

    for (uint32 i = 0; i < HYP_ARRAY_SIZE(result.values); i++)
    {
        result.values[i] = Lerp(from.values[i], to.values[i], amt);
    }

    return result;
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(!isMathVectorV<T>, T) Step(T edge, T x)
{
    return x < edge ? 0.0f : 1.0f;
}

template <class T>
static HYP_FORCE_INLINE HYP_ENABLE_IF(isMathVectorV<T>, T) Step(const T& edge, const T& x)
{
    T result;

    for (uint32 i = 0; i < HYP_ARRAY_SIZE(result.values); i++)
    {
        result.values[i] = Step(edge.values[i], x.values[i]);
    }

    return result;
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(!isMathVectorV<T>, T) Min(T a)
{
    return a;
}

template <class T, class U, class V = std::common_type_t<T, U>>
static HYP_FORCE_INLINE constexpr V Min(T a, U b)
{
    return (a < b) ? a : b;
}

template <class T, class U, class V = std::common_type_t<T, U>, class... Args>
static HYP_FORCE_INLINE constexpr V Min(T a, U b, Args... args)
{
    return Min(Min(a, b), args...);
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(!isMathVectorV<T>, T) Max(T a)
{
    return a;
}

template <class T, class U, class V = std::common_type_t<T, U>>
static HYP_FORCE_INLINE constexpr V Max(T a, U b)
{
    return (a > b) ? a : b;
}

template <class T, class U, class V = std::common_type_t<T, U>, class... Args>
static HYP_FORCE_INLINE constexpr V Max(T a, U b, Args... args)
{
    return Max(Max(a, b), args...);
}

template <class T>
static HYP_FORCE_INLINE HYP_ENABLE_IF(isMathVectorV<T>, T) Min(const T& a, const T& b)
{
    T result;

    for (uint32 i = 0; i < HYP_ARRAY_SIZE(result.values); i++)
    {
        result.values[i] = Min(a.values[i], b.values[i]);
    }

    return result;
}

template <class T>
static HYP_FORCE_INLINE HYP_ENABLE_IF(isMathVectorV<T>, T) Max(const T& a, const T& b)
{
    T result;

    for (uint32 i = 0; i < HYP_ARRAY_SIZE(result.values); i++)
    {
        result.values[i] = Max(a.values[i], b.values[i]);
    }

    return result;
}

template <class T, class IntegralType = int>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(!isMathVectorV<T>, IntegralType) Sign(T value)
{
    return IntegralType(T(0) < value) - IntegralType(value < T(0));
}

template <class T>
static HYP_FORCE_INLINE HYP_ENABLE_IF(isMathVectorV<T>, T) Sign(const T& a)
{
    using VectorScalarType = NormalizedType<decltype(T::values[0])>;

    T result {};

    for (uint32 i = 0; i < HYP_ARRAY_SIZE(result.values); i++)
    {
        result.values[i] = VectorScalarType(Sign<VectorScalarType>(a.values[i]));
    }

    return result;
}

template <class T, class IntegralType = std::conditional_t<std::is_integral_v<T>, T, int>>
static HYP_FORCE_INLINE HYP_ENABLE_IF(!isMathVectorV<T>, IntegralType) Trunc(T a)
{
    return IntegralType(std::trunc(a));
}

template <class T, class IntegralType = std::conditional_t<std::is_integral_v<T>, T, int>>
static HYP_FORCE_INLINE HYP_ENABLE_IF(isMathVectorV<T>, T) Trunc(const T& a)
{
    using VectorScalarType = NormalizedType<decltype(T::values[0])>;

    T result {}; /* doesn't need initialization but gets rid of annoying warnings */

    for (uint32 i = 0; i < HYP_ARRAY_SIZE(result.values); i++)
    {
        result.values[i] = VectorScalarType(Trunc<VectorScalarType, IntegralType>(a.values[i]));
    }

    return result;
}

template <class T, class IntegralType = std::conditional_t<std::is_integral_v<T>, T, int>>
static HYP_FORCE_INLINE HYP_ENABLE_IF(!isMathVectorV<T>, IntegralType) Floor(T a)
{
    return IntegralType(std::floor(a));
}

template <class T, class IntegralType = std::conditional_t<std::is_integral_v<T>, T, int>>
static HYP_FORCE_INLINE HYP_ENABLE_IF(isMathVectorV<T>, T) Floor(const T& a)
{
    using VectorScalarType = NormalizedType<decltype(T::values[0])>;

    T result {}; /* doesn't need initialization but gets rid of annoying warnings */

    for (uint32 i = 0; i < HYP_ARRAY_SIZE(result.values); i++)
    {
        result.values[i] = VectorScalarType(Floor<VectorScalarType, IntegralType>(a.values[i]));
    }

    return result;
}

template <class T, class IntegralType = std::conditional_t<std::is_integral_v<T>, T, int>>
static HYP_FORCE_INLINE HYP_ENABLE_IF(!isMathVectorV<T>, IntegralType) Ceil(T a)
{
    return IntegralType(std::ceil(a));
}

template <class T, class IntegralType = std::conditional_t<std::is_integral_v<T>, T, int>>
static HYP_FORCE_INLINE HYP_ENABLE_IF(isMathVectorV<T>, T) Ceil(const T& a)
{
    using VectorScalarType = NormalizedType<decltype(T::values[0])>;

    T result {}; /* doesn't need initialization but gets rid of annoying warnings */

    for (uint32 i = 0; i < HYP_ARRAY_SIZE(result.values); i++)
    {
        result.values[i] = VectorScalarType(Ceil<VectorScalarType, IntegralType>(a.values[i]));
    }

    return result;
}

template <class T>
static HYP_FORCE_INLINE T Fract(T a)
{
    return a - Floor<T, T>(a);
}

template <class T>
static HYP_FORCE_INLINE T Exp(T a)
{
    return T(std::exp(a));
}

template <class T>
static HYP_FORCE_INLINE constexpr T Mod(T a, T b)
{
    return (a % b + b) % b;
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(!isMathVectorV<T>, T) Abs(T a)
{
    return a >= T(0) ? a : -a;
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(isMathVectorV<T>, T) Abs(const T& a)
{
    using VectorScalarType = NormalizedType<decltype(T::values[0])>;

    T result {}; /* doesn't need initialization but gets rid of annoying warnings */

    for (uint32 i = 0; i < HYP_ARRAY_SIZE(result.values); i++)
    {
        result.values[i] = VectorScalarType(Abs<VectorScalarType>(a.values[i]));
    }

    return result;
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(isMathVectorV<T>, bool) ApproxEqual(const T& a, const T& b)
{
    return a.DistanceSquared(b) < (std::is_same_v<std::remove_all_extents_t<decltype(T::values)>, double> ? epsilonD : epsilonF);
}

template <class T>
static HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(!isMathVectorV<T>, bool) ApproxEqual(T a, T b, T eps = std::is_same_v<T, double> ? epsilonD : epsilonF)
{
    return Abs(a - b) <= eps;
}

template <class T, class U = T>
static HYP_FORCE_INLINE HYP_ENABLE_IF(!isMathVectorV<T>, U) Round(T a)
{
    return U(std::round(a));
}

template <class T>
static HYP_FORCE_INLINE HYP_ENABLE_IF(isMathVectorV<T>, T) Round(const T& a)
{
    return T::Round(a);
}

static HYP_FORCE_INLINE float Sin(float x)
{
    return sinf(x);
}

static HYP_FORCE_INLINE double Sin(double x)
{
    return sin(x);
}

static HYP_FORCE_INLINE float Arcsin(float x)
{
    return asinf(x);
}

static HYP_FORCE_INLINE double Arcsin(double x)
{
    return asin(x);
}

static HYP_FORCE_INLINE float Cos(float x)
{
    return cosf(x);
}

static HYP_FORCE_INLINE double Cos(double x)
{
    return cos(x);
}

static HYP_FORCE_INLINE float Arccos(float x)
{
    return acosf(x);
}

static HYP_FORCE_INLINE double Arccos(double x)
{
    return acos(x);
}

static HYP_FORCE_INLINE float Tan(float x)
{
    return tanf(x);
}

static HYP_FORCE_INLINE double Tan(double x)
{
    return tan(x);
}

static HYP_FORCE_INLINE float Arctan(float x)
{
    return atanf(x);
}

static HYP_FORCE_INLINE double Arctan(double x)
{
    return atan(x);
}

template <class T>
HYP_FORCE_INLINE constexpr T Factorial(T value)
{
    T f = value;

    if (f > 1)
    {
        for (T i = value - 1; i >= 1; --i)
        {
            f *= i;
        }
    }

    return f;
}

template <class T, class U = T, class V = U>
HYP_FORCE_INLINE constexpr bool InRange(T value, const std::pair<U, V>& range)
{
    return value >= range.first && value < range.second;
}

template <class T, class U = T>
HYP_FORCE_INLINE constexpr U Sqrt(T value)
{
    if constexpr (std::is_same_v<U, double>)
    {
        return sqrt(static_cast<double>(value));
    }
    else if constexpr (std::is_same_v<U, float>)
    {
        return sqrtf(static_cast<float>(value));
    }
    else
    {
        return static_cast<U>(sqrtf(static_cast<float>(value)));
    }
}

template <class T>
HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(!isMathVectorV<T>, T) Pow(T value, T exponent)
{
    if constexpr (std::is_same_v<T, double>)
    {
        return pow(static_cast<double>(value), static_cast<double>(exponent));
    }
    else if constexpr (std::is_same_v<T, float>)
    {
        return powf(static_cast<float>(value), static_cast<float>(exponent));
    }
    else
    {
        return static_cast<T>(powf(static_cast<float>(value), static_cast<float>(exponent)));
    }
}

template <class T>
HYP_FORCE_INLINE constexpr HYP_ENABLE_IF(isMathVectorV<T>, T) Pow(const T& value, typename T::Type exponent)
{
    T result;

    /// \todo : simd
    for (uint32 i = 0; i < HYP_ARRAY_SIZE(result.values); i++)
    {
        result.values[i] = Pow(value.values[i], exponent);
    }

    return result;
}

template <class T>
HYP_FORCE_INLINE constexpr bool IsPowerOfTwo(T value)
{
    return (value & (value - 1)) == 0;
}

// https://stackoverflow.com/questions/11376288/fast-computing-of-log2-for-64-bit-integers
HYP_FORCE_INLINE constexpr uint64 FastLog2(uint64 value)
{
    const int tab64[64] = {
        63, 0, 58, 1, 59, 47, 53, 2,
        60, 39, 48, 27, 54, 33, 42, 3,
        61, 51, 37, 40, 49, 18, 28, 20,
        55, 30, 34, 11, 43, 14, 22, 4,
        62, 57, 46, 52, 38, 26, 32, 41,
        50, 36, 17, 19, 29, 10, 13, 21,
        56, 45, 25, 31, 35, 16, 9, 12,
        44, 24, 15, 8, 23, 7, 6, 5
    };

    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;

    return tab64[(value - (value >> 1)) * 0x07EDD5E59A4E28C2 >> 58];
}

/* Fast log2 for power-of-2 numbers */
HYP_FORCE_INLINE constexpr uint64 FastLog2_Pow2(uint64 value)
{
    if (std::is_constant_evaluated())
    {
        return FastLog2(value);
    }

#if defined(__clang__) || defined(__GNUC__)
#if defined(_MSC_VER)
    return FastLog2(value); // fallback
#else
    return __builtin_ctzll(value);
#endif // _MSC_VER
#elif defined(_MSC_VER)
#ifndef HYP_ARM
    return _tzcnt_u64(value);
#else
    return FastLog2(value); // fallback
#endif // HYP_ARM
#else
    return __builtin_ctzll(value);
#endif // _MSC_VER
}

// https://www.techiedelight.com/round-next-highest-power-2/
HYP_FORCE_INLINE constexpr uint64 NextPowerOf2(uint64 value)
{
    // decrement `n` (to handle the case when `n` itself
    // is a power of 2)
    value = value - 1;

    // next power of two will have a bit set at position `lg+1`.
    return 1ull << (FastLog2(value) + 1);
}

HYP_FORCE_INLINE constexpr uint64 PreviousPowerOf2(uint64 value)
{
    if (value <= 1)
    {
        return 0;
    }

    return NextPowerOf2(value) >> 1;
}

template <class T, class U>
HYP_FORCE_INLINE constexpr auto NextMultiple(T&& value, U&& multiple) -> std::common_type_t<T, U>
{
    if (multiple == 0)
    {
        return value;
    }

    auto remainder = value % multiple;

    if (remainder == 0)
    {
        return value;
    }

    return value + multiple - remainder;
}

Vec2i ReshapeExtent(Vec2i extent);

Vec2f Hammersley(uint32 sampleIndex, uint32 numSamples);

Vec3f RandomInSphere(Vec3f rnd);
Vec3f RandomInHemisphere(Vec3f rnd, Vec3f n);

Vec2f VogelDisk(uint32 sampleIndex, uint32 numSamples, float phi);

Vec3f ImportanceSampleGGX(Vec2f xi, Vec3f n, float roughness);

Vec3f CalculateBarycentricCoordinates(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2, const Vec3f& p);
Vec3f CalculateBarycentricCoordinates(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2, const Vec2f& p);

void ComputeOrthonormalBasis(const Vec3f& normal, Vec3f& outTangent, Vec3f& outBitangent);

Vec2f EncodeOctahedralCoord(const Vec3f& in);
Vec3f DecodeOctahedralCoord(const Vec2f& in);
Vec2f NormalizeOctahedralCoord(const Vec2i& coord, const Vec2i& extent);

}; // namespace MathUtil

} // namespace Hyperion
