/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BYTE_UTIL_HPP
#define HYPERION_BYTE_UTIL_HPP

#include <core/math/MathUtil.hpp>
#include <core/math/Vector4.hpp>

#include <Types.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace utilities {

class HYP_API ByteUtil
{
public:
    /*! \brief Packs a float into a 32-bit integer.
     *  \param value The value to pack.
     *  \return The 32-bit integer packed from the value. */
    static inline uint32 PackFloat(float value)
    {
        union
        {
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
        union
        {
            uint32 u;
            float f;
        };

        u = value;

        return f;
    }

    /*! \brief Packs a 4-component vector into a 32-bit integer.
     *  \param vec The vector to pack.
     *  \return The 32-bit integer packed from the vector. */
    static inline uint32 PackVec4f(const Vec4f& vec)
    {
        union
        {
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
            float((value & 0xFF000000u) >> 24) / 255.0f,
            float((value & 0x00FF0000u) >> 16) / 255.0f,
            float((value & 0x0000FF00u) >> 8) / 255.0f,
            float((value & 0x000000FFu)) / 255.0f
        };
    }

    /*! \brief Aligns a value to the specified alignment.
     *  \tparam T The type of the value to align.
     *  \param value The value to align.
     *  \param alignment The alignment to use.
     *  \return The value aligned to the specified alignment. */
    template <class T>
    static inline constexpr T AlignAs(T value, uint32 alignment)
    {
        return ((value + alignment - 1) / alignment) * alignment;
    }

    /*! \brief Gets the index of the lowest set bit in a 64-bit integer.
     *  \param bits The bits to get the index of the lowest set bit of.
     *  \return The index of the lowest set bit in the bits. */
    static inline uint32 LowestSetBitIndex(uint64 bits);

    /*! \brief Gets the index of the highest set bit in a 64-bit integer.
     *  \param bits The bits to get the index of the highest set bit of.
     *  \return The index of the highest set bit in the bits. */
    static inline uint32 HighestSetBitIndex(uint64 bits);

    /*! \brief Counts the number of bits set in a 64-bit integer.
     *  \param value The value to count the bits of.
     *  \return The number of bits set in the value. */
    static inline uint64 BitCount(uint64 value);
};

uint32 ByteUtil::LowestSetBitIndex(uint64 bits)
{
#ifdef HYP_CLANG_OR_GCC
    const int bit_index = bits != 0 ? (__builtin_ffsll(bits) - 1) : uint32(-1);
#elif defined(HYP_MSVC)
    unsigned long bit_index = 0;
    if (!_BitScanForward64(&bit_index, bits))
    {
        return uint32(-1);
    }
#else
#error "ByteUtil::LowestSetBitIndex() not implemented for this platform"
#endif

    return uint32(bit_index);
}

uint32 ByteUtil::HighestSetBitIndex(uint64 bits)
{
#ifdef HYP_CLANG_OR_GCC
    const int bit_index = bits != 0 ? (63 - __builtin_clzll(bits)) : uint32(-1);
#elif defined(HYP_MSVC)
    unsigned long bit_index = 0;
    if (!_BitScanReverse64(&bit_index, bits))
    {
        return uint32(-1);
    }
#else
#error "ByteUtil::HighestSetBitIndex() not implemented for this platform"
#endif
    return uint32(bit_index);
}

uint64 ByteUtil::BitCount(uint64 value)
{
#if HYP_WINDOWS
    return __popcnt64(value);
#else
    // https://graphics.stanford.edu/~seander/bithacks.html
    value = value - ((value >> 1) & (uint64) ~(uint64)0 / 3);
    value = (value & (uint64) ~(uint64)0 / 15 * 3) + ((value >> 2) & (uint64) ~(uint64)0 / 15 * 3);
    value = (value + (value >> 4)) & (uint64) ~(uint64)0 / 255 * 15;
    return (uint64)(value * ((uint64) ~(uint64)0 / 255)) >> (sizeof(uint64) - 1) * CHAR_BIT;
#endif
}

/*! \brief Converts a value of type \ref{To} to a value of type \ref{From}.
 *  Both types must be standard layout and have the same size.
 *  \tparam To The type to convert to.
 *  \tparam From The type to convert from.
 *  \param from The value to convert.
 *  \return The value of type \ref{From} converted to type \ref{To}.
 */
template <class To, class From>
static HYP_FORCE_INLINE To BitCast(const From& from)
{
    return ValueStorage<To>(&from).Get();
}

#define FOR_EACH_BIT(_num, _iter) for (uint64 ITER_BITMASK = (_num), _iter; (_iter = ByteUtil::LowestSetBitIndex(ITER_BITMASK)) != uint32(-1); (ITER_BITMASK &= ~(uint64(1) << _iter)))

} // namespace utilities

using utilities::BitCast;
using utilities::ByteUtil;

} // namespace hyperion

#endif