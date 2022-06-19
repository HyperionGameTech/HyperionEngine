#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

namespace hyperion {

static_assert(sizeof(float) == 4, "float must be 32 bit");
static_assert(sizeof(double) == 8, "double must be 64 bit");

using UByte    = uint8_t;
using SByte    = int8_t;

using UInt8    = uint8_t;
using UInt16   = uint16_t;
using UInt32   = uint32_t;
using UInt64   = uint64_t;

using UInt     = UInt32;

using Int8     = int8_t;
using Int16    = int16_t;
using Int32    = int32_t;
using Int64    = int64_t;

using Int      = Int32;

using Float32  = float;
using Float64  = double;

using Float    = Float32;

} // namespace hyperion

#endif
