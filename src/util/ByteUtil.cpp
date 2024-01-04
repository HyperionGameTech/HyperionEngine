#include <util/ByteUtil.hpp>
#include <util/Defines.hpp>

namespace hyperion {

UInt ByteUtil::HighestSetBitIndex(UInt64 bits)
{
#ifdef HYP_WINDOWS
    UInt index;
    _BitScanReverse64(&index, bits);
    return index;
#elif defined(HYP_CLANG_OR_GCC)
    return 63 - __builtin_clzll(bits);
#else
    // UInt index = 0;

    // if (bits & 0xFFFFFFFF00000000) { bits >>= 32; index += 32; }
    // if (bits & 0x00000000FFFF0000) { bits >>= 16; index += 16; }
    // if (bits & 0x000000000000FF00) { bits >>=  8; index +=  8; }
    // if (bits & 0x00000000000000F0) { bits >>=  4; index +=  4; }
    // if (bits & 0x000000000000000C) { bits >>=  2; index +=  2; }
    // if (bits & 0x0000000000000002) { bits >>=  1; index +=  1; }

    // return index;
    #error "ByteUtil::HighestSetBitIndex() not implemented for this platform"
#endif
}

} // namespace hyperion