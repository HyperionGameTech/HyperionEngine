#pragma once

#include "FxTypes.hpp"

namespace FxBit {

static constexpr uint32 scBitNotFound = UINT32_MAX;

/**
 * @brief Gets the least significant bit for an integer `v_`.
 */
#define FX_BIT_GET_LSB(v_) ((v_) & -(v_))


#pragma intrinsic(_BitScanReverse)
#pragma intrinsic(_BitScanForward)
#pragma intrinsic(_BitScanForward64)

/**
 * @brief Find the first set bit in the input value.
 * @param v The 64-bit value to search in.
 * @return The index of the bit, or `scBitNotFound` on failure.
 */
FX_FORCE_INLINE uint32 FindFirstOne64(uint64 value)
{
#if defined(FX_COMPILER_CLANG) || defined(FX_COMPILER_GCC)
    uint32 index = static_cast<uint32>(__builtin_ffsll(value));

    if (!index) {
        return scBitNotFound;
    }

    return index - 1ULL;

#else
    unsigned long index = 0;
    uint8 bit_found = _BitScanForward64(&index, value);

    if (!bit_found) {
        return scBitNotFound;
    }

    return static_cast<uint32>(index);
#endif
}


/**
 * @brief Find the first set bit in the input value.
 * @param v The 32-bit value to search in.
 * @return The index of the bit, or `scBitNotFound` on failure.
 */
FX_FORCE_INLINE uint32 FindFirstOne32(uint32 value)
{
#if defined(FX_COMPILER_CLANG) || defined(FX_COMPILER_GCC)
    uint32 index = static_cast<uint32>(__builtin_ffs(value));
    if (!index) {
        return scBitNotFound;
    }

    return index - 1U;

#else
    unsigned long index = 0;
    uint8 bit_found = _BitScanForward(&index, value);

    if (!bit_found) {
        return scBitNotFound;
    }

    return static_cast<uint32>(index);
#endif
}

FX_FORCE_INLINE uint32 FindLastOne32(uint32 value)
{
#if defined(FX_COMPILER_CLANG) || defined(FX_COMPILER_GCC)
    uint32 index = 32 - static_cast<uint32>(__builtin_clz(value));
    if (!index) {
        return scBitNotFound;
    }
    return index - 1U;
#else
    unsigned long index = 0;
    uint8 bit_found = _BitScanReverse(&index, value);

    if (!bit_found) {
        return scBitNotFound;
    }

    return static_cast<uint32>(index);
#endif
}

FX_FORCE_INLINE uint32 FindLastOne64(uint64 value)
{
#if defined(FX_COMPILER_CLANG) || defined(FX_COMPILER_GCC)
    if (value == 0) {
        return scBitNotFound;
    }

    uint32 index = 64 - static_cast<uint32>(__builtin_clzll(value));
    if (!index) {
        return scBitNotFound;
    }
    return index - 1U;
#else
    unsigned long index = 0;
    uint8 bit_found = _BitScanReverse64(&index, value);

    if (!bit_found) {
        return scBitNotFound;
    }

    return static_cast<uint32>(index);
#endif
}

FX_FORCE_INLINE uint32 FindFirstZero32(uint32 value) { return FindFirstOne32(~value); }

FX_FORCE_INLINE uint32 FindFirstZero64(uint64 value) { return FindFirstOne64(~value); }


} // namespace FxBit
