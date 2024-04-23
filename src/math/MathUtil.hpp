/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MATHUTIL_HPP
#define HYPERION_MATHUTIL_HPP

#include <math/Vector2.hpp>
#include <math/Vector3.hpp>
#include <math/Vector4.hpp>

#include <core/Util.hpp>

#include <core/Defines.hpp>
#include <Types.hpp>

#include <math.h>
#include <cstdlib>
#include <cmath>
#include <cfloat>
#include <limits>
#include <type_traits>

#if HYP_WINDOWS
#include <intrin.h>
#endif

namespace hyperion {

template <class T>
constexpr bool is_math_vector_v = is_vec2<T> || is_vec3<T> || is_vec4<T>;

class HYP_API MathUtil
{
    static uint64 g_seed;

public:
    template <class T>
    static constexpr T pi = T(3.14159265358979);

    static constexpr float epsilon_f = FLT_EPSILON;

    static constexpr float epsilon_d = DBL_EPSILON;

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(is_math_vector_v<T>, T) MaxSafeValue()
        { return T(MaxSafeValue<std::remove_all_extents_t<decltype(T::values)>>()); }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(is_math_vector_v<T>, T) MinSafeValue()
        { return T(MinSafeValue<std::remove_all_extents_t<decltype(T::values)>>()); }
    
    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(std::is_enum_v<T> && !is_math_vector_v<T>, std::underlying_type_t<T>) MaxSafeValue()
        { return std::numeric_limits<std::underlying_type_t<T>>::max(); }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(std::is_enum_v<T> && !is_math_vector_v<T>, std::underlying_type_t<T>) MinSafeValue()
        { return std::numeric_limits<std::underlying_type_t<T>>::lowest(); }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(!std::is_enum_v<T> && !is_math_vector_v<T>, T) MaxSafeValue()
        { return std::numeric_limits<T>::max(); }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(!std::is_enum_v<T> && !is_math_vector_v<T>, T) MinSafeValue()
        { return std::numeric_limits<T>::lowest(); }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr auto MaxSafeValue(T)
        { return MaxSafeValue<T>(); }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr auto MinSafeValue(T)
        { return MinSafeValue<T>(); }
    
    HYP_FORCE_INLINE
    static Vec2f SafeValue(const Vec2f &value)
        { return Vec2f::Max(Vec2f::Min(value, MaxSafeValue<decltype(value[0])>()), MinSafeValue<decltype(value[0])>()); }

    HYP_FORCE_INLINE
    static Vec3f SafeValue(const Vec3f &value)
        { return Vec3f::Max(Vec3f::Min(value, MaxSafeValue<decltype(value[0])>()), MinSafeValue<decltype(value[0])>()); }

    HYP_FORCE_INLINE
    static Vec4f SafeValue(const Vec4f &value)
        { return Vec4f::Max(Vector4::Min(value, MaxSafeValue<decltype(value[0])>()), MinSafeValue<decltype(value[0])>()); }

    template <class T>
    HYP_FORCE_INLINE
    static T SafeValue(const T &value)
        { return MathUtil::Max(MathUtil::Min(value, MathUtil::MaxSafeValue<T>()), MathUtil::MinSafeValue<T>()); }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(std::is_floating_point_v<T>, T) NaN()
        { return std::numeric_limits<T>::quiet_NaN(); }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(is_math_vector_v<T> && std::is_floating_point_v<NormalizedType<decltype(T::values[0])>>, T) NaN()
    {
        using VectorScalarType = NormalizedType<decltype(T::values[0])>;

        T result { }; /* doesn't need initialization but gets rid of annoying warnings */

        for (uint i = 0; i < ArraySize(result.values); i++) {
            result.values[i] = VectorScalarType(NaN<VectorScalarType>());
        }

        return result;
    }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(std::is_floating_point_v<T>, bool) IsNaN(T value)
        { return value != value; }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(is_math_vector_v<T> && std::is_floating_point_v<NormalizedType<decltype(T::values[0])>>, bool) IsNaN(const T &value)
    {
        for (uint i = 0; i < ArraySize(value.values); i++) {
            if (IsNaN(value.values[i])) {
                return true;
            }
        }

        return false;
    }

    template <class T>
    static constexpr HYP_ENABLE_IF(std::is_floating_point_v<T>, T) Infinity()
        { return std::numeric_limits<T>::infinity(); }

