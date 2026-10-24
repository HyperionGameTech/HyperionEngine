/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Containers/Array.hpp>

#include <Core/Types.hpp>

#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

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

static constexpr size_t InvalidUTF8 = SIZE_MAX;

#define HYP_UTF8_ASSERT(cond)      \
    do                             \
    {                              \
        if (!HYP_UNLIKELY((cond))) \
        {                          \
            HYP_BREAKPOINT;        \
        }                          \
    }                              \
    while (0)

using Char32 = char32_t;
using Char16 = char16_t;
using Char8 = char; //  backwards compatibility

inline void Init()
{
#ifdef _WIN32
    _setmode(_fileno(stdout), 0x00020000 /*_O_U16TEXT*/);
#endif
}

inline constexpr bool IsWhitespace(Char32 ch)
{
    return ch == Char32(' ') || ch == Char32('\n') || ch == Char32('\t') || ch == Char32('\r');
}

inline constexpr bool IsDecimal(Char32 ch)
{
    return (ch >= Char32('0')) && (ch <= Char32('9'));
}

inline constexpr bool IsHexadecimal(Char32 ch)
{
    return (ch >= Char32('0')) && (ch <= Char32('9'))
        || ((ch >= Char32('A') && ch <= Char32('F')))
        || ((ch >= Char32('a') && ch <= Char32('f')));
}

inline constexpr bool IsAlphabetical(Char32 ch)
{
    return (ch >= 0xC0) || ((ch >= Char32('A') && ch <= Char32('Z')) || (ch >= Char32('a') && ch <= Char32('z')));
}

// Byte width of the utf-8 sequence starting with lead, or 0 if lead is not a valid lead byte
inline constexpr size_t Utf8SequenceWidth(Char8 lead)
{
    const uint8 b = uint8(lead);

    return b >= 0xF8 ? 0 : size_t((0x4322000011111111ull >> ((b >> 4) * 4)) & 0xF);
}

inline constexpr bool IsUtf8Continuation(Char8 ch)
{
    return (uint8(ch) & 0xC0) == 0x80;
}

inline bool IsAscii(const Char8* str, size_t size)
{
    uint64 bits = 0;
    size_t i = 0;

    for (; i + sizeof(uint64) <= size; i += sizeof(uint64))
    {
        uint64 word;
        std::memcpy(&word, str + i, sizeof(uint64));

        bits |= word;
    }

    for (; i < size; i++)
    {
        bits |= uint8(str[i]);
    }

    return (bits & 0x8080808080808080ull) == 0;
}

// Stops at a null terminator or truncated sequence, InvalidUTF8 on a bad lead or continuation byte
inline constexpr size_t StringLengthScalar(const Char8* first, const Char8* last, size_t& outBytes)
{
    size_t count = 0;
    const Char8* pos = first;

    while (pos != last && *pos != '\0')
    {
        const size_t width = Utf8SequenceWidth(*pos);

        if (width == 0)
        {
            return InvalidUTF8;
        }

        if (size_t(last - pos) < width)
        {
            break;
        }

        size_t i = 1;

        while (i < width && IsUtf8Continuation(pos[i]))
        {
            i++;
        }

        if (i != width)
        {
            if (pos[i] == '\0')
            {
                break;
            }

            return InvalidUTF8;
        }

        pos += width;
        count++;
    }

    outBytes = size_t(pos - first);

    return count;
}

inline constexpr size_t StringLengthSized(const Char8* str, size_t size, size_t& outBytes)
{
    if (!std::is_constant_evaluated() && IsAscii(str, size))
    {
        outBytes = size;

        return size;
    }

    return StringLengthScalar(str, str + size, outBytes);
}

inline constexpr size_t StringLength(const Char8* first, const Char8* last, size_t& outBytes)
{
    if (first == last)
    {
        outBytes = 0;

        return 0;
    }

    size_t size = size_t(last - first);

    if (!std::is_constant_evaluated())
    {
        if (const void* nul = std::memchr(first, 0, size))
        {
            size = size_t(static_cast<const Char8*>(nul) - first);
        }
    }

    return StringLengthSized(first, size, outBytes);
}

inline constexpr size_t StringLength(const Char8* first, const Char8* last)
{
    size_t bytes = 0;

    return StringLength(first, last, bytes);
}

inline constexpr size_t StringLength(const Char8* str, size_t& outBytes)
{
    return StringLengthSized(str, std::char_traits<Char8>::length(str), outBytes);
}

