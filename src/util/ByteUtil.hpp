/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BYTE_UTIL_H
#define HYPERION_BYTE_UTIL_H

#include <math/MathUtil.hpp>
#include <math/Vector4.hpp>

#include <Types.hpp>

#include <util/Defines.hpp>

namespace hyperion {

struct alignas(2) float16
{
    uint16 value;

    float16() = default;
    
    float16(float value)
    {
        // Bit mask to extract bits from the float
        const uint32 signMask = 0x80000000;
        const uint32 expMask = 0x7F800000;
        const uint32 fracMask = 0x007FFFFF;

        // Extract bits by applying masks and shifting
        uint32 floatBits = *reinterpret_cast<uint32 *>(&value);
        uint32 sign = (floatBits & signMask) >> 16; // Shift sign to bit 15
        int32 exponent = (floatBits & expMask) >> 23; // Get exponent
        uint32 fraction = floatBits & fracMask; // Get fraction

        // Normalize exponent to fit float16 format
        exponent -= 127; // Subtract float32 bias
        exponent += 15; // Add float16 bias

        // Handle overflow/underflow cases
        if (exponent >= 31) {
            // Overflow, set to infinity or max
            exponent = 31;
            fraction = 0;
        } else if (exponent <= 0) {
            // Underflow, handle as subnormal or zero
            if (exponent < -10) {
                // Too small, becomes zero
                exponent = 0;
                fraction = 0;
            } else {
                // Subnormal numbers
                fraction = (fraction | 0x00800000) >> (1 - exponent);
                exponent = 0;
            }
        }

        // Round and clamp fraction to fit 10 bits
        fraction = fraction >> 13; // Shift to fit into 10 bits
        // clamp to 10 bits
        if (fraction > 0x3FF) {
            fraction = 0x3FF;
        }

        // Combine sign, exponent, and fraction into a 16-bit integer
        this->value = (sign | (exponent << 10) | fraction);
    }

    explicit operator float() const
    {
        // Masks for the float16 components
        const uint16 signMask = 0x8000;
        const uint16 expMask = 0x7C00;
        const uint16 fracMask = 0x03FF;

        // Extract the sign, exponent, and fraction from the float16 value
        uint32 sign = (this->value & signMask) << 16; // Shift sign to the correct bit for a 32-bit float
        int32 exponent = (this->value & expMask) >> 10; // Extract and shift exponent to proper position
        uint32 fraction = (this->value & fracMask) << 13; // Shift fraction to align with 32-bit float

        // Adjust the exponent from float16 bias (15) to float32 bias (127)
        if (exponent == 0) {
            if (fraction == 0) {
                // Zero (sign bit only)
                exponent = 0;
            } else {
                // Subnormal number, normalize it
                while ((fraction & (1 << 23)) == 0) {
                    fraction <<= 1;
                    exponent--;
                }
                fraction &= ~(1 << 23); // Clear the leading 1 bit
                exponent += 127; // Adjust bias
            }
        } else if (exponent == 31) {
            // Inf or NaN
            exponent = 255; // Adjust bias for 32-bit float
        } else {
            // Normalized number
            exponent -= 15; // Adjust from float16 exponent bias
            exponent += 127; // Adjust to float32 exponent bias
        }

        // Assemble the bits into a 32-bit integer, interpreting them as a float
        uint32 floatBits = sign | (exponent << 23) | fraction;

        return *reinterpret_cast<float*>(&floatBits);
    }

    float16 operator+(float16 other) const
    {
        return float16(float(*this) + float(other));
    }

    float16 operator-(float16 other) const
    {
        return float16(float(*this) - float(other));
    }

    float16 operator*(float16 other) const
    {
        return float16(float(*this) * float(other));
    }

    float16 operator/(float16 other) const
    {
        return float16(float(*this) / float(other));
    }

    float16 &operator+=(float16 other)
    {
        *this = *this + other;
        return *this;
    }

    float16 &operator-=(float16 other)
    {
        *this = *this - other;
        return *this;
    }

    float16 &operator*=(float16 other)
    {
        *this = *this * other;
        return *this;
    }

    float16 &operator/=(float16 other)
    {
        *this = *this / other;
        return *this;
    }

    float16 operator-() const
    {
        return float16(-float(*this));
    }

    bool operator==(float16 other) const
    {
        return float(*this) == float(other);
    }

    bool operator!=(float16 other) const
    {
        return float(*this) != float(other);
    }

    bool operator<(float16 other) const
    {
        return float(*this) < float(other);
    }

    bool operator<=(float16 other) const
    {
        return float(*this) <= float(other);
    }

    bool operator>(float16 other) const
    {
        return float(*this) > float(other);
    }

    bool operator>=(float16 other) const
    {
        return float(*this) >= float(other);
    }

    float16 operator++(int)
    {
        float16 result = *this;
        *this += 1.0f;
        return result;
    }

    float16 operator--(int)
    {
        float16 result = *this;
        *this -= 1.0f;
        return result;
    }

    float16 &operator++()
    {
        *this += 1.0f;
        return *this;
    }

    float16 &operator--()
    {
        *this -= 1.0f;
        return *this;
    }
};

static_assert(sizeof(float16) == 2, "float16 must be 2 bytes in size");

class HYP_API ByteUtil
{
public:
    static inline uint32 PackFloat(float value)
    {
        union {
            uint32 u;
            float f;
        };

        f = value;

        return u;
    }

    static inline float UnpackFloat(uint32 value)
    {
        union {
            uint32 u;
            float f;
        };

        u = value;

        return f;
    }

    static inline uint32 PackVec4f(const Vector4 &vec)
    {
        union {
            uint8 bytes[4];
            uint32 result;
        };

        bytes[0] = MathUtil::Round<float, uint8>(MathUtil::Clamp(vec.values[0], 0.0f, 1.0f) * 255.0f);
        bytes[1] = MathUtil::Round<float, uint8>(MathUtil::Clamp(vec.values[1], 0.0f, 1.0f) * 255.0f);
        bytes[2] = MathUtil::Round<float, uint8>(MathUtil::Clamp(vec.values[2], 0.0f, 1.0f) * 255.0f);
        bytes[3] = MathUtil::Round<float, uint8>(MathUtil::Clamp(vec.values[3], 0.0f, 1.0f) * 255.0f);

        return result;
    }

    static inline Vec4f UnpackVec4f(uint32 value)
    {
        return {
            float(value & 0xff000000) / 255.0f,
            float(value & 0x00ff0000) / 255.0f,
            float(value & 0x0000ff00) / 255.0f,
            float(value & 0x000000ff) / 255.0f
        };
    }

    template <class T>
    static inline constexpr T AlignAs(T value, uint alignment)
    {
        return ((value + alignment - 1) / alignment) * alignment;
    }

    static uint LowestSetBitIndex(uint64 bits);
};

/*! \brief Converts a value of type \ref{To} to a value of type \ref{From}.
 *  Both types must be standard layout and have the same size.
 *  \tparam To The type to convert to.
 *  \tparam From The type to convert from.
 *  \param from The value to convert.
 *  \return The value of type \ref{From} converted to type \ref{To}.
 */
template <class To, class From>
static HYP_FORCE_INLINE To BitCast(const From &from)
{
    return ValueStorage<To>(&from).Get();
}

#define FOR_EACH_BIT(_num, _iter) for (uint64 num = (_num), _iter = ByteUtil::LowestSetBitIndex(num); _iter != uint(-1); num &= ~_iter)

} // namespace hyperion

#endif