    template <class T>
    static constexpr HYP_ENABLE_IF(is_math_vector_v<T> && std::is_floating_point_v<NormalizedType<decltype(T::values[0])>>, T) Infinity()
    {
        using VectorScalarType = NormalizedType<decltype(T::values[0])>;

        T result { }; /* doesn't need initialization but gets rid of annoying warnings */

        for (uint i = 0; i < ArraySize(result.values); i++) {
            result.values[i] = VectorScalarType(Infinity<VectorScalarType>());
        }

        return result;
    }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(std::is_floating_point_v<T>, bool) IsFinite(T value)
        { return value != Infinity<T>() && value != -Infinity<T>(); }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(is_math_vector_v<T> && std::is_floating_point_v<NormalizedType<decltype(T::values[0])>>, bool) IsFinite(const T &value)
    {
        for (uint i = 0; i < ArraySize(value.values); i++) {
            if (!IsFinite(value.values[i])) {
                return false;
            }
        }

        return true;
    }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(is_math_vector_v<T>, bool) ApproxEqual(const T &a, const T &b)
        { return a.DistanceSquared(b) < std::is_same_v<std::remove_all_extents_t<decltype(T::values)>, double> ? epsilon_d : epsilon_f; }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(!is_math_vector_v<T>, bool) ApproxEqual(T a, T b, T eps = std::is_same_v<T, double> ? epsilon_d : epsilon_f)
        { return Abs(a - b) <= eps; }

    template <class T>
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) RandRange(const T &a, const T &b)
    {
        T result;

        for (uint i = 0; i < ArraySize(result.values); i++) {
            result.values[i] = RandRange(a.values[i], b.values[i]);
        }

        return result;
    }

    template <class T>
    static HYP_ENABLE_IF(!is_math_vector_v<T>, T) RandRange(T a, T b)
    {
        const auto random = T(rand()) / T(RAND_MAX);
        const auto diff = b - a;
        const auto r = random * diff;

        return a + r;
    }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr T RadToDeg(T rad)
        { return rad * T(180) / pi<T>; }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr T DegToRad(T deg)
        { return deg * pi<T> / T(180); }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(!is_math_vector_v<T>, T) Clamp(T val, T min, T max)
    {
        if (val > max) {
            return max;
        } else if (val < min) {
            return min;
        } else {
            return val;
        }
    }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(is_math_vector_v<T>, T) Clamp(const T &val, const T &min, const T &max)
    {
        T result;

        for (uint i = 0; i < ArraySize(result.values); i++) {
            result.values[i] = Clamp(val.values[i], min.values[i], max.values[i]);
        }

        return result;
    }

    template <class T, class U, class V>
    HYP_FORCE_INLINE
    static constexpr auto Lerp(const T &from, const U &to, const V &amt) -> std::enable_if_t<!is_math_vector_v<T>, decltype(from + amt * (to - from))>
        { return from + amt * (to - from); }

