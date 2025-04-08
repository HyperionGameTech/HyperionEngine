/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <core/utilities/ByteUtil.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace utilities {

uint32 ByteUtil::LowestSetBitIndex(uint64 bits)
{
#ifdef HYP_CLANG_OR_GCC
    const int bit_index = __builtin_ffsll(bits) - 1;
#elif defined(HYP_MSVC)
    unsigned long bit_index = 0;
    _BitScanForward64(&bit_index, bits);
#else
    #error "ByteUtil::LowestSetBitIndex() not implemented for this platform"
#endif

    return uint32(bit_index);
}

uint32 ByteUtil::HighestSetBitIndex(uint64 bits)
{
#ifdef HYP_CLANG_OR_GCC
    const int bit_index = 63 - __builtin_clzll(bits);
#elif defined(HYP_MSVC)
    unsigned long bit_index = 0;
    _BitScanReverse64(&bit_index, bits);
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
    value = value - ((value >> 1) & (uint64)~(uint64)0/3); 
    value = (value & (uint64)~(uint64)0/15*3) + ((value >> 2) & (uint64)~(uint64)0/15*3);
    value = (value + (value >> 4)) & (uint64)~(uint64)0/255*15;
    return (uint64)(value * ((uint64)~(uint64)0/255)) >> (sizeof(uint64) - 1) * CHAR_BIT;
#endif
}

} // namespace utilities
} // namespace hyperion