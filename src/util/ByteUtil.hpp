/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BYTE_UTIL_HPP
#define HYPERION_BYTE_UTIL_HPP

#include <math/MathUtil.hpp>
#include <math/Vector4.hpp>

#include <Types.hpp>

#include <core/Defines.hpp>

namespace hyperion {

/*! \brief A 16-bit floating point number. */
struct alignas(2) float16
{
    uint16 value;

    float16() = default;
    
    float16(float float_value)
    {
        static constexpr uint32 sign_mask = 0x80000000;
        static constexpr uint32 exp_mask = 0x7F800000;
        static constexpr uint32 frac_mask = 0x007FFFFF;

        const uint32 float_bits = *reinterpret_cast<uint32 *>(&float_value);
        const uint32 sign = (float_bits & sign_mask) >> 16;

        int32 exponent = (float_bits & exp_mask) >> 23;
        uint32 fraction = float_bits & frac_mask;

        exponent -= 127;
        exponent += 15;

        if (exponent >= 31) {
            exponent = 31;
            fraction = 0;
        } else if (exponent <= 0) {
            if (exponent < -10) {
                exponent = 0;
                fraction = 0;
            } else {
                fraction = (fraction | 0x00800000) >> (1 - exponent);
                exponent = 0;
            }
        }

        fraction = fraction >> 13;

        if (fraction > 0x3FF) {
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

        if (exponent == 0) {
            if (fraction == 0) {
                exponent = 0;
            } else {
                while ((fraction & (1 << 23)) == 0) {
                    fraction <<= 1;
                    exponent--;
                }
                fraction &= ~(1 << 23);
                exponent += 127;
            }
        } else if (exponent == 31) {
            exponent = 255;
        } else {
            exponent -= 15;
            exponent += 127;
        }

        uint32 float_bits = sign | (exponent << 23) | fraction;

        return *reinterpret_cast<float *>(&float_bits);
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    float16 operator+(float16 other) const
    {
        return float16(float(*this) + float(other));
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    float16 operator-(float16 other) const
    {
        return float16(float(*this) - float(other));
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    float16 operator*(float16 other) const
    {
        return float16(float(*this) * float(other));
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    float16 operator/(float16 other) const
    {
        return float16(float(*this) / float(other));
    }

    HYP_FORCE_INLINE
    float16 &operator+=(float16 other)
    {
        *this = *this + other;
        return *this;
    }

    HYP_FORCE_INLINE
    float16 &operator-=(float16 other)
    {
        *this = *this - other;
        return *this;
    }

    HYP_FORCE_INLINE
    float16 &operator*=(float16 other)
    {
        *this = *this * other;
        return *this;
    }

    HYP_FORCE_INLINE
    float16 &operator/=(float16 other)
    {
        *this = *this / other;
        return *this;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    float16 operator-() const
    {
        return float16(-float(*this));
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator==(float16 other) const
    {
        return float(*this) == float(other);
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator!=(float16 other) const
    {
        return float(*this) != float(other);
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<(float16 other) const
    {
        return float(*this) < float(other);
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<=(float16 other) const
    {
        return float(*this) <= float(other);
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator>(float16 other) const
    {
        return float(*this) > float(other);
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator>=(float16 other) const
    {
        return float(*this) >= float(other);
    }

    HYP_FORCE_INLINE
    float16 operator++(int)
    {
        float16 result = *this;
        *this += 1.0f;
        return result;
    }

    HYP_FORCE_INLINE
    float16 operator--(int)
    {
        float16 result = *this;
        *this -= 1.0f;
        return result;
    }

    HYP_FORCE_INLINE
    float16 &operator++()
    {
        *this += 1.0f;
        return *this;
    }

    HYP_FORCE_INLINE
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
    /*! \brief Packs a float into a 32-bit integer.
     *  \param value The value to pack.
     *  \return The 32-bit integer packed from the value. */
    static inline uint32 PackFloat(float value)
    {
        union {
            uint32 u;
            float f;
        };

        f = value;

        return u;
    }

    /*! \brief Unpacks a 32-bit integer into a float.
     *  \param value The value to unpack.
     *  \return The float unpacked from the value. */
    static inline float UnpackFloat(uint32 value)
    {
        union {
            uint32 u;
            float f;
        };

        u = value;

        return f;
    }

    /*! \brief Packs a 4-component vector into a 32-bit integer.
     *  \param vec The vector to pack.
     *  \return The 32-bit integer packed from the vector. */
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

    /*! \brief Unpacks a 32-bit integer into a 4-component vector.
     *  \param value The value to unpack.
     *  \return The 4-component vector unpacked from the value. */
    static inline Vec4f UnpackVec4f(uint32 value)
    {
        return {
            float(value & 0xff000000) / 255.0f,
            float(value & 0x00ff0000) / 255.0f,
            float(value & 0x0000ff00) / 255.0f,
            float(value & 0x000000ff) / 255.0f
        };
    }

    /*! \brief Aligns a value to the specified alignment.
     *  \tparam T The type of the value to align.
     *  \param value The value to align.
     *  \param alignment The alignment to use.
     *  \return The value aligned to the specified alignment. */
    template <class T>
    static inline constexpr T AlignAs(T value, uint alignment)
    {
        return ((value + alignment - 1) / alignment) * alignment;
    }
    
    /*! \brief Gets the index of the lowest set bit in a 64-bit integer.
     *  \param bits The bits to get the index of the lowest set bit of.
     *  \return The index of the lowest set bit in the bits. */
    static uint LowestSetBitIndex(uint64 bits);

    /*! \brief Counts the number of bits set in a 64-bit integer.
     *  \param value The value to count the bits of.
     *  \return The number of bits set in the value. */
    static uint64 BitCount(uint64 value);
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