inline constexpr size_t TrimIncompleteUtf8(const Char8* str, size_t size)
{
    for (size_t back = 1; back <= 3 && back < size; back++)
    {
        const Char8 ch = str[size - back];

        if (!IsUtf8Continuation(ch))
        {
            return Utf8SequenceWidth(ch) > back ? size - back : size;
        }
    }

    return size;
}

inline size_t RemoveInvalidUtf8(Char8* str, size_t size)
{
    const Char8* pos = str;
    const Char8* const last = str + size;
    Char8* out = str;

    while (pos != last)
    {
        const size_t width = Utf8SequenceWidth(*pos);
        const size_t available = size_t(last - pos);

        size_t i = 1;

        while (i < width && i < available && IsUtf8Continuation(pos[i]))
        {
            i++;
        }

        if (width == 0 || i != width)
        {
            pos++;

            continue;
        }

        for (size_t j = 0; j < width; j++)
        {
            *out++ = *pos++;
        }
    }

    return size_t(out - str);
}

template <class T, bool IsUtf8>
static inline constexpr size_t StringLength(const T* str)
{
    if constexpr (IsUtf8)
    {
        size_t bytes = 0;
        return StringLength(str, bytes);
    }
    else
    {
        return std::char_traits<T>::length(str);
    }
}

template <class T, bool IsUtf8>
static inline constexpr size_t StringLength(const T* str, size_t& outCodepoints)
{
    if constexpr (IsUtf8)
    {
        return StringLength(str, outCodepoints);
    }
    else
    {
        return (outCodepoints = std::char_traits<T>::length(str));
    }
}

// utf-8 byte order matches codepoint order. count is in characters, 0 for unbounded
static inline int StringCompare(const Char8* s1, const Char8* s2, size_t count)
{
    if (count == 0)
    {
        const int result = std::strcmp(s1, s2);

        return (result > 0) - (result < 0);
    }

    size_t characters = 0;

    for (;; s1++, s2++)
    {
        if (!IsUtf8Continuation(*s1) && characters++ == count)
        {
            return 0;
        }

        const uint8 c1 = uint8(*s1);
        const uint8 c2 = uint8(*s2);

        if (c1 != c2)
        {
            return c1 < c2 ? -1 : 1;
        }

        if (c1 == 0)
        {
            return 0;
        }
    }
}

