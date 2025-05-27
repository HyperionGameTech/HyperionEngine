/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FLOAT16_HPP
#define HYPERION_FLOAT16_HPP

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {

/*! \brief A 16-bit floating point number. */
struct alignas(2) Float16
{
    uint16 value;

    Float16() = default;

    Float16(float float_value)
    {
        static constexpr uint32 sign_mask = 0x80000000;
        static constexpr uint32 exp_mask = 0x7F800000;
        static constexpr uint32 frac_mask = 0x007FFFFF;

        const uint32 float_bits = *reinterpret_cast<uint32*>(&float_value);
        const uint32 sign = (float_bits & sign_mask) >> 16;

        int32 exponent = (float_bits & exp_mask) >> 23;
        uint32 fraction = float_bits & frac_mask;

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

    explicit operator float() const
    {
        static constexpr uint16 sign_mask = 0x8000;
        static constexpr uint16 exp_mask = 0x7C00;
        static constexpr uint16 frac_mask = 0x03FF;

        uint32 sign = (this->value & sign_mask) << 16;
        int32 exponent = (this->value & exp_mask) >> 10;
        uint32 fraction = (this->value & frac_mask) << 13;

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

        uint32 float_bits = sign | (exponent << 23) | fraction;

        return *reinterpret_cast<float*>(&float_bits);
    }

    HYP_NODISCARD HYP_FORCE_INLINE Float16 operator+(Float16 other) const
    {
        return Float16(float(*this) + float(other));
    }

    HYP_NODISCARD HYP_FORCE_INLINE Float16 operator-(Float16 other) const
    {
        return Float16(float(*this) - float(other));
    }

    HYP_NODISCARD HYP_FORCE_INLINE Float16 operator*(Float16 other) const
    {
        return Float16(float(*this) * float(other));
    }

    HYP_NODISCARD HYP_FORCE_INLINE Float16 operator/(Float16 other) const
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

    HYP_NODISCARD HYP_FORCE_INLINE Float16 operator-() const
    {
        return Float16(-float(*this));
    }

    HYP_NODISCARD HYP_FORCE_INLINE bool operator==(Float16 other) const
    {
        return float(*this) == float(other);
    }

    HYP_NODISCARD HYP_FORCE_INLINE bool operator!=(Float16 other) const
    {
        return float(*this) != float(other);
    }

    HYP_NODISCARD HYP_FORCE_INLINE bool operator<(Float16 other) const
    {
        return float(*this) < float(other);
    }

    HYP_NODISCARD HYP_FORCE_INLINE bool operator<=(Float16 other) const
    {
        return float(*this) <= float(other);
    }

    HYP_NODISCARD HYP_FORCE_INLINE bool operator>(Float16 other) const
    {
        return float(*this) > float(other);
    }

    HYP_NODISCARD HYP_FORCE_INLINE bool operator>=(Float16 other) const
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
};

static_assert(sizeof(Float16) == 2, "float16 must be 2 bytes in size");

} // namespace hyperion

#endif