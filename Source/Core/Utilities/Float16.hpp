/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Utilities/Span.hpp>

#include <bit>
#include <type_traits> // For std::is_constant_evaluated

#if !HYP_ARM && (defined(__F16C__) || (defined(_MSC_VER) && defined(__AVX2__)))
#define HYP_FLOAT16_F16C 1
#include <immintrin.h>
#elif !HYP_ARM && (defined(__SSE2__) || defined(_M_X64))
#define HYP_FLOAT16_SSE2 1
#include <emmintrin.h>
#elif defined(__aarch64__)
#define HYP_FLOAT16_NEON 1
#include <arm_neon.h>
#endif

#ifndef HYP_FLOAT16_F16C
#define HYP_FLOAT16_F16C 0
#endif

#ifndef HYP_FLOAT16_SSE2
#define HYP_FLOAT16_SSE2 0
#endif

#ifndef HYP_FLOAT16_NEON
#define HYP_FLOAT16_NEON 0
#endif

#if HYP_FLOAT16_NEON && defined(__ARM_FP16_FORMAT_IEEE)
#define HYP_FLOAT16_NATIVE_SCALAR 1
#else
#define HYP_FLOAT16_NATIVE_SCALAR 0
#endif

namespace Hyperion {

/*! \brief A 16-bit floating point number. */
struct alignas(2) Float16
{
    uint16 value;

    Float16() = default;

    constexpr Float16(float floatValue)
        : value(0)
    {
        if (!std::is_constant_evaluated())
        {
#if HYP_FLOAT16_F16C
            this->value = uint16(_mm_cvtsi128_si32(_mm_cvtps_ph(_mm_set_ss(floatValue), _MM_FROUND_TO_NEAREST_INT)));
            return;
#elif HYP_FLOAT16_NATIVE_SCALAR
            this->value = std::bit_cast<uint16>(static_cast<__fp16>(floatValue));
            return;
#endif
        }

        this->value = FloatBitsToHalf(std::bit_cast<uint32>(floatValue));
    }

    constexpr operator float() const
    {
        if (!std::is_constant_evaluated())
        {
#if HYP_FLOAT16_F16C
            return _mm_cvtss_f32(_mm_cvtph_ps(_mm_cvtsi32_si128(int(this->value))));
#elif HYP_FLOAT16_NATIVE_SCALAR
            return static_cast<float>(std::bit_cast<__fp16>(this->value));
#endif
        }

        return HalfToFloat(this->value);
    }

    HYP_FORCE_INLINE constexpr Float16 operator+(Float16 other) const
    {
        return Float16(float(*this) + float(other));
    }

    HYP_FORCE_INLINE constexpr Float16 operator-(Float16 other) const
    {
        return Float16(float(*this) - float(other));
    }

    HYP_FORCE_INLINE constexpr Float16 operator*(Float16 other) const
    {
        return Float16(float(*this) * float(other));
    }

    HYP_FORCE_INLINE constexpr Float16 operator/(Float16 other) const
    {
        return Float16(float(*this) / float(other));
    }

    HYP_FORCE_INLINE Float16& operator+=(Float16 other)
    {
        *this = *this + other;
        return *this;
    }

    HYP_FORCE_INLINE Float16& operator-=(Float16 other)
    {
        *this = *this - other;
        return *this;
    }

    HYP_FORCE_INLINE Float16& operator*=(Float16 other)
    {
        *this = *this * other;
        return *this;
    }

    HYP_FORCE_INLINE Float16& operator/=(Float16 other)
    {
        *this = *this / other;
        return *this;
    }

    HYP_FORCE_INLINE constexpr Float16 operator-() const
    {
        return FromRaw(uint16(this->value ^ 0x8000u));
    }

    HYP_FORCE_INLINE constexpr bool operator==(Float16 other) const
    {
        return float(*this) == float(other);
    }

    HYP_FORCE_INLINE constexpr bool operator!=(Float16 other) const
    {
        return float(*this) != float(other);
    }

    HYP_FORCE_INLINE constexpr bool operator<(Float16 other) const
    {
        return float(*this) < float(other);
    }

    HYP_FORCE_INLINE constexpr bool operator<=(Float16 other) const
    {
        return float(*this) <= float(other);
    }

    HYP_FORCE_INLINE constexpr bool operator>(Float16 other) const
    {
        return float(*this) > float(other);
    }

    HYP_FORCE_INLINE constexpr bool operator>=(Float16 other) const
    {
        return float(*this) >= float(other);
    }

    HYP_FORCE_INLINE Float16 operator++(int)
    {
        Float16 result = *this;
        *this += 1.0f;
        return result;
    }

    HYP_FORCE_INLINE Float16 operator--(int)
    {
        Float16 result = *this;
        *this -= 1.0f;
        return result;
    }