template <class T, bool IsUtf8>
static inline int StringCompare(const T* lhs, const T* rhs, size_t count)
{
    if constexpr (IsUtf8)
    {
        return StringCompare(lhs, rhs, count);
    }

    const T* s1 = lhs;
    const T* s2 = rhs;

    for (size_t i = 0; (*s1 || *s2) && (i < count || count == 0); s1++, s2++, i++)
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

/*! \brief Decode a single utf-8 character (multiple code units) into a utf-32 codepoint.
 *   Reads at most \p max bytes of \p str. On invalid or truncated input returns Char32(-1)
 *   and sets \p outCodepoints to 1 (0 if \p max is 0), so callers can skip the bad byte.
 */
static inline Char32 Char8to32(const Char8* str, size_t max, size_t& outCodepoints)
{
    if (max == 0)
    {
        outCodepoints = 0;

        return Char32(-1);
    }

    const uint8 lead = uint8(str[0]);

    if (lead < 0x80)
    {
        outCodepoints = 1;

        return Char32(lead);
    }

    const size_t width = Utf8SequenceWidth(Char8(lead));

    outCodepoints = 1;

    if (width == 0 || width > max)
    {
        return Char32(-1);
    }

    uint32 cp = lead & (0x7Fu >> width);

    for (size_t i = 1; i < width; i++)
    {
        if (!IsUtf8Continuation(str[i]))
        {
            return Char32(-1);
        }

        cp = (cp << 6) | (uint8(str[i]) & 0x3Fu);
    }

    outCodepoints = width;

    return Char32(cp);
}

/*! \brief Decode a single utf-8 character (multiple code units) into a utf-32 codepoint.
 *   \p str _must_ be at least sizeof(Char32), or null terminated
 */
static inline Char32 Char8to32(const Char8* str)
{
    size_t codepoints = 0;

    return Char8to32(str, sizeof(Char32), codepoints);
}

/*! \brief Convert a single UTF-32 codepoint into its UTF-8 encoded byte sequence.
 *  The array at \p dst MUST have a sizeof Char32 (4 bytes), which is enough to hold
 *  the longest possible UTF-8 encoding of any valid Unicode codepoint.
 */
static inline void Char32to8(Char32 src, Char8* dst, size_t& outCodepoints)
{
    // set all dst bytes to 0
    std::memset(dst, 0, sizeof(Char32));

    const uint32 codepoint = uint32(src);

    if (codepoint < 0x80)
    {
        dst[0] = Char8(codepoint);
        outCodepoints = 1;
    }
    else if (codepoint < 0x800)
    {
        dst[0] = Char8(0xC0 | (codepoint >> 6));
        dst[1] = Char8(0x80 | (codepoint & 0x3F));
        outCodepoints = 2;
    }
    else if (codepoint < 0x10000)
    {
        dst[0] = Char8(0xE0 | (codepoint >> 12));
        dst[1] = Char8(0x80 | ((codepoint >> 6) & 0x3F));
        dst[2] = Char8(0x80 | (codepoint & 0x3F));
        outCodepoints = 3;
    }
    else
    {
        dst[0] = Char8(0xF0 | (codepoint >> 18));
        dst[1] = Char8(0x80 | ((codepoint >> 12) & 0x3F));
        dst[2] = Char8(0x80 | ((codepoint >> 6) & 0x3F));
        dst[3] = Char8(0x80 | (codepoint & 0x3F));
        outCodepoints = 4;
    }
}

static inline void Char32to8(Char32 src, Char8* dst)
{
    size_t codepoints = 0;
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
static inline Char32 Char16to32(const Char16* str, size_t max, size_t& outCodeUnits)
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
static inline void Char32to16(Char32 src, Char16* dst, size_t& outCodeUnits)
{
    outCodeUnits = 0;

    if (src <= 0xFFFF)
    {
        // BMP character, single UTF-16 code unit
        dst[outCodeUnits++] = Char16(src);
    }
    else if (src <= 0x10FFFF)
    {
        // Supplementary character, needs surrogate pair
        const Char32 adjusted = src - 0x10000;
        dst[outCodeUnits++] = Char16((adjusted >> 10) + 0xD800);   // High surrogate
        dst[outCodeUnits++] = Char16((adjusted & 0x3FF) + 0xDC00); // Low surrogate
    }
    // else: invalid codepoint, outCodeUnits remains 0
}

static inline void Char32to16(Char32 src, Char16* dst)
{
    size_t codeUnits = 0;
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
        return Char32(*str);
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
static inline Char32 WideTo32(const wchar_t* str, size_t max, size_t& outCodeUnits)
{
    outCodeUnits = 0;

    if (max == 0)
    {
        return Char32(-1);
    }

    if constexpr (sizeof(wchar_t) == 4)
    {
        outCodeUnits = 1;
        return Char32(*str);
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
static inline void Char32ToWide(Char32 src, wchar_t* dst, size_t& outCodeUnits)
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
    size_t codeUnits = 0;
    Char32ToWide(src, dst, codeUnits);
}

/*! \brief Pass nullptr to \p result on the first call to get the size needed for the buffer.
 *  Then call the function again with the memory allocated for \p result. */
inline size_t ToUtf16(const Char32* start, const Char32* end, Char16* result)
{
    size_t len = 0;

    while (start != end)
    {
        const Char32 cp = *start++;

        if (cp <= 0xFFFF)
        {
            // BMP character
            if (result)
            {
                result[len] = Char16(cp);
            }
            len++;
        }
        else if (cp <= 0x10FFFF)
        {
            // Supplementary character, needs surrogate pair
            if (result)
            {
                const Char32 adjusted = cp - 0x10000;
                result[len] = Char16((adjusted >> 10) + 0xD800);       // High surrogate
                result[len + 1] = Char16((adjusted & 0x3FF) + 0xDC00); // Low surrogate
            }
            len += 2;
        }
        // else: invalid codepoint, skip
    }

    return len;
}

/*! \brief Pass nullptr to \p result on the first call to get the size needed for the buffer.
 *  Then call the function again with the memory allocated for \p result. */
inline size_t ToUtf16(const Char8* start, const Char8* end, Char16* result)
{
    size_t len = 0;

    while (start != end)
    {
        size_t codepoints = 0;
        const Char32 cp = Char8to32(start, size_t(end - start), codepoints);

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
                result[len] = Char16(cp);
            }
            len++;
        }
        else if (cp <= 0x10FFFF)
        {
            // Supplementary character, needs surrogate pair
            if (result)
            {
                const Char32 adjusted = cp - 0x10000;
                result[len] = Char16((adjusted >> 10) + 0xD800);       // High surrogate
                result[len + 1] = Char16((adjusted & 0x3FF) + 0xDC00); // Low surrogate
            }
            len += 2;
        }
    }

    return len;
}

/*! \brief Pass nullptr to \p result on the first call to get the size needed for the buffer.
 *  Then call the function again with the memory allocated for \p result. */
inline size_t ToUtf16(const wchar_t* start, const wchar_t* end, Char16* result)
{
    size_t len = 0;

    while (start != end)
    {
        const wchar_t ch = *start++;

        if constexpr (sizeof(wchar_t) == 2)
        {
            // wchar_t is already UTF-16 (Windows)
            if (result)
            {
                result[len] = Char16(ch);
            }
            len++;
        }
        else
        {
            // wchar_t is UTF-32 (Unix/macOS)
            const Char32 cp = Char32(ch);

            if (cp <= 0xFFFF)
            {
                if (result)
                {
                    result[len] = Char16(cp);
                }
                len++;
            }
            else if (cp <= 0x10FFFF)
            {
                if (result)
                {
                    const Char32 adjusted = cp - 0x10000;
                    result[len] = Char16((adjusted >> 10) + 0xD800);
                    result[len + 1] = Char16((adjusted & 0x3FF) + 0xDC00);
                }
                len += 2;
            }
        }
    }

    return len;
}

inline Char32 CharAt(const utf::Char8* str, size_t max, size_t index)
{
    size_t characterIndex = 0;

    for (size_t i = 0; i < max; i++)
    {
        if (IsUtf8Continuation(str[i]))
        {
            continue;
        }

        if (characterIndex++ == index)
        {
            size_t codepoints = 0;

            return Char8to32(str + i, max - i, codepoints);
        }

        if (str[i] == '\0')
        {
            return Char32(-1);
        }
    }

    return Char32(-1);
}

/*! \brief Get the UTF-8 char (array of code points) at the specific index of the string.
 *  \p dst MUST have a size of at least the sizeof Char32, so 4 bytes.
 */
inline void CharAt(const Char8* str, Char8* dst, size_t max, size_t index)
{
    Char32to8(CharAt(str, max, index), dst);
}

inline size_t NextCodePoint(uint32 cp, Char8*& result)
{
    if (result)
    {
        size_t len = 0;

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
inline size_t ToUtf8(const Char16* start, const Char16* end, Char8* result)
{
    size_t len = 0;

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
inline size_t ToUtf8(const Char32* start, const Char32* end, Char8* result)
{
    size_t len = 0;

    while (start != end)
    {
        uint32 cp = *start++;

        len += NextCodePoint(cp, result);
    }

    return len;
}

/*! \brief Pass nullptr to \p result on the first call to get the size needed for the buffer.
 * *  Then call the function again with the memory allocated for \p result. */
inline size_t ToUtf8(const wchar_t* start, const wchar_t* end, Char8* result)
{
#ifdef _WIN32
    size_t len = 0;

    if (result)
    {
        len = size_t(WideCharToMultiByte(CP_UTF8, 0, start, (int)(end - start), (char*)result, 0, NULL, NULL));

        if (len == 0)
        {
            return 0;
        }

        WideCharToMultiByte(CP_UTF8, 0, start, (int)(end - start), (char*)result, int(len), NULL, NULL);
    }
    else
    {
        len = size_t(WideCharToMultiByte(CP_UTF8, 0, start, (int)(end - start), NULL, 0, NULL, NULL));
    }

    return len;
#else
    size_t len = 0;

    if (result)
    {
        while (start != end)
        {
            wchar_t ch = *start++;

            if (ch <= 0x7F)
            {
                result[len++] = Char8(ch);
            }
            else if (ch <= 0x7FF)
            {
                result[len++] = Char8(0xC0 | ((ch >> 6) & 0x1F));
                result[len++] = Char8(0x80 | (ch & 0x3F));
            }
            else if (ch <= 0xFFFF)
            {
                result[len++] = Char8(0xE0 | ((ch >> 12) & 0x0F));
                result[len++] = Char8(0x80 | ((ch >> 6) & 0x3F));
                result[len++] = Char8(0x80 | (ch & 0x3F));
            }
            else if (ch <= 0x10FFFF)
            {
                result[len++] = Char8(0xF0 | ((ch >> 18) & 0x07));
                result[len++] = Char8(0x80 | ((ch >> 12) & 0x3F));
                result[len++] = Char8(0x80 | ((ch >> 6) & 0x3F));
                result[len++] = Char8(0x80 | (ch & 0x3F));
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

inline size_t ToWide(const Char8* start, const Char8* end, wchar_t* result)
{
    size_t len = 0;

#ifdef _WIN32
    if (result)
    {
        len = size_t(MultiByteToWideChar(CP_UTF8, 0, start, (int)(end - start), NULL, 0));

        if (len == 0)
        {
            return 0;
        }

        MultiByteToWideChar(CP_UTF8, 0, start, (int)(end - start), result, int(len));
    }
    else
    {
        len = size_t(MultiByteToWideChar(CP_UTF8, 0, start, (int)(end - start), NULL, 0));
    }
#else
    if (result)
    {
        while (start != end)
        {
            Char32 ch = 0;
            size_t codepoints = 0;

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
            size_t codepoints = 0;

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

inline size_t ToWide(const Char16* start, const Char16* end, wchar_t* result)
{
    const size_t len = size_t(end - start);

    if (result)
    {
        for (size_t i = 0; i < len; i++)
        {
            result[i] = (wchar_t)start[i];
        }
    }

    return size_t(len);
}

inline size_t ToWide(const Char32* start, const Char32* end, wchar_t* result)
{
    size_t len = 0;

    if constexpr (sizeof(wchar_t) == 4)
    {
        // wchar_t is UTF-32 (Unix/macOS), direct copy
        len = size_t(end - start);

        if (result)
        {
            for (size_t i = 0; i < len; i++)
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
inline size_t ToUtf32(const Char8* start, const Char8* end, Char32* result)
{
    size_t len = 0;

    while (start != end)
    {
        size_t codepoints = 0;
        const Char32 cp = Char8to32(start, size_t(end - start), codepoints);

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
inline size_t ToUtf32(const Char16* start, const Char16* end, Char32* result)
{
    size_t len = 0;

    while (start != end)
    {
        size_t codeUnits = 0;
        const Char32 cp = Char16to32(start, size_t(end - start), codeUnits);

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
inline size_t ToUtf32(const wchar_t* start, const wchar_t* end, Char32* result)
{
    size_t len = 0;

    if constexpr (sizeof(wchar_t) == 4)
    {
        // wchar_t is UTF-32 (Unix/macOS), direct copy
        len = size_t(end - start);

        if (result)
        {
            for (size_t i = 0; i < len; i++)
            {
                result[i] = Char32(start[i]);
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
                    result[len] = Char32(cp);
                }
                len++;
                start++;
            }
        }
    }

    return len;
}

namespace detail {

// the unsigned type that holds the magnitude of every value of T, so negating the minimum of a signed type doesn't overflow
template <class T, bool = std::is_integral_v<T> && !std::is_same_v<T, bool>>
struct ToStringMagnitude
{
    using Type = T;
};

template <class T>
struct ToStringMagnitude<T, true>
{
    using Type = std::make_unsigned_t<T>;
};

} // namespace detail

/*! \brief How to use:
    if buffer length is not known, pass nullptr for \p result.
    bufferLength will be set to the size needed for \p result.
    Next, call the function again, passing in the previously mentioned
    value for \p bufferLength. The resulting string will be written into the provided
    param, \p result, so it'll need to have \p bufferLength bytes allocated to it. */
template <class T, class CharType>
inline void ToString(T signedValue, size_t& bufferLength, CharType* result)
{
    using Magnitude = typename detail::ToStringMagnitude<T>::Type;

    Magnitude value = Magnitude(signedValue);
    Magnitude divisor = 1;
    bool isNegative = 0;
    size_t bufferIndex = 0;

    if (signedValue < 0)
    {
        isNegative = true;
        value = Magnitude(0) - value;
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
    size_t bufferLengthRemaining = bufferLength - 1;

    if (isNegative)
    {
        HYP_UTF8_ASSERT(bufferLength != 1);
        result[bufferIndex++] = CharType('-');

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

/*! \brief Encode a single UTF-32 codepoint as UTF-8, in place, into \p ch's own storage
 *  (which is large enough to hold the longest possible encoding) and NUL-terminate it.
 *  The returned pointer aliases \p ch, and is only valid as long as \p ch is alive. */
inline char* ToUtf8Chars(Char32& ch)
{
    Char8* dst = reinterpret_cast<Char8*>(&ch);

    size_t outCodepoints = 0;
    Char32to8(ch, dst, outCodepoints);

    return reinterpret_cast<char*>(dst);
}

} // namespace utf
} // namespace Hyperion
