/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <util/ByteUtil.hpp>
#include <util/Defines.hpp>

namespace hyperion {

uint ByteUtil::LowestSetBitIndex(uint64 bits)
{
#ifdef HYP_CLANG_OR_GCC
    const int bit_index = __builtin_ffsll(bits) - 1;
#elif defined(HYP_MSVC)
    unsigned long bit_index = 0;
    _BitScanForward64(&bit_index, bits);
#else
    #error "ByteUtil::LowestSetBitIndex() not implemented for this platform"
#endif

    return uint(bit_index);
}

} // namespace hyperion