    HYP_FORCE_INLINE Float16& operator++()
    {
        *this += 1.0f;
        return *this;
    }

    HYP_FORCE_INLINE Float16& operator--()
    {
        *this -= 1.0f;
        return *this;
    }

    HYP_FORCE_INLINE constexpr uint16 Raw() const
    {
        return value;
    }

    HYP_FORCE_INLINE static constexpr Float16 FromRaw(uint16 v)
    {
        Float16 result {};
        result.value = v;
        return result;
    }

    HYP_FORCE_INLINE static void ToFloat4(const Float16* src, float* dst)
    {
#if HYP_FLOAT16_F16C
        _mm_storeu_ps(dst, _mm_cvtph_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(src))));
#elif HYP_FLOAT16_SSE2
        _mm_storeu_ps(dst, HalvesToFloatsSSE2(_mm_unpacklo_epi16(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(src)), _mm_setzero_si128())));
#elif HYP_FLOAT16_NEON
        vst1q_f32(dst, vcvt_f32_f16(vreinterpret_f16_u16(vld1_u16(reinterpret_cast<const uint16_t*>(src)))));
#else
        for (uint32 index = 0; index < 4; index++)
        {
            dst[index] = HalfToFloat(src[index].value);
        }
#endif
    }

    HYP_FORCE_INLINE static void FromFloat4(const float* src, Float16* dst)
    {
#if HYP_FLOAT16_F16C
        _mm_storel_epi64(reinterpret_cast<__m128i*>(dst), _mm_cvtps_ph(_mm_loadu_ps(src), _MM_FROUND_TO_NEAREST_INT));
#elif HYP_FLOAT16_SSE2
        const __m128i halves = FloatsToHalvesSSE2(_mm_loadu_ps(src));
        _mm_storel_epi64(reinterpret_cast<__m128i*>(dst), _mm_packs_epi32(halves, halves));
#elif HYP_FLOAT16_NEON
        vst1_u16(reinterpret_cast<uint16_t*>(dst), vreinterpret_u16_f16(vcvt_f16_f32(vld1q_f32(src))));
#else
        for (uint32 index = 0; index < 4; index++)
        {
            dst[index].value = FloatBitsToHalf(std::bit_cast<uint32>(src[index]));
        }
#endif
    }

    static void ToFloatArray(Span<const Float16> src, Span<float> dst)
    {
        const size_t count = src.Size() < dst.Size() ? src.Size() : dst.Size();

        const Float16* srcData = src.Data();
        float* dstData = dst.Data();

        size_t index = 0;

#if HYP_FLOAT16_F16C && defined(__AVX__)
        for (; index + 8 <= count; index += 8)
        {
            _mm256_storeu_ps(dstData + index, _mm256_cvtph_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(srcData + index))));
        }
#endif

        for (; index + 4 <= count; index += 4)
        {
            ToFloat4(srcData + index, dstData + index);
        }

        for (; index < count; index++)
        {
            dstData[index] = float(srcData[index]);
        }
    }

    static void FromFloatArray(Span<const float> src, Span<Float16> dst)
    {
        const size_t count = src.Size() < dst.Size() ? src.Size() : dst.Size();

        const float* srcData = src.Data();
        Float16* dstData = dst.Data();

        size_t index = 0;

#if HYP_FLOAT16_F16C && defined(__AVX__)
        for (; index + 8 <= count; index += 8)
        {
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dstData + index), _mm256_cvtps_ph(_mm256_loadu_ps(srcData + index), _MM_FROUND_TO_NEAREST_INT));
        }
#endif

        for (; index + 4 <= count; index += 4)
        {
            FromFloat4(srcData + index, dstData + index);
        }

        for (; index < count; index++)
        {
            dstData[index] = Float16(srcData[index]);
        }
    }

    static constexpr uint16 FloatBitsToHalf(uint32 bits)
    {
        constexpr uint32 roundsToInfinity = (127 + 16) << 23;
        constexpr uint32 smallestNormal = (127 - 14) << 23;
        constexpr uint32 subnormalMagic = ((127 - 15) + (23 - 10) + 1) << 23;

        const uint32 sign = bits & 0x80000000u;
        bits ^= sign;

        uint32 result;

        if (bits >= roundsToInfinity)
        {
            result = bits > 0x7F800000u ? 0x7E00u : 0x7C00u;
        }
        else if (bits < smallestNormal)
        {
            // adding the magic value lines the 10 mantissa bits up at the bottom, rounded by the FPU
            result = std::bit_cast<uint32>(std::bit_cast<float>(bits) + std::bit_cast<float>(subnormalMagic)) - subnormalMagic;
        }
        else
        {
            const uint32 mantissaOdd = (bits >> 13) & 1u;
            result = (bits + (uint32(15 - 127) << 23) + 0xFFFu + mantissaOdd) >> 13;
        }

        return uint16(result | (sign >> 16));
    }

    static constexpr float HalfToFloat(uint16 half)
    {
        constexpr uint32 shiftedExponent = 0x7C00u << 13;
        constexpr uint32 subnormalMagic = 113u << 23;

        uint32 bits = uint32(half & 0x7FFFu) << 13;
        const uint32 exponent = bits & shiftedExponent;

        bits += uint32(127 - 15) << 23;

        if (exponent == shiftedExponent)
        {
            bits += uint32(128 - 16) << 23;
        }
        else if (exponent == 0)
        {
            bits = std::bit_cast<uint32>(std::bit_cast<float>(bits + (1u << 23)) - std::bit_cast<float>(subnormalMagic));
        }

        return std::bit_cast<float>(bits | (uint32(half & 0x8000u) << 16));
    }

