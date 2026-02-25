/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/math/MathUtil.hpp>
#include <Core/math/Vector4.hpp>

#include <Core/Types.hpp>
#include <Core/Defines.hpp>

#include <climits>

namespace Hyperion {
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

    static inline uint32 QuantizeFloat(float f, uint8 bits)
    {
        return uint32(MathUtil::Round(MathUtil::Clamp(f, 0.0f, 1.0f) * ((1u << bits) - 1)));
    }

    static inline float UnquantizeFloat(uint32 x, uint8 bits)
    {
        return float(x) / float((1u << bits) - 1);
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

    static inline bool IsBigEndian()
    {
        constexpr union
        {
            uint32 i;
            uint8 ch[sizeof(uint32)];
        } u = { 0x01020304 };

        return u.ch[0] == 1;
    }

    static inline bool IsLittleEndian()
    {
        return !ByteUtil::IsBigEndian();
    }
};

uint32 ByteUtil::LowestSetBitIndex(uint64 bits)
{
#ifdef HYP_CLANG_OR_GCC
    const int bitIndex = bits != 0 ? (__builtin_ffsll(bits) - 1) : uint32(-1);
#elif defined(HYP_MSVC)
    unsigned long bitIndex = 0;
    if (!_BitScanForward64(&bitIndex, bits))
    {
        return uint32(-1);
    }
#else
#error "ByteUtil::LowestSetBitIndex() not implemented for this platform"
#endif

    return uint32(bitIndex);
}

uint32 ByteUtil::HighestSetBitIndex(uint64 bits)
{
#ifdef HYP_CLANG_OR_GCC
    const int bitIndex = bits != 0 ? (63 - __builtin_clzll(bits)) : uint32(-1);
#elif defined(HYP_MSVC)
    unsigned long bitIndex = 0;
    if (!_BitScanReverse64(&bitIndex, bits))
    {
        return uint32(-1);
    }
#else
#error "ByteUtil::HighestSetBitIndex() not implemented for this platform"
#endif
    return uint32(bitIndex);
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

/*! \brief Converts a value of type \ref To to a value of type \ref From.
 *  Both types must be standard layout and have the same size.
 *  \tparam To The type to convert to.
 *  \tparam From The type to convert from.
 *  \param from The value to convert.
 *  \return The value of type \ref From converted to type \ref To.
 */
template <class To, class From>
static HYP_FORCE_INLINE To BitCast(const From& from)
{
    return ValueStorage<To>(&from).Get();
}

/// Endian swap functions - Free functions so new overloads can be easily added

static inline constexpr uint8 SwapEndian(uint8 value)
{
    return value;
}

static inline constexpr uint16 SwapEndian(uint16 value)
{
    return uint16(((value >> 8u) & 0xffu) | ((value & 0xffu) << 8u));
}

static inline constexpr uint32 SwapEndian(uint32 value)
{
    return ((value & 0xff000000u) >> 24u) | ((value & 0x00ff0000u) >> 8u) | ((value & 0x0000ff00u) << 8u) | ((value & 0x000000ffu) << 24u);
}

static inline constexpr uint64 SwapEndian(uint64 value)
{
    return ((value & 0xff00000000000000ull) >> 56ull) | ((value & 0x00ff000000000000ull) >> 40ull) | ((value & 0x0000ff0000000000ull) >> 24ull) | ((value & 0x000000ff00000000ull) >> 8ull) | ((value & 0x00000000ff000000ull) << 8ull) | ((value & 0x0000000000ff0000ull) << 24ull) | ((value & 0x000000000000ff00ull) << 40ull) | ((value & 0x00000000000000ffull) << 56ull);
}

static inline constexpr int8 SwapEndian(int8 value)
{
    return value;
}

static inline constexpr int16 SwapEndian(int16 value)
{
    union
    {
        uint16 u;
        int16 i;
    };

    i = value;

    u = SwapEndian(u);

    return i;
}

static inline constexpr int32 SwapEndian(int32 value)
{
    union
    {
        uint32 u;
        int32 i;
    };

    i = value;

    u = SwapEndian(u);

    return i;
}

static inline constexpr int64 SwapEndian(int64 value)
{
    union
    {
        uint64 u;
        int64 i;
    };

    i = value;

    u = SwapEndian(u);

    return i;
}

#define FOR_EACH_BIT(_num, _iter) for (uint64 ITER_BITMASK = (_num), _iter; (_iter = ByteUtil::LowestSetBitIndex(ITER_BITMASK)) != uint32(-1); (ITER_BITMASK &= ~(uint64(1) << _iter)))

} // namespace utilities

using utilities::BitCast;
using utilities::ByteUtil;
using utilities::SwapEndian;

} // namespace Hyperion