    template <class T, class U, class V>
    HYP_FORCE_INLINE
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) Lerp(const T &from, const U &to, const V &amt)
    {
        T result;

        for (uint i = 0; i < ArraySize(result.values); i++) {
            result.values[i] = Lerp(from.values[i], to.values[i], amt);
        }

        return result;
    }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(!is_math_vector_v<T>, T) Step(T edge, T x)
        { return x < edge ? 0.0f : 1.0f; }

    template <class T>
    HYP_FORCE_INLINE
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) Step(const T &edge, const T &x)
    {
        T result;

        for (uint i = 0; i < ArraySize(result.values); i++) {
            result.values[i] = Step(edge.values[i], x.values[i]);
        }

        return result;
    }

    template <class T, class U, class V = std::common_type_t<T, U>>
    HYP_FORCE_INLINE
    static constexpr V Min(T a, U b)
        { return (a < b) ? a : b; }

    template <class T, class U, class V = std::common_type_t<T, U>, class ...Args>
    HYP_FORCE_INLINE
    static constexpr V Min(T a, U b, Args... args)
        { return Min(Min(a, b), args...); }
    
    template <class T, class U, class V = std::common_type_t<T, U>>
    HYP_FORCE_INLINE
    static constexpr V Max(T a, U b)
        { return (a > b) ? a : b; }

    template <class T, class U, class V = std::common_type_t<T, U>, class ...Args>
    HYP_FORCE_INLINE
    static constexpr V Max(T a, U b, Args... args)
        { return Max(Max(a, b), args...); }

    template <class T>
    HYP_FORCE_INLINE
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) Min(const T &a, const T &b)
    {
        T result;

        for (uint i = 0; i < ArraySize(result.values); i++) {
            result.values[i] = Min(a.values[i], b.values[i]);
        }

        return result;
    }

    template <class T>
    HYP_FORCE_INLINE
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) Max(const T &a, const T &b)
    {
        T result;

        for (uint i = 0; i < ArraySize(result.values); i++) {
            result.values[i] = Max(a.values[i], b.values[i]);
        }

        return result;
    }

    template <class T, class IntegralType = int>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(!is_math_vector_v<T>, IntegralType) Sign(T value)
        { return IntegralType(T(0) < value) - IntegralType(value < T(0)); }

    template <class T>
    HYP_FORCE_INLINE
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) Sign(const T &a)
    {
        using VectorScalarType = NormalizedType<decltype(T::values[0])>;

        T result { };

        for (uint i = 0; i < ArraySize(result.values); i++) {
            result.values[i] = VectorScalarType(Sign<VectorScalarType>(a.values[i]));
        }

        return result;
    }

    template <class T, class IntegralType = std::conditional_t<std::is_integral_v<T>, T, int>>
    HYP_FORCE_INLINE
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) Trunc(const T &a)
    {
        using VectorScalarType = NormalizedType<decltype(T::values[0])>;

        T result { }; /* doesn't need initialization but gets rid of annoying warnings */

        for (uint i = 0; i < ArraySize(result.values); i++) {
            result.values[i] = VectorScalarType(Trunc<VectorScalarType, IntegralType>(a.values[i]));
        }

        return result;
    }

    template <class T, class IntegralType = std::conditional_t<std::is_integral_v<T>, T, int>>
    HYP_FORCE_INLINE
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) Floor(const T &a)
    {
        using VectorScalarType = NormalizedType<decltype(T::values[0])>;

        T result { }; /* doesn't need initialization but gets rid of annoying warnings */

        for (uint i = 0; i < ArraySize(result.values); i++) {
            result.values[i] = VectorScalarType(Floor<VectorScalarType, IntegralType>(a.values[i]));
        }

        return result;
    }

    template <class T, class IntegralType = std::conditional_t<std::is_integral_v<T>, T, int>>
    HYP_FORCE_INLINE
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) Ceil(const T &a)
    {
        using VectorScalarType = NormalizedType<decltype(T::values[0])>;

        T result { }; /* doesn't need initialization but gets rid of annoying warnings */

        for (uint i = 0; i < ArraySize(result.values); i++) {
            result.values[i] = VectorScalarType(Ceil<VectorScalarType, IntegralType>(a.values[i]));
        }

        return result;
    }

    template <class T, class IntegralType = std::conditional_t<std::is_integral_v<T>, T, int>>
    HYP_FORCE_INLINE
    static HYP_ENABLE_IF(!is_math_vector_v<T>, IntegralType) Trunc(T a)
        { return IntegralType(std::trunc(a)); }

    template <class T, class IntegralType = std::conditional_t<std::is_integral_v<T>, T, int>>
    HYP_FORCE_INLINE
    static HYP_ENABLE_IF(!is_math_vector_v<T>, IntegralType) Floor(T a)
        { return IntegralType(std::floor(a)); }

    template <class T, class IntegralType = std::conditional_t<std::is_integral_v<T>, T, int>>
    HYP_FORCE_INLINE
    static HYP_ENABLE_IF(!is_math_vector_v<T>, IntegralType) Ceil(T a)
        { return IntegralType(std::ceil(a)); }

    template <class T>
    HYP_FORCE_INLINE
    static T Fract(T a)
        { return a - Floor<T, T>(a); }

    template <class T>
    HYP_FORCE_INLINE
    static T Exp(T a)
        { return T(std::exp(a)); }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr T Mod(T a, T b)
        { return (a % b + b) % b; }
    
    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(!is_math_vector_v<T>, T) Abs(T a)
        { return a > T(0) ? a : -a; }
    
    template <class T>
    HYP_FORCE_INLINE
    static constexpr HYP_ENABLE_IF(is_math_vector_v<T>, T) Abs(const T &a)
    {
        using VectorScalarType = NormalizedType<decltype(T::values[0])>;

        T result { }; /* doesn't need initialization but gets rid of annoying warnings */

        for (uint i = 0; i < ArraySize(result.values); i++) {
            result.values[i] = VectorScalarType(Abs<VectorScalarType>(a.values[i]));
        }

        return result;
    }

    template <class T, class U = T>
    HYP_FORCE_INLINE
    static HYP_ENABLE_IF(!is_math_vector_v<T>, U) Round(T a)
        { return U(std::round(a)); }

    template <class T>
    HYP_FORCE_INLINE
    static HYP_ENABLE_IF(is_math_vector_v<T>, T) Round(const T &a)
        { return T::Round(a); }

    static float Sin(float x) { return sinf(x); }
    static double Sin(double x) { return sin(x); }
    static float Arcsin(float x) { return asinf(x); }
    static double Arcsin(double x) { return asin(x); }

    static float Cos(float x) { return cosf(x); }
    static double Cos(double x) { return cos(x); }
    static float Arccos(float x) { return acosf(x); }
    static double Arccos(double x) { return acos(x); }

    static float Tan(float x) { return tanf(x); }
    static double Tan(double x) { return tan(x); }
    static float Arctan(float x) { return atanf(x); }
    static double Arctan(double x) { return atan(x); }

    template <class T>
    static constexpr T Factorial(T value)
    {
        T f = value;

        if (f > 1) {
            for (T i = value - 1; i >= 1; --i) {
                f *= i;
            }
        }
        
        return f;
    }

    template <class T, class U = T, class V = U>
    HYP_FORCE_INLINE
    static constexpr bool InRange(T value, const std::pair<U, V> &range)
        { return value >= range.first && value < range.second; }

    template <class T, class U = T>
    HYP_FORCE_INLINE
    static constexpr U Sqrt(T value)
    {
        if constexpr (std::is_same_v<U, double>) {
            return sqrt(static_cast<double>(value));
        } else if constexpr (std::is_same_v<U, float>) {
            return sqrtf(static_cast<float>(value));
        } else {
            return static_cast<U>(sqrtf(static_cast<float>(value)));
        }
    }

    template <class T, class U = T>
    HYP_FORCE_INLINE
    static constexpr U Pow(T value, T exponent)
    {
        if constexpr (std::is_same_v<U, double>) {
            return pow(static_cast<double>(value), static_cast<double>(exponent));
        } else if constexpr (std::is_same_v<U, float>) {
            return powf(static_cast<float>(value), static_cast<float>(exponent));
        } else {
            return static_cast<U>(powf(static_cast<float>(value), static_cast<float>(exponent)));
        }
    }

    template <class T>
    HYP_FORCE_INLINE
    static constexpr bool IsPowerOfTwo(T value)
        { return (value & (value - 1)) == 0; }

    /* Fast log2 for power-of-2 numbers */
    static constexpr uint64 FastLog2_Pow2(uint64 value)
    {
        if (std::is_constant_evaluated()) {
            return FastLog2(value);
        }

#if defined(__clang__) || defined(__GNUC__)
    #if defined(_MSC_VER)
        return FastLog2(value); // fallback :/
    #else
        return __builtin_ctzll(value);
    #endif
#elif defined(_MSC_VER)
        return _tzcnt_u64(value);
#else
        return __builtin_ctzll(value);
#endif
    }

    // https://stackoverflow.com/questions/11376288/fast-computing-of-log2-for-64-bit-integers
    static inline constexpr uint64 FastLog2(uint64 value)
    {
        const int tab64[64] = {
            63,  0, 58,  1, 59, 47, 53,  2,
            60, 39, 48, 27, 54, 33, 42,  3,
            61, 51, 37, 40, 49, 18, 28, 20,
            55, 30, 34, 11, 43, 14, 22,  4,
            62, 57, 46, 52, 38, 26, 32, 41,
            50, 36, 17, 19, 29, 10, 13, 21,
            56, 45, 25, 31, 35, 16,  9, 12,
            44, 24, 15,  8, 23,  7,  6,  5
        };

        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        value |= value >> 32;

        return tab64[(((static_cast<uint64>(value) - (value >> 1)) * 0x07EDD5E59A4E28C2)) >> 58];
    }

    // https://www.techiedelight.com/round-next-highest-power-2/
    static inline constexpr uint64 NextPowerOf2(uint64 value)
    {
        // decrement `n` (to handle the case when `n` itself
        // is a power of 2)
        value = value - 1;

        // next power of two will have a bit set at position `lg+1`.
        return 1ull << (FastLog2(value) + 1);
    }

    static inline constexpr uint64 PreviousPowerOf2(uint64 value)
    {
        // dumb implementation
        uint64 result = 1;

        while (result * 2 < value) {
            result *= 2;
        }

        return result;

        //return NextPowerOf2(value) / 2;
    }

    template <class T, class U>
    static inline constexpr auto NextMultiple(T &&value, U &&multiple) -> std::common_type_t<T, U>
    {
        if (multiple == 0) {
            return value;
        }

        auto remainder = value % multiple;

        if (remainder == 0) {
            return value;
        }

        return value + multiple - remainder;
    }

    static Vec3f RandomInSphere(Vec3f rnd);
    static Vec3f RandomInHemisphere(Vec3f rnd, Vec3f n);
    static Vec2f VogelDisk(uint sample_index, uint num_samples, float phi);
    static Vec3f ImportanceSampleGGX(Vec2f Xi, Vec3f N, float roughness);
    static Vec3f CalculateBarycentricCoordinates(const Vec3f &v0, const Vec3f &v1, const Vec3f &v2, const Vec3f &p);
    static Vec3f CalculateBarycentricCoordinates(const Vec2f &v0, const Vec2f &v1, const Vec2f &v2, const Vec2f &p);
    static void ComputeOrthonormalBasis(const Vec3f &normal, Vec3f &out_tangent, Vec3f &out_bitangent);
};

} // namespace hyperion

#endif