private:
#if HYP_FLOAT16_SSE2
    static HYP_FORCE_INLINE __m128 HalvesToFloatsSSE2(__m128i halves)
    {
        const __m128i shiftedExponent = _mm_set1_epi32(0x7C00 << 13);

        const __m128i expMantissa = _mm_and_si128(halves, _mm_set1_epi32(0x7FFF));
        const __m128i sign = _mm_slli_epi32(_mm_xor_si128(halves, expMantissa), 16);

        __m128i bits = _mm_slli_epi32(expMantissa, 13);
        const __m128i exponent = _mm_and_si128(bits, shiftedExponent);

        bits = _mm_add_epi32(bits, _mm_set1_epi32((127 - 15) << 23));

        const __m128i isInfNan = _mm_cmpeq_epi32(exponent, shiftedExponent);
        bits = _mm_add_epi32(bits, _mm_and_si128(isInfNan, _mm_set1_epi32((128 - 16) << 23)));

        const __m128 isSubnormal = _mm_castsi128_ps(_mm_cmpeq_epi32(exponent, _mm_setzero_si128()));
        const __m128 renormalized = _mm_sub_ps(
            _mm_castsi128_ps(_mm_add_epi32(bits, _mm_set1_epi32(1 << 23))),
            _mm_castsi128_ps(_mm_set1_epi32(113 << 23)));

        const __m128 result = _mm_or_ps(_mm_and_ps(isSubnormal, renormalized), _mm_andnot_ps(isSubnormal, _mm_castsi128_ps(bits)));

        return _mm_or_ps(result, _mm_castsi128_ps(sign));
    }

    static HYP_FORCE_INLINE __m128i FloatsToHalvesSSE2(__m128 floats)
    {
        const __m128i subnormalMagic = _mm_set1_epi32(((127 - 15) + (23 - 10) + 1) << 23);

        const __m128 justSign = _mm_and_ps(floats, _mm_castsi128_ps(_mm_set1_epi32(int(0x80000000u))));
        const __m128 absFloats = _mm_xor_ps(floats, justSign);
        const __m128i absBits = _mm_castps_si128(absFloats);

        const __m128i isNan = _mm_castps_si128(_mm_cmpunord_ps(absFloats, absFloats));
        const __m128i isRegular = _mm_cmpgt_epi32(_mm_set1_epi32((127 + 16) << 23), absBits);
        const __m128i infOrNan = _mm_or_si128(_mm_and_si128(isNan, _mm_set1_epi32(0x200)), _mm_set1_epi32(0x7C00));

        const __m128i isSubnormal = _mm_cmpgt_epi32(_mm_set1_epi32((127 - 14) << 23), absBits);
        const __m128i subnormal = _mm_sub_epi32(_mm_castps_si128(_mm_add_ps(absFloats, _mm_castsi128_ps(subnormalMagic))), subnormalMagic);

        const __m128i mantissaOdd = _mm_srai_epi32(_mm_slli_epi32(absBits, 31 - 13), 31);
        const __m128i normal = _mm_srli_epi32(_mm_sub_epi32(_mm_add_epi32(absBits, _mm_set1_epi32(int(0xFFFu - (uint32(127 - 15) << 23)))), mantissaOdd), 13);

        const __m128i nonSpecial = _mm_or_si128(_mm_and_si128(isSubnormal, subnormal), _mm_andnot_si128(isSubnormal, normal));
        const __m128i joined = _mm_or_si128(_mm_and_si128(isRegular, nonSpecial), _mm_andnot_si128(isRegular, infOrNan));

        return _mm_or_si128(joined, _mm_srai_epi32(_mm_castps_si128(justSign), 16));
    }
#endif
};

static_assert(sizeof(Float16) == 2, "float16 must be 2 bytes in size");

#ifndef FLT16_MAX
#define FLT16_MAX Float16::FromRaw(0x7BFF)
#endif

#ifndef FLT16_MIN
#define FLT16_MIN Float16::FromRaw(1)
#endif

} // namespace Hyperion
