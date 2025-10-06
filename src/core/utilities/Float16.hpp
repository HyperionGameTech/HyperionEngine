/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/Types.hpp>

#include <bit>

namespace hyperion {

/*! \brief A 16-bit floating point number. */
struct alignas(2) Float16
{
    uint16 value;

    Float16() = default;

    constexpr Float16(float floatValue)
        : value(0)
    {
        constexpr uint32 signMask = 0x80000000;
        constexpr uint32 expMask = 0x7F800000;
        constexpr uint32 fracMask = 0x007FFFFF;

        const uint32 floatBits = ::std::bit_cast<uint32>(floatValue);
        const uint32 sign = (floatBits & signMask) >> 16;

        int32 exponent = (floatBits & expMask) >> 23;
        uint32 fraction = floatBits & fracMask;

        exponent -= 127;
        exponent += 15;

        if (exponent >= 31)
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
                fraction = (fraction | 0x00800000) >> (1 - exponent);
                exponent = 0;
            }
        }

        fraction = fraction >> 13;

        if (fraction > 0x3FF)
        {
            fraction = 0x3FF;
        }

        this->value = (sign | (exponent << 10) | fraction);
    }

    constexpr operator float() const
    {
        constexpr uint16 signMask = 0x8000;
        constexpr uint16 expMask = 0x7C00;
        constexpr uint16 fracMask = 0x03FF;

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
                while ((fraction & (1 << 23)) == 0)
                {
                    fraction <<= 1;
                    exponent--;
                }
                fraction &= ~(1 << 23);
                exponent += 127;
            }
        }
        else if (exponent == 31)
        {
            exponent = 255;
        }
        else
        {
            exponent -= 15;
            exponent += 127;
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
#define FLT16_MAX Float16::FromRaw(65504)
#endif

#ifndef FLT16_MIN
#define FLT16_MIN Float16::FromRaw(1)
#endif

} // namespace hyperion
