/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <stdint.h>
#include <stddef.h>
#include <ctype.h>

namespace Hyperion {

using ubyte = unsigned char;

using uint8 = unsigned char;
using uint16 = unsigned short;
using uint32 = unsigned int;

#if HYP_WINDOWS
using uint64 = unsigned long long;
#else
using uint64 = unsigned long;
#endif

using int8 = signed char;
using int16 = short;
using int32 = int;

#if HYP_WINDOWS
using int64 = long long;
#else
using int64 = long;
#endif

using float32 = float;
static_assert(sizeof(float32) == 4, "Expected float to be 32-bit!");

using float64 = double;
static_assert(sizeof(float64) == 8, "Expected double to be 64-bit!");

#if HYP_MSVC
using size_t = decltype(sizeof(int));
#endif

#if HYP_WINDOWS
using TChar = wchar_t;
#else
using TChar = char;
#endif

// declare custom pointer-sized types so they don't get defined as long / unsigned long etc.

template <int Size, bool Signed>
struct PointerSizedTypeHelper;

template <>
struct PointerSizedTypeHelper<4, true>
{
    using Type = int32;
};

template <>
struct PointerSizedTypeHelper<4, false>
{
    using Type = uint32;
};

template <>
struct PointerSizedTypeHelper<8, true>
{
    using Type = int64;
};

template <>
struct PointerSizedTypeHelper<8, false>
{
    using Type = uint64;
};

using UIntPtr = typename PointerSizedTypeHelper<int(sizeof(void*)), false>::Type;
using IntPtr = typename PointerSizedTypeHelper<int(sizeof(void*)), true>::Type;

} // namespace Hyperion
