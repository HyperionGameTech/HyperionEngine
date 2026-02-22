/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <stdint.h>

namespace Hyperion {

using ubyte = uint8_t;

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

using float32 = float;
static_assert(sizeof(float32) == 4, "Expected float to be 32-bit!");

using float64 = double;
static_assert(sizeof(float64) == 8, "Expected double to be 64-bit!");

using SizeType = decltype(sizeof(int));

#ifdef HYP_WINDOWS
using TChar = wchar_t;
#else
using TChar = char;
#endif

// declare custom pointer-sized types so they don't get defined as long / unsigned long etc.

template <SizeType Size, bool Signed>
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

using UIntPtr = typename PointerSizedTypeHelper<sizeof(void*), false>::Type;
using IntPtr = typename PointerSizedTypeHelper<sizeof(void*), true>::Type;

} // namespace Hyperion
