/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <bit>
#include <type_traits> // For std::is_constant_evaluated

// Conditionally enable hardware acceleration for x86/x64 F16C instructions
#if defined(__F16C__) || (defined(_MSC_VER) && defined(__AVX2__))
#define HYP_USE_F16C
#include <immintrin.h>

#if defined(_MSC_VER) && defined(__AVX2__)
inline float _cvtsh_ss(unsigned short h)
{
    __m128i v = _mm_set1_epi16(h);
    __m128 f = _mm_cvtph_ps(v);
    return _mm_cvtss_f32(f);
}

inline unsigned short _cvtss_sh(float f, int /* ignored */)
{
    __m128 vec = _mm_set_ss(f);
    __m128i packed = _mm_cvtps_ph(vec, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
    return _mm_extract_epi16(packed, 0);
}
#endif // _MSC_VER && __AVX2__

#endif // __F16C__ || (_MSC_VER && __AVX2__)

namespace Hyperion {

/*! \brief A 16-bit floating point number. */
struct alignas(2) Float16
{
    uint16 value;

    Float16() = default;

    constexpr Float16(float floatValue)
        : value(0)
    {
#ifdef HYP_USE_F16C
        if (!std::is_constant_evaluated())
        {
            this->value = _cvtss_sh(floatValue, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
            return;
        }
#endif

        constexpr uint32 signMask = 0x80000000;
        constexpr uint32 expMask = 0x7F800000;
        constexpr uint32 fracMask = 0x007FFFFF;

        const uint32 floatBits = ::std::bit_cast<uint32>(floatValue);
        const uint32 sign = (floatBits & signMask) >> 16;

        int32 originalExp = (floatBits & expMask) >> 23;
        uint32 fraction = floatBits & fracMask;

        int32 exponent = originalExp - 127 + 15;

        if (originalExp == 255)
        {
            exponent = 31;
            fraction = (fraction != 0) ? 0x200 : 0;
        }
        else if (exponent >= 31)
        {
            exponent = 31;
            fraction = 0;
        }
        else if (exponent <= 0)
        {
            if (exponent < -10)
            {
                exponent = 0;
                fraction = 0;
            }
            else
            {
                fraction = (fraction | 0x00800000);
                
                uint32 shift = 1 - exponent;

                fraction >>= shift;
                exponent = 0;
            }
        }

        if (exponent != 31)
        {
            uint32 remainder = fraction & 0x1FFF;
            fraction >>= 13;

            if (remainder > 0x1000 || (remainder == 0x1000 && (fraction & 1)))
            {
                fraction++;

                if (fraction >= 0x400)
                {
                    fraction = 0;
                    exponent++;
                }
            }
        }
        else
        {
            fraction >>= 13;
        }

        this->value = static_cast<uint16>(sign | (exponent << 10) | fraction);
    }

    constexpr operator float() const
    {
#ifdef HYP_USE_F16C
        if (!std::is_constant_evaluated())
        {
            return _cvtsh_ss(this->value);
        }
#endif

        constexpr uint32 signMask = 0x8000;
        constexpr uint32 expMask = 0x7C00;
        constexpr uint32 fracMask = 0x03FF;

        uint32 sign = (this->value & signMask) << 16;
        int32 exponent = (this->value & expMask) >> 10;
        uint32 fraction = (this->value & fracMask) << 13;

        if (exponent == 0)
        {
            if (fraction == 0)
            {
                exponent = 0;
            }
            else
            {
                int32 e = -14;
                while ((fraction & (1 << 23)) == 0)
                {
                    fraction <<= 1;
                    e--;
                }
                fraction &= ~(1 << 23);
                exponent = e + 127;
            }
        }
        else if (exponent == 31)
        {
            exponent = 255;
        }
        else
        {
            exponent = exponent - 15 + 127;
        }

        uint32 floatBits = sign | (exponent << 23) | fraction;
        return ::std::bit_cast<float>(floatBits);
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
        return Float16(-float(*this));
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
};

static_assert(sizeof(Float16) == 2, "float16 must be 2 bytes in size");

#ifndef FLT16_MAX
#define FLT16_MAX Float16::FromRaw(0x7BFF)
#endif

#ifndef FLT16_MIN
#define FLT16_MIN Float16::FromRaw(1)
#endif

} // namespace Hyperion