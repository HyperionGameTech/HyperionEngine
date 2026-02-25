/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/containers/Array.hpp>

#include <Core/Types.hpp>

#include <cstdint>
#include <cstring>

// #ifdef __MINGW32__
// #undef _WIN32
// #endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <cwchar>
#endif

namespace Hyperion {
namespace utf {

// #define HYP_UTF8_CHECKED

#define HYP_UTF8_ASSERT(cond)      \
    do                             \
    {                              \
        if (!HYP_UNLIKELY((cond))) \
        {                          \
            HYP_BREAKPOINT;        \
        }                          \
    }                              \
    while (0)

#ifdef HYP_UTF8_CHECKED

#define HYP_UTF8_CHECK_BOUNDS(idx, max) \
    do                                  \
    {                                   \
        if ((idx) == (max))             \
        {                               \
            return -1;                  \
        }                               \
    }                                   \
    while (0)

#else
#define HYP_UTF8_CHECK_BOUNDS(...)
#endif

using Char32 = char32_t;
using Char16 = char16_t;
using Char8 = char; //  backwards compatibility

inline void Init()
{
#ifdef _WIN32
    _setmode(_fileno(stdout), 0x00020000 /*_O_U16TEXT*/);
#endif
}

constexpr inline bool IsWhitespace(Char32 ch)
{
    return ch == Char32(' ') || ch == Char32('\n') || ch == Char32('\t') || ch == Char32('\r');
}

constexpr inline bool IsDecimal(Char32 ch)
{
    return (ch >= Char32('0')) && (ch <= Char32('9'));
}

constexpr inline bool IsHexadecimal(Char32 ch)
{
    return (ch >= Char32('0')) && (ch <= Char32('9'))
        || ((ch >= Char32('A') && ch <= Char32('F')))
        || ((ch >= Char32('a') && ch <= Char32('f')));
}

constexpr inline bool IsAlphabetical(Char32 ch)
{
    return (ch >= 0xC0) || ((ch >= Char32('A') && ch <= Char32('Z')) || (ch >= Char32('a') && ch <= Char32('z')));
}

constexpr inline SizeType StringLength(const Char8* first, const Char8* last)
{
    if (first == last)
    {
        return 0;
    }

    SizeType count = 0;
    SizeType codepoints = 0;

    for (; first[codepoints] != '\0' && (first + codepoints) != last; count++)
    {
        const Char8 c = first[codepoints];

        if (c >= 0 && c <= 127)
            codepoints += 1;
        else if ((c & 0xE0) == 0xC0)
            codepoints += 2;
        else if ((c & 0xF0) == 0xE0)
            codepoints += 3;
        else if ((c & 0xF8) == 0xF0)
            codepoints += 4;
        else
            return -1; // invalid utf8
    }

    return count;
}

constexpr inline SizeType StringLength(const Char8* first, const Char8* last, SizeType& outCodepoints)
{
    if (first == last)
    {
        outCodepoints = 0;

        return 0;
    }

    SizeType count = 0;
    SizeType codepoints = 0;

    for (; first[codepoints] != '\0' && (first + codepoints) != last; count++)
    {
        const Char8 c = first[codepoints];

        if (c >= 0 && c <= 127)
            codepoints += 1;
        else if ((c & 0xE0) == 0xC0)
            codepoints += 2;
        else if ((c & 0xF0) == 0xE0)
            codepoints += 3;
        else if ((c & 0xF8) == 0xF0)
            codepoints += 4;
        else
            return -1; // invalid utf8
    }

    outCodepoints = codepoints;

    return count;
}

constexpr inline SizeType StringLength(const Char8* str, SizeType& outCodepoints)
{
    SizeType count = 0;
    SizeType codepoints = 0;

    for (; str[codepoints] != '\0'; count++)
    {
        const Char8 c = str[codepoints];

        if (c >= 0 && c <= 127)
            codepoints += 1;
        else if ((c & 0xE0) == 0xC0)
            codepoints += 2;
        else if ((c & 0xF0) == 0xE0)
            codepoints += 3;
        else if ((c & 0xF8) == 0xF0)
            codepoints += 4;
        else
            return -1; // invalid utf8
    }

    outCodepoints = codepoints;

    return count;
}

template <class T, bool IsUtf8>
static inline constexpr SizeType StringLength(const T* str)
{
    if constexpr (IsUtf8)
    {
        SizeType codepoints = 0;
        return StringLength(str, codepoints);
    }
    else if constexpr (sizeof(T) == sizeof(char))
    {
        return std::strlen(str);
    }
    else
    {
        SizeType count = 0;
        const T* pos = str;
        for (; *pos; ++pos, count++)
            ;

        return count;
    }
}

template <class T, bool IsUtf8>
static inline constexpr SizeType StringLength(const T* str, SizeType& outCodepoints)
{
    if constexpr (IsUtf8)
    {
        return StringLength(str, outCodepoints);
    }
    else if constexpr (sizeof(T) == sizeof(char))
    {
        return (outCodepoints = std::strlen(str));
    }
    else
    {
        SizeType cp = 0;
        const T* pos = str;
        for (; *pos; ++pos, cp++)
            ;

        return (outCodepoints = cp);
    }
}

static inline int StringCompare(const Char8* s1, const Char8* s2, SizeType count)
{
    for (SizeType i = 0; (*s1 || *s2) && (i < count || count == 0); i++)
    {
        unsigned char c;

        Char32 c1 = 0;
        ubyte* c1Bytes = reinterpret_cast<ubyte*>(&c1);

        Char32 c2 = 0;
        ubyte* c2Bytes = reinterpret_cast<ubyte*>(&c2);

        // get the character for s1
        c = (ubyte)*s1;

        if (c >= 0 && c <= 127)
        {
            c1Bytes[0] = *(s1++);
        }
        else if ((c & 0xE0) == 0xC0)
        {
            c1Bytes[0] = *(s1++);
            c1Bytes[1] = *(s1++);
        }
        else if ((c & 0xF0) == 0xE0)
        {
            c1Bytes[0] = *(s1++);
            c1Bytes[1] = *(s1++);
            c1Bytes[2] = *(s1++);
        }
        else if ((c & 0xF8) == 0xF0)
        {
            c1Bytes[0] = *(s1++);
            c1Bytes[1] = *(s1++);
            c1Bytes[2] = *(s1++);
            c1Bytes[3] = *(s1++);
        }

        // get the character for s2
        c = (ubyte)*s2;

        if (c >= 0 && c <= 127)
        {
            c2Bytes[0] = *(s2++);
        }
        else if ((c & 0xE0) == 0xC0)
        {
            c2Bytes[0] = *(s2++);
            c2Bytes[1] = *(s2++);
        }
        else if ((c & 0xF0) == 0xE0)
        {
            c2Bytes[0] = *(s2++);
            c2Bytes[1] = *(s2++);
            c2Bytes[2] = *(s2++);
        }
        else if ((c & 0xF8) == 0xF0)
        {
            c2Bytes[0] = *(s2++);
            c2Bytes[1] = *(s2++);
            c2Bytes[2] = *(s2++);
            c2Bytes[3] = *(s2++);
        }

        if (c1 < c2)
        {
            return -1;
        }
        else if (c1 > c2)
        {
            return 1;
        }
    }

    return 0;
}

template <class T, bool IsUtf8>
static inline int StringCompare(const T* lhs, const T* rhs, SizeType count)
{
    if constexpr (IsUtf8)
    {
        return StringCompare(lhs, rhs, count);
    }

    const T* s1 = lhs;
    const T* s2 = rhs;

    for (SizeType i = 0; (*s1 || *s2) && (i < count || count == 0); s1++, s2++, i++)
    {
        if (*s1 < *s2)
        {
            return -1;
        }
        else if (*s1 > *s2)
        {
            return 1;
        }
    }

    return 0;
}

template <class T, bool IsUtf8>
static inline int StringCompare(const T* lhs, const T* rhs)
{
    return StringCompare<T, IsUtf8>(lhs, rhs, 0);
}

/*! \brief Convert a single utf-8 character (multiple code units) into a single utf-32 char
 *   \p str _must_ be at least sizeof(Char32)
 */
static inline Char32 Char8to32(const Char8* str)
{
    union
    {
        Char32 ret;
        Char8 retBytes[sizeof(Char32)];
    };

    ret = 0;

    Hyperion::uint32 i = 0;

    const uint8 ch = (uint8)str[0];

    if (ch <= 0x7F)
    {
        retBytes[0] = str[i];
    }
    else if ((ch & 0xE0) == 0xC0)
    {
        retBytes[0] = str[i++];
        HYP_UTF8_CHECK_BOUNDS(i, sizeof(Char32));
        retBytes[1] = str[i];
    }
    else if ((ch & 0xF0) == 0xE0)
    {
        retBytes[0] = str[i++];
        HYP_UTF8_CHECK_BOUNDS(i, sizeof(Char32));
        retBytes[1] = str[i++];
        HYP_UTF8_CHECK_BOUNDS(i, sizeof(Char32));
        retBytes[2] = str[i];
    }
    else if ((ch & 0xF8) == 0xF0)
    {
        retBytes[0] = str[i++];
        HYP_UTF8_CHECK_BOUNDS(i, sizeof(Char32));
        retBytes[1] = str[i++];
        HYP_UTF8_CHECK_BOUNDS(i, sizeof(Char32));
        retBytes[2] = str[i++];
        HYP_UTF8_CHECK_BOUNDS(i, sizeof(Char32));
        retBytes[3] = str[i];
    }
    else
    {
        // invalid utf-8
        return -1;
    }

    return ret;
}

/*! \brief Convert a single utf-8 character (multiple code units) into a single utf-32 char
 *   \p str _must_ be at least the the size of `max` (defaults to sizeof(Char32))
 */
static inline Char32 Char8to32(const Char8* str, SizeType max, SizeType& outCodepoints)
{
    union
    {
        Char32 ret;
        Char8 retBytes[sizeof(Char32)];
    };

    ret = 0;

    outCodepoints = 0;

    const uint8 ch = (uint8)str[0];

    if (ch <= 0x7F)
    {
        retBytes[0] = str[outCodepoints++];
    }
    else if ((ch & 0xE0) == 0xC0)
    {
        retBytes[0] = str[outCodepoints++];
        HYP_UTF8_CHECK_BOUNDS(outCodepoints, max);
        retBytes[1] = str[outCodepoints++];
    }
    else if ((ch & 0xF0) == 0xE0)
    {
        retBytes[0] = str[outCodepoints++];
        HYP_UTF8_CHECK_BOUNDS(outCodepoints, max);
        retBytes[1] = str[outCodepoints++];
        HYP_UTF8_CHECK_BOUNDS(outCodepoints, max);
        retBytes[2] = str[outCodepoints++];
    }
    else if ((ch & 0xF8) == 0xF0)
    {
        retBytes[0] = str[outCodepoints++];
        HYP_UTF8_CHECK_BOUNDS(outCodepoints, max);
        retBytes[1] = str[outCodepoints++];
        HYP_UTF8_CHECK_BOUNDS(outCodepoints, max);
        retBytes[2] = str[outCodepoints++];
        HYP_UTF8_CHECK_BOUNDS(outCodepoints, max);
        retBytes[3] = str[outCodepoints++];
    }
    else
    {
        // invalid utf-8
        return -1;
    }

    return ret;
}

/*! \brief Convert a single UTF-32 char to UTF-8 array of code points.
 *  The array at \p dst MUST have a sizeof Char32 (4 bytes)
 */
static inline void Char32to8(Char32 src, Char8* dst, SizeType& outCodepoints)
{
    // set all dst bytes to 0
    *reinterpret_cast<Char32*>(dst) = 0;

    outCodepoints = 0;

    const ubyte* srcBytes = reinterpret_cast<ubyte*>(&src);

    if (HYP_UNLIKELY(!*srcBytes))
        return;
    dst[outCodepoints++] = *(srcBytes++);
    if (!*srcBytes)
        return;
    dst[outCodepoints++] = *(srcBytes++);
    if (!*srcBytes)
        return;
    dst[outCodepoints++] = *(srcBytes++);
    if (!*srcBytes)
        return;
    dst[outCodepoints++] = *(srcBytes++);
}

static inline void Char32to8(Char32 src, Char8* dst)
{
    SizeType codepoints = 0;
    Char32to8(src, dst, codepoints);
}

#define HYP_UTF_MASK16(ch) ((uint16_t)(0xffff & (ch)))
#define HYP_UTF_IS_LEAD_SURROGATE(ch) ((ch) >= 0xd800u && (ch) <= 0xdbffu)
#define HYP_UTF_IS_TRAIL_SURROGATE(ch) ((ch) >= 0xdc00u && (ch) <= 0xdfffu)
#define HYP_UTF_SURROGATE_OFFSET (0x10000u - (0xd800u << 10) - 0xdc00u)

/*! \brief Convert a single UTF-16 character (possibly a surrogate pair) into a single UTF-32 char.
 *   \p str must point to at least one valid Char16, and two if it starts with a lead surrogate.
 */
static inline Char32 Char16to32(const Char16* str)
{
    const uint32 cp = HYP_UTF_MASK16(*str);

    if (HYP_UTF_IS_LEAD_SURROGATE(cp))
    {
        const uint32 trailSurrogate = HYP_UTF_MASK16(*(str + 1));

        if (!HYP_UTF_IS_TRAIL_SURROGATE(trailSurrogate))
        {
            // Invalid surrogate pair
            return Char32(-1);
        }

        return Char32((cp << 10) + trailSurrogate + HYP_UTF_SURROGATE_OFFSET);
    }
    else if (HYP_UTF_IS_TRAIL_SURROGATE(cp))
    {
        // Lone trail surrogate is invalid
        return Char32(-1);
    }

    return Char32(cp);
}

/*! \brief Convert a single UTF-16 character (possibly a surrogate pair) into a single UTF-32 char.
 *   \p str must be at least the size of \p max.
 *   \p outCodeUnits will be set to the number of UTF-16 code units consumed (1 or 2).
 */
static inline Char32 Char16to32(const Char16* str, SizeType max, SizeType& outCodeUnits)
{
    outCodeUnits = 0;

    if (max == 0)
    {
        return Char32(-1);
    }

    const uint32 cp = HYP_UTF_MASK16(*str);

    if (HYP_UTF_IS_LEAD_SURROGATE(cp))
    {
        if (max < 2)
        {
            // Not enough space for surrogate pair
            return Char32(-1);
        }

        const uint32 trailSurrogate = HYP_UTF_MASK16(*(str + 1));

        if (!HYP_UTF_IS_TRAIL_SURROGATE(trailSurrogate))
        {
            // Invalid surrogate pair
            return Char32(-1);
        }

        outCodeUnits = 2;
        return Char32((cp << 10) + trailSurrogate + HYP_UTF_SURROGATE_OFFSET);
    }
    else if (HYP_UTF_IS_TRAIL_SURROGATE(cp))
    {
        // Lone trail surrogate is invalid
        return Char32(-1);
    }

    outCodeUnits = 1;
    return Char32(cp);
}

/*! \brief Convert a single UTF-32 char to UTF-16 code unit(s).
 *  \p dst MUST have space for at least 2 Char16 values (for surrogate pairs).
 *  \p outCodeUnits will be set to the number of UTF-16 code units written (1 or 2).
 */
static inline void Char32to16(Char32 src, Char16* dst, SizeType& outCodeUnits)
{
    outCodeUnits = 0;

    if (src <= 0xFFFF)
    {
        // BMP character, single UTF-16 code unit
        dst[outCodeUnits++] = static_cast<Char16>(src);
    }
    else if (src <= 0x10FFFF)
    {
        // Supplementary character, needs surrogate pair
        const Char32 adjusted = src - 0x10000;
        dst[outCodeUnits++] = static_cast<Char16>((adjusted >> 10) + 0xD800);   // High surrogate
        dst[outCodeUnits++] = static_cast<Char16>((adjusted & 0x3FF) + 0xDC00); // Low surrogate
    }
    // else: invalid codepoint, outCodeUnits remains 0
}

static inline void Char32to16(Char32 src, Char16* dst)
{
    SizeType codeUnits = 0;
    Char32to16(src, dst, codeUnits);
}

/*! \brief Convert a single wide character (possibly a surrogate pair on Windows) into a single UTF-32 char.
 *  \p str must point to at least one valid wchar_t, and two if it starts with a lead surrogate (Windows).
 */
static inline Char32 WideTo32(const wchar_t* str)
{
    if constexpr (sizeof(wchar_t) == 4)
    {
        // direct conversion
        return static_cast<Char32>(*str);
    }
    else
    {
        const uint32 cp = HYP_UTF_MASK16(*str);

        if (HYP_UTF_IS_LEAD_SURROGATE(cp))
        {
            const uint32 trailSurrogate = HYP_UTF_MASK16(*(str + 1));

            if (!HYP_UTF_IS_TRAIL_SURROGATE(trailSurrogate))
            {
                // Invalid surrogate pair
                return Char32(-1);
            }

            return Char32((cp << 10) + trailSurrogate + HYP_UTF_SURROGATE_OFFSET);
        }
        else if (HYP_UTF_IS_TRAIL_SURROGATE(cp))
        {
            // Lone trail surrogate is invalid
            return Char32(-1);
        }

        return Char32(cp);
    }
}

/*! \brief Convert a single wide character (possibly a surrogate pair on Windows) into a single UTF-32 char.
 *  \p str must be at least the size of \p max.
 *  \p outCodeUnits will be set to the number of wchar_t code units consumed (1, or 2 on Windows for surrogate pairs).
 */
static inline Char32 WideTo32(const wchar_t* str, SizeType max, SizeType& outCodeUnits)
{
    outCodeUnits = 0;

    if (max == 0)
    {
        return Char32(-1);
    }

    if constexpr (sizeof(wchar_t) == 4)
    {
        outCodeUnits = 1;
        return static_cast<Char32>(*str);
    }
    else
    {
        const uint32 cp = HYP_UTF_MASK16(*str);

        if (HYP_UTF_IS_LEAD_SURROGATE(cp))
        {
            if (max < 2)
            {
                // Not enough space
                return Char32(-1);
            }

            const uint32 trailSurrogate = HYP_UTF_MASK16(*(str + 1));

            if (!HYP_UTF_IS_TRAIL_SURROGATE(trailSurrogate))
            {
                // Invalid surrogate pair
                return Char32(-1);
            }

            outCodeUnits = 2;
            return Char32((cp << 10) + trailSurrogate + HYP_UTF_SURROGATE_OFFSET);
        }
        else if (HYP_UTF_IS_TRAIL_SURROGATE(cp))
        {
            // Lone trail surrogate
            return Char32(-1);
        }

        outCodeUnits = 1;
        return Char32(cp);
    }
}

/*! \brief Convert a single UTF-32 char to wide character(s).
 *  \p dst MUST have space for at least 2 wchar_t values (for surrogate pairs on Windows).
 *  \p outCodeUnits will be set to the number of wchar_t code units written (1, or up to 2 on Windows).
 */
static inline void Char32ToWide(Char32 src, wchar_t* dst, SizeType& outCodeUnits)
{
    outCodeUnits = 0;

    if constexpr (sizeof(wchar_t) == 4)
    {
        dst[outCodeUnits++] = static_cast<wchar_t>(src);
    }
    else
    {
        if (src <= 0xFFFF)
        {
            dst[outCodeUnits++] = static_cast<wchar_t>(src);
        }
        else if (src <= 0x10FFFF)
        {
            const Char32 adjusted = src - 0x10000;
            dst[outCodeUnits++] = static_cast<wchar_t>((adjusted >> 10) + 0xD800);   // High surrogate
            dst[outCodeUnits++] = static_cast<wchar_t>((adjusted & 0x3FF) + 0xDC00); // Low surrogate
        }
        // else: invalid codepoint, outCodeUnits remains 0
    }
}

static inline void Char32ToWide(Char32 src, wchar_t* dst)
{
    SizeType codeUnits = 0;
    Char32ToWide(src, dst, codeUnits);
}

/*! \brief Pass nullptr to \p result on the first call to get the size needed for the buffer.
 *  Then call the function again with the memory allocated for \p result. */
inline SizeType ToUtf16(const Char32* start, const Char32* end, Char16* result)
{
    SizeType len = 0;

    while (start != end)
    {
        const Char32 cp = *start++;

        if (cp <= 0xFFFF)
        {
            // BMP character
            if (result)
            {
                result[len] = static_cast<Char16>(cp);
            }
            len++;
        }
        else if (cp <= 0x10FFFF)
        {
            // Supplementary character, needs surrogate pair
            if (result)
            {
                const Char32 adjusted = cp - 0x10000;
                result[len] = static_cast<Char16>((adjusted >> 10) + 0xD800);       // High surrogate
                result[len + 1] = static_cast<Char16>((adjusted & 0x3FF) + 0xDC00); // Low surrogate
            }
            len += 2;
        }
        // else: invalid codepoint, skip
    }

    return len;
}

/*! \brief Pass nullptr to \p result on the first call to get the size needed for the buffer.
 *  Then call the function again with the memory allocated for \p result. */
inline SizeType ToUtf16(const Char8* start, const Char8* end, Char16* result)
{
    SizeType len = 0;

    while (start != end)
    {
        SizeType codepoints = 0;
        const Char32 cp = Char8to32(start, SizeType(end - start), codepoints);

        if (cp == Char32(-1) || codepoints == 0)
        {
            break;
        }

        start += codepoints;

        if (cp <= 0xFFFF)
        {
            // BMP character
            if (result)
            {
                result[len] = static_cast<Char16>(cp);
            }
            len++;
        }
        else if (cp <= 0x10FFFF)
        {
            // Supplementary character, needs surrogate pair
            if (result)
            {
                const Char32 adjusted = cp - 0x10000;
                result[len] = static_cast<Char16>((adjusted >> 10) + 0xD800);       // High surrogate
                result[len + 1] = static_cast<Char16>((adjusted & 0x3FF) + 0xDC00); // Low surrogate
            }
            len += 2;
        }
    }

    return len;
}

/*! \brief Pass nullptr to \p result on the first call to get the size needed for the buffer.
 *  Then call the function again with the memory allocated for \p result. */
inline SizeType ToUtf16(const wchar_t* start, const wchar_t* end, Char16* result)
{
    SizeType len = 0;

    while (start != end)
    {
        const wchar_t ch = *start++;

        if constexpr (sizeof(wchar_t) == 2)
        {
            // wchar_t is already UTF-16 (Windows)
            if (result)
            {
                result[len] = static_cast<Char16>(ch);
            }
            len++;
        }
        else
        {
            // wchar_t is UTF-32 (Unix/macOS)
            const Char32 cp = static_cast<Char32>(ch);

            if (cp <= 0xFFFF)
            {
                if (result)
                {
                    result[len] = static_cast<Char16>(cp);
                }
                len++;
            }
            else if (cp <= 0x10FFFF)
            {
                if (result)
                {
                    const Char32 adjusted = cp - 0x10000;
                    result[len] = static_cast<Char16>((adjusted >> 10) + 0xD800);
                    result[len + 1] = static_cast<Char16>((adjusted & 0x3FF) + 0xDC00);
                }
                len += 2;
            }
        }
    }

    return len;
}

inline Char32 CharAt(const utf::Char8* str, SizeType max, SizeType index)
{
    SizeType characterIndex = 0;

    for (SizeType i = 0; i < max; characterIndex++)
    {
        Char8 c(str[i]);

        union
        {
            Char32 ret;
            char retBytes[sizeof(Char32)];
        };

        ret = 0;

        if (c <= 0x7F)
        {
            retBytes[0] = str[i++];
        }
        else if ((c & 0xE0) == 0xC0)
        {
            retBytes[0] = str[i++];
            HYP_UTF8_CHECK_BOUNDS(i, max);
            retBytes[1] = str[i++];
        }
        else if ((c & 0xF0) == 0xE0)
        {
            retBytes[0] = str[i++];
            HYP_UTF8_CHECK_BOUNDS(i, max);
            retBytes[1] = str[i++];
            HYP_UTF8_CHECK_BOUNDS(i, max);
            retBytes[2] = str[i++];
        }
        else if ((c & 0xF8) == 0xF0)
        {
            retBytes[0] = str[i++];
            HYP_UTF8_CHECK_BOUNDS(i, max);
            retBytes[1] = str[i++];
            HYP_UTF8_CHECK_BOUNDS(i, max);
            retBytes[2] = str[i++];
            HYP_UTF8_CHECK_BOUNDS(i, max);
            retBytes[3] = str[i++];
        }
        else
        {
            // invalid utf-8
            return -1;
        }

        if (characterIndex == index)
        {
            // reached index
            return ret;
        }

        // end reached
        if (c == Char8('\0'))
        {
            return -1;
        }
    }

    // error
    return -1;
}

/*! \brief Get the UTF-8 char (array of code points) at the specific index of the string.
 *  \p dst MUST have a size of at least the sizeof Char32, so 4 bytes.
 */
inline void CharAt(const Char8* str, Char8* dst, SizeType max, SizeType index)
{
    Char32to8(CharAt(str, max, index), dst);
}

inline SizeType NextCodePoint(uint32 cp, Char8*& result)
{
    if (result)
    {
        SizeType len = 0;

        if (cp < 0x80)
        {
            result[len++] = Char8(cp);
        }
        else if (cp < 0x800)
        {
            result[len++] = Char8((cp >> 6) | 0xc0);
            result[len++] = Char8((cp & 0x3f) | 0x80);
        }
        else if (cp < 0x10000)
        {
            result[len++] = Char8((cp >> 12) | 0xe0);
            result[len++] = Char8(((cp >> 6) & 0x3f) | 0x80);
            result[len++] = Char8((cp & 0x3f) | 0x80);
        }
        else
        {
            result[len++] = Char8((cp >> 18) | 0xf0);
            result[len++] = Char8(((cp >> 12) & 0x3f) | 0x80);
            result[len++] = Char8(((cp >> 6) & 0x3f) | 0x80);
            result[len++] = Char8((cp & 0x3f) | 0x80);
        }

        result += len;

        return len;
    }

    if (cp < 0x80)
    {
        return 1;
    }
    else if (cp < 0x800)
    {
        return 2;
    }
    else if (cp < 0x10000)
    {
        return 3;
    }
    else
    {
        return 4;
    }
}

/*! \brief Pass nullptr to \p result on the first call to get the size needed for the buffer.
 * *  Then call the function again with the memory allocated for \p result. */
inline SizeType ToUtf8(const Char16* start, const Char16* end, Char8* result)
{
    SizeType len = 0;

    while (start != end)
    {
        uint32 cp = HYP_UTF_MASK16(*start++);
        // Take care of surrogate pairs first
        if (HYP_UTF_IS_LEAD_SURROGATE(cp))
        {
            const uint32 trailSurrogate = HYP_UTF_MASK16(*start++);
            HYP_UTF8_ASSERT(HYP_UTF_IS_TRAIL_SURROGATE(trailSurrogate));
            cp = (cp << 10) + trailSurrogate + HYP_UTF_SURROGATE_OFFSET;
        }
        else
        {
            // Lone trail surrogate
            HYP_UTF8_ASSERT(!HYP_UTF_IS_TRAIL_SURROGATE(cp));
        }

        len += NextCodePoint(cp, result);
    }

    return len;
}

/*! \brief Pass nullptr to \p result on the first call to get the size needed for the buffer.
 * *  Then call the function again with the memory allocated for \p result. */
inline SizeType ToUtf8(const Char32* start, const Char32* end, Char8* result)
{
    SizeType len = 0;

    while (start != end)
    {
        uint32 cp = *start++;

        len += NextCodePoint(cp, result);
    }

    return len;
}

/*! \brief Pass nullptr to \p result on the first call to get the size needed for the buffer.
 * *  Then call the function again with the memory allocated for \p result. */
inline SizeType ToUtf8(const wchar_t* start, const wchar_t* end, Char8* result)
{
#ifdef _WIN32
    SizeType len = 0;

    if (result)
    {
        len = SizeType(WideCharToMultiByte(CP_UTF8, 0, start, (int)(end - start), (char*)result, 0, NULL, NULL));

        if (len == 0)
        {
            return 0;
        }

        WideCharToMultiByte(CP_UTF8, 0, start, (int)(end - start), (char*)result, int(len), NULL, NULL);
    }
    else
    {
        len = SizeType(WideCharToMultiByte(CP_UTF8, 0, start, (int)(end - start), NULL, 0, NULL, NULL));
    }

    return len;
#else
    SizeType len = 0;

    if (result)
    {
        while (start != end)
        {
            wchar_t ch = *start++;

            if (ch <= 0x7F)
            {
                result[len++] = static_cast<Char8>(ch);
            }
            else if (ch <= 0x7FF)
            {
                result[len++] = static_cast<Char8>(0xC0 | ((ch >> 6) & 0x1F));
                result[len++] = static_cast<Char8>(0x80 | (ch & 0x3F));
            }
            else if (ch <= 0xFFFF)
            {
                result[len++] = static_cast<Char8>(0xE0 | ((ch >> 12) & 0x0F));
                result[len++] = static_cast<Char8>(0x80 | ((ch >> 6) & 0x3F));
                result[len++] = static_cast<Char8>(0x80 | (ch & 0x3F));
            }
            else if (ch <= 0x10FFFF)
            {
                result[len++] = static_cast<Char8>(0xF0 | ((ch >> 18) & 0x07));
                result[len++] = static_cast<Char8>(0x80 | ((ch >> 12) & 0x3F));
                result[len++] = static_cast<Char8>(0x80 | ((ch >> 6) & 0x3F));
                result[len++] = static_cast<Char8>(0x80 | (ch & 0x3F));
            }
        }
    }
    else
    {
        while (start != end)
        {
            wchar_t ch = *start++;

            if (ch <= 0x7F)
            {
                len++;
            }
            else if (ch <= 0x7FF)
            {
                len += 2;
            }
            else if (ch <= 0xFFFF)
            {
                len += 3;
            }
            else if (ch <= 0x10FFFF)
            {
                len += 4;
            }
        }
    }

    return len;
#endif
}

inline SizeType ToWide(const Char8* start, const Char8* end, wchar_t* result)
{
    SizeType len = 0;

#ifdef _WIN32
    if (result)
    {
        len = SizeType(MultiByteToWideChar(CP_UTF8, 0, start, (int)(end - start), NULL, 0));

        if (len == 0)
        {
            return 0;
        }

        MultiByteToWideChar(CP_UTF8, 0, start, (int)(end - start), result, int(len));
    }
    else
    {
        len = SizeType(MultiByteToWideChar(CP_UTF8, 0, start, (int)(end - start), NULL, 0));
    }
#else
    if (result)
    {
        while (start != end)
        {
            Char32 ch = 0;
            SizeType codepoints = 0;

            ch = utf::Char8to32(start, end - start, codepoints);

            if (ch == -1)
            {
                break;
            }

            start += codepoints;

            if (ch <= 0xFFFF)
            {
                *result++ = static_cast<wchar_t>(ch);
                len++;
            }
            else if (ch <= 0x10FFFF)
            {
                ch -= 0x10000;
                *result++ = static_cast<wchar_t>((ch >> 10) + 0xD800);
                *result++ = static_cast<wchar_t>((ch & 0x3FF) + 0xDC00);
                len += 2;
            }
        }
    }
    else
    {
        while (start != end)
        {
            Char32 ch = 0;
            SizeType codepoints = 0;

            ch = utf::Char8to32(start, end - start, codepoints);
            if (ch == -1)
            {
                break;
            }

            start += codepoints;

            if (ch <= 0xFFFF)
            {
                len++;
            }
            else if (ch <= 0x10FFFF)
            {
                len += 2;
            }
        }
    }
#endif

    return len;
}

inline SizeType ToWide(const Char16* start, const Char16* end, wchar_t* result)
{
    const SizeType len = SizeType(end - start);

    if (result)
    {
        for (SizeType i = 0; i < len; i++)
        {
            result[i] = (wchar_t)start[i];
        }
    }

    return SizeType(len);
}

inline SizeType ToWide(const Char32* start, const Char32* end, wchar_t* result)
{
    SizeType len = 0;

    if constexpr (sizeof(wchar_t) == 4)
    {
        // wchar_t is UTF-32 (Unix/macOS), direct copy
        len = SizeType(end - start);

        if (result)
        {
            for (SizeType i = 0; i < len; i++)
            {
                result[i] = static_cast<wchar_t>(start[i]);
            }
        }
    }
    else // Win32 needs to handle surrogate pairs
    {
        while (start != end)
        {
            const Char32 cp = *start++;

            if (cp <= 0xFFFF)
            {
                if (result)
                {
                    result[len] = static_cast<wchar_t>(cp);
                }
                len++;
            }
            else if (cp <= 0x10FFFF)
            {
                if (result)
                {
                    const Char32 adjusted = cp - 0x10000;
                    result[len] = static_cast<wchar_t>((adjusted >> 10) + 0xD800);
                    result[len + 1] = static_cast<wchar_t>((adjusted & 0x3FF) + 0xDC00);
                }
                len += 2;
            }
        }
    }

    return len;
}

/*! \brief Pass nullptr to \p result on the first call to get the size needed for the buffer.
 *  Then call the function again with the memory allocated for \p result. */
inline SizeType ToUtf32(const Char8* start, const Char8* end, Char32* result)
{
    SizeType len = 0;

    while (start != end)
    {
        SizeType codepoints = 0;
        const Char32 cp = Char8to32(start, SizeType(end - start), codepoints);

        if (cp == Char32(-1) || codepoints == 0)
        {
            break;
        }

        start += codepoints;

        if (result)
        {
            result[len] = cp;
        }
        len++;
    }

    return len;
}

/*! \brief Pass nullptr to \p result on the first call to get the size needed for the buffer.
 *  Then call the function again with the memory allocated for \p result. */
inline SizeType ToUtf32(const Char16* start, const Char16* end, Char32* result)
{
    SizeType len = 0;

    while (start != end)
    {
        SizeType codeUnits = 0;
        const Char32 cp = Char16to32(start, SizeType(end - start), codeUnits);

        if (cp == Char32(-1) || codeUnits == 0)
        {
            break;
        }

        start += codeUnits;

        if (result)
        {
            result[len] = cp;
        }
        len++;
    }

    return len;
}

/*! \brief Pass nullptr to \p result on the first call to get the size needed for the buffer.
 *  Then call the function again with the memory allocated for \p result. */
inline SizeType ToUtf32(const wchar_t* start, const wchar_t* end, Char32* result)
{
    SizeType len = 0;

    if constexpr (sizeof(wchar_t) == 4)
    {
        // wchar_t is UTF-32 (Unix/macOS), direct copy
        len = SizeType(end - start);

        if (result)
        {
            for (SizeType i = 0; i < len; i++)
            {
                result[i] = static_cast<Char32>(start[i]);
            }
        }
    }
    else
    {
        // wchar_t is UTF-16 (Windows), need to handle surrogate pairs
        while (start != end)
        {
            const uint32 cp = HYP_UTF_MASK16(*start);

            if (HYP_UTF_IS_LEAD_SURROGATE(cp))
            {
                if ((start + 1) >= end)
                {
                    // Incomplete surrogate pair
                    break;
                }

                const uint32 trailSurrogate = HYP_UTF_MASK16(*(start + 1));

                if (!HYP_UTF_IS_TRAIL_SURROGATE(trailSurrogate))
                {
                    // Invalid surrogate pair
                    break;
                }

                if (result)
                {
                    result[len] = Char32((cp << 10) + trailSurrogate + HYP_UTF_SURROGATE_OFFSET);
                }
                len++;
                start += 2;
            }
            else if (HYP_UTF_IS_TRAIL_SURROGATE(cp))
            {
                // Lone trail surrogate is invalid
                break;
            }
            else
            {
                if (result)
                {
                    result[len] = static_cast<Char32>(cp);
                }
                len++;
                start++;
            }
        }
    }

    return len;
}

/*! \brief How to use:
    if buffer length is not known, pass nullptr for \p result.
    bufferLength will be set to the size needed for \p result.
    Next, call the function again, passing in the previously mentioned
    value for \p bufferLength. The resulting string will be written into the provided
    param, \p result, so it'll need to have \p bufferLength bytes allocated to it. */
template <class T, class CharType>
inline void ToString(T value, SizeType& bufferLength, CharType* result)
{
    T divisor = 1;
    bool isNegative = 0;
    SizeType bufferIndex = 0;

    if (value < 0)
    {
        isNegative = true;
    }

    if (result == nullptr)
    {
        // first call
        bufferLength = 1;

        while (value / divisor >= 10)
        {
            divisor *= 10;
            ++bufferLength;
        }

        // for negative sign
        if (isNegative)
        {
            ++bufferLength;
        }

        // for null char
        ++bufferLength;

        return; // return after writing bufferLength
    }

    HYP_UTF8_ASSERT(bufferLength != 0);

    // don't modify passed in value any more
    SizeType bufferLengthRemaining = bufferLength - 1;

    if (isNegative)
    {
        HYP_UTF8_ASSERT(bufferLength != 1);
        result[bufferIndex++] = CharType('-');
        value *= -1;

        --bufferLengthRemaining;
    }

    while (value / divisor >= 10)
    {
        divisor *= 10;
    }

    while (bufferLengthRemaining)
    {
        // ASCII table has the number characters in sequence from 0-9 so use the
        // ASCII value of '0' as the base
        result[bufferIndex++] = CharType(T('0') + value / divisor);

        // This removes the most significant digit converting 1337 to 337 because
        // 1337 % 1000 = 337
        value = value % divisor;

        // Adjust the divisor to next lowest position
        divisor = divisor / 10;

        --bufferLengthRemaining;
    }

    // NULL terminate the string
    result[bufferIndex] = 0;
}

inline char* ToUtf8Chars(Char32& ch)
{
    return std::bit_cast<char*>(&ch);
}

} // namespace utf
} // namespace Hyperion
