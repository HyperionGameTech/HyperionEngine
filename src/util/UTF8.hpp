#ifndef UTF8_HPP
#define UTF8_HPP

#include <iostream>
#include <algorithm>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <cstring>

#include <core/Core.hpp>

// #ifdef __MINGW32__
// #undef _WIN32
// #endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX // do not allow windows.h to define 'max' and 'min'
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <cwchar>
#endif

namespace utf {

#ifdef _WIN32
typedef std::wstring stdstring;
typedef std::wostream utf8_ostream;
typedef std::wofstream utf8_ofstream;
typedef std::wistream utf8_istream;
typedef std::wifstream utf8_ifstream;
typedef std::wstringstream utf8_stringstream;
static utf8_ostream &cout = std::wcout;
static utf8_istream &cin = std::wcin;
static auto &printf = std::wprintf;
static auto &sprintf = wsprintf;
static auto &fputs = std::fputws;
#define PRIutf8s "ls"
#define HYP_UTF8_CSTR(str) L##str

inline std::vector<wchar_t> ToWide(const char *str)
{
    std::vector<wchar_t> buffer;
    buffer.resize(MultiByteToWideChar(CP_UTF8, 0, str, -1, 0, 0));
    MultiByteToWideChar(CP_UTF8, 0, str, -1, &buffer[0], static_cast<int>(buffer.size()));
    return buffer;
}

inline std::vector<char> ToMultiByte(const wchar_t *wstr)
{
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    std::vector<char> buffer(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &buffer[0], size_needed, NULL, NULL);
    return buffer;
}

#define HYP_UTF8_WIDE
#define HYP_UTF8_TOWIDE(str) utf::ToWide(str).data()
#define HYP_UTF8_TOMULTIBYTE(str) utf::ToMultiByte(str).data()

#else
typedef std::string stdstring;
typedef std::ostream utf8_ostream;
typedef std::ofstream utf8_ofstream;
typedef std::istream utf8_istream;
typedef std::ifstream utf8_ifstream;
typedef std::stringstream utf8_stringstream;
static utf8_ostream &cout = std::cout;
static utf8_istream &cin = std::cin;
static auto &printf = std::printf;
static auto &sprintf = std::sprintf;
static auto &fputs = std::fputs;
#define PRIutf8s "s"
#define HYP_UTF8_CSTR(str) (str)
#define HYP_UTF8_TOWIDE(str) (str)
#define HYP_UTF8_TOMULTIBYTE(str) (str)
#endif

using u32char = hyperion::UInt32;
using u16char = hyperion::UInt16;
using u8char = hyperion::UInt8;

#define HYP_UTF8_CHECK_BOUNDS(idx, max) \
    do { if ((idx) == (max)) { return -1; } } while (0)

inline void init()
{
#ifdef _WIN32
    _setmode(_fileno(stdout), 0x00020000/*_O_U16TEXT*/);
#endif
}

inline bool utf32_isspace(u32char ch)
{
    return ch == u32char(' ')  ||
           ch == u32char('\n') ||
           ch == u32char('\t') ||
           ch == u32char('\r');
}

inline bool utf32_isdigit(u32char ch)
{
    return (ch >= u32char('0')) && (ch <= u32char('9'));
}

inline bool utf32_isxdigit(u32char ch)
{
    return (ch >= u32char('0')) && (ch <= u32char('9'))
        || ((ch >= u32char('A') && ch <= u32char('F')))
        || ((ch >= u32char('a') && ch <= u32char('f')));
}

inline bool utf32_isalpha(u32char ch)
{
    return (ch >= 0xC0) || ((ch >= u32char('A') && ch <= u32char('Z')) ||
                            (ch >= u32char('a') && ch <= u32char('z')));
}

inline int utf8_strlen(const char *str, int *out_count = nullptr)
{
    const auto max = std::strlen(str);
    int count = 0;
    int i;

    for (i = 0; i < max; i++, count++) {
        unsigned char c = (unsigned char)str[i];
        if (c >= 0 && c <= 127) i += 0;
        else if ((c & 0xE0) == 0xC0) i += 1;
        else if ((c & 0xF0) == 0xE0) i += 2;
        else if ((c & 0xF8) == 0xF0) i += 3;
        else return -1;//invalid utf8
    }

    if (out_count) {
        *out_count = i;
    }

    return count;
}

inline int utf32_strlen(const u32char *str)
{
    int counter = 0;
    const u32char *pos = str;
    for (; *pos; ++pos, counter++);
    return counter;
}

template <class T, bool IsUtf8>
int utf_strlen(const T *str, int *out_count = nullptr)
{
    if constexpr (IsUtf8) {
        return utf8_strlen(str, out_count);
    }

    int counter = 0;
    const T *pos = str;
    for (; *pos; ++pos, counter++);

    if (out_count != nullptr) {
        *out_count = counter;
    }

    return counter;
}

inline int utf8_strcmp(const char *s1, const char *s2)
{
    for (; *s1 || *s2;) {
        unsigned char c;

        u32char c1 = 0;
        char *c1_bytes = reinterpret_cast<char*>(&c1);

        u32char c2 = 0;
        char *c2_bytes = reinterpret_cast<char*>(&c2);

        // get the character for s1
        c = (unsigned char)*s1;

        if (c >= 0 && c <= 127) {
            c1_bytes[0] = *(s1++);
        } else if ((c & 0xE0) == 0xC0) {
            c1_bytes[0] = *(s1++);
            c1_bytes[1] = *(s1++);
        } else if ((c & 0xF0) == 0xE0) {
            c1_bytes[0] = *(s1++);
            c1_bytes[1] = *(s1++);
            c1_bytes[2] = *(s1++);
        } else if ((c & 0xF8) == 0xF0) {
            c1_bytes[0] = *(s1++);
            c1_bytes[1] = *(s1++);
            c1_bytes[2] = *(s1++);
            c1_bytes[3] = *(s1++);
        }

        // get the character for s2
        c = (unsigned char)*s2;

        if (c >= 0 && c <= 127) {
            c2_bytes[0] = *(s2++);
        } else if ((c & 0xE0) == 0xC0) {
            c2_bytes[0] = *(s2++);
            c2_bytes[1] = *(s2++);
        } else if ((c & 0xF0) == 0xE0) {
            c2_bytes[0] = *(s2++);
            c2_bytes[1] = *(s2++);
            c2_bytes[2] = *(s2++);
        } else if ((c & 0xF8) == 0xF0) {
            c2_bytes[0] = *(s2++);
            c2_bytes[1] = *(s2++);
            c2_bytes[2] = *(s2++);
            c2_bytes[3] = *(s2++);
        }

        if (c1 < c2) {
            return -1;
        } else if (c1 > c2) {
            return 1;
        }
    }

    return 0;
}

inline int utf32_strcmp(const u32char *lhs, const u32char *rhs)
{
    const u32char *s1 = lhs;
    const u32char *s2 = rhs;

    for (; *s1 || *s2; s1++, s2++) {
        if (*s1 < *s2) {
            return -1;
        } else if (*s1 > *s2) {
            return 1;
        }
    }

    return 0;
}

template <class T, bool IsUtf8>
int utf_strcmp(const T *lhs, const T *rhs)
{
    if constexpr (IsUtf8) {
        return utf8_strcmp(lhs, rhs);
    }

    const T *s1 = lhs;
    const T *s2 = rhs;

    for (; *s1 || *s2; s1++, s2++) {
        if (*s1 < *s2) {
            return -1;
        } else if (*s1 > *s2) {
            return 1;
        }
    }

    return 0;
}

inline char *utf8_strcpy(char *dst, const char *src)
{
    // copy all raw bytes
    for (int i = 0; (dst[i] = src[i]) != '\0'; i++);
    return dst;
}

inline u32char *utf32_strcpy(u32char *dst, const u32char *src)
{
    for (int i = 0; (dst[i] = src[i]) != '\0'; i++);
    return dst;
}

inline char *utf8_strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    size_t count = 0;

    size_t max = std::strlen(dst) + 1;

    for (; src[i] != '\0'; i++, count++) {

        if (count == n) {
            // finished copying, jump to end
            break;
        }

        unsigned char c = (unsigned char)src[i];

        if (c >= 0 && c <= 127) {
            dst[i] = src[i];
        } else if ((c & 0xE0) == 0xC0) {
            dst[i] = src[i];
            dst[i + 1] = src[i + 1];
            i += 1;
        } else if ((c & 0xF0) == 0xE0) {
            dst[i] = src[i];
            dst[i + 1] = src[i + 1];
            dst[i + 2] = src[i + 2];
            i += 2;
        } else if ((c & 0xF8) == 0xF0) {
            dst[i] = src[i];
            dst[i + 1] = src[i + 1];
            dst[i + 2] = src[i + 2];
            dst[i + 3] = src[i + 3];
            i += 3;
        } else {
            break; // invalid utf-8
        }
    }

    // fill rest with NUL bytes
    for (; i < max; i++) {
        dst[i] = '\0';
    }

    return dst;
}

inline u32char *utf32_strncpy(u32char *s1, const u32char *s2, size_t n)
{
    u32char *s = s1;
    while (n > 0 && *s2 != '\0') {
        *s++ = *s2++;
        --n;
    }
    while (n > 0) {
        *s++ = '\0';
        --n;
    }
    return s1;
}

inline char *utf8_strcat(char *dst, const char *src)
{
    while (*dst) {
        dst++;
    }
    while ((*dst++ = *src++));
    return --dst;
}

inline u32char *utf32_strcat(u32char *dst, const u32char *src)
{
    while (*dst) {
        dst++;
    }
    while ((*dst++ = *src++));
    return --dst;
}

/*! \brief Convert a single utf-8 character (multiple code units) into a single utf-32 char
 *   \ref{str} _must_ be at least the the size of `max` (defaults to sizeof(u32char))
 */
inline u32char char8to32(const char *str, hyperion::SizeType max = sizeof(u32char))
{
    union { u32char ret; char ret_bytes[sizeof(u32char)]; };
    ret = 0;

    hyperion::UInt i = 0;

    const unsigned char ch = (unsigned char)str[0];

    if (ch <= 0x7F) {
        ret_bytes[0] = str[i];
    } else if ((ch & 0xE0) == 0xC0) {
        ret_bytes[0] = str[i++];
        HYP_UTF8_CHECK_BOUNDS(i, max);
        ret_bytes[1] = str[i];
    } else if ((ch & 0xF0) == 0xE0) {
        ret_bytes[0] = str[i++];
        HYP_UTF8_CHECK_BOUNDS(i, max);
        ret_bytes[1] = str[i++];
        HYP_UTF8_CHECK_BOUNDS(i, max);
        ret_bytes[2] = str[i];
    } else if ((ch & 0xF8) == 0xF0) {
        ret_bytes[0] = str[i++];
        HYP_UTF8_CHECK_BOUNDS(i, max);
        ret_bytes[1] = str[i++];
        HYP_UTF8_CHECK_BOUNDS(i, max);
        ret_bytes[2] = str[i++];
        HYP_UTF8_CHECK_BOUNDS(i, max);
        ret_bytes[3] = str[i];
    } else {
        // invalid utf-8
        return -1;
    }

    return ret;
}

/*! \brief Convert a single utf-8 character (multiple code units) into a single utf-32 char
 *   \ref{str} _must_ be at least the the size of `max` (defaults to sizeof(u32char))
 */
inline u32char char8to32(const char *str, hyperion::SizeType max, hyperion::UInt8 *num_bytes_read_ptr)
{
    union { u32char ret; char ret_bytes[sizeof(u32char)]; };
    ret = 0;

    hyperion::UInt8 num_bytes_read = 0;

    if (!num_bytes_read_ptr) {
        num_bytes_read_ptr = &num_bytes_read;
    }

    *num_bytes_read_ptr = 0;

    const unsigned char ch = (unsigned char)str[0];

    if (ch <= 0x7F) {
        ret_bytes[0] = str[(*num_bytes_read_ptr)++];
    } else if ((ch & 0xE0) == 0xC0) {
        ret_bytes[0] = str[(*num_bytes_read_ptr)++];
        HYP_UTF8_CHECK_BOUNDS(*num_bytes_read_ptr, max);
        ret_bytes[1] = str[(*num_bytes_read_ptr)++];
    } else if ((ch & 0xF0) == 0xE0) {
        ret_bytes[0] = str[(*num_bytes_read_ptr)++];
        HYP_UTF8_CHECK_BOUNDS(*num_bytes_read_ptr, max);
        ret_bytes[1] = str[(*num_bytes_read_ptr)++];
        HYP_UTF8_CHECK_BOUNDS(*num_bytes_read_ptr, max);
        ret_bytes[2] = str[(*num_bytes_read_ptr)++];
    } else if ((ch & 0xF8) == 0xF0) {
        ret_bytes[0] = str[(*num_bytes_read_ptr)++];
        HYP_UTF8_CHECK_BOUNDS(*num_bytes_read_ptr, max);
        ret_bytes[1] = str[(*num_bytes_read_ptr)++];
        HYP_UTF8_CHECK_BOUNDS(*num_bytes_read_ptr, max);
        ret_bytes[2] = str[(*num_bytes_read_ptr)++];
        HYP_UTF8_CHECK_BOUNDS(*num_bytes_read_ptr, max);
        ret_bytes[3] = str[(*num_bytes_read_ptr)++];
    } else {
        // invalid utf-8
        return -1;
    }

    return ret;
}

/*! \brief Convert a single UTF-32 char to UTF-8 array of code points.
 *  \ref{dst} MUST have a size of at least 4 bytes.
 */
inline void char32to8(u32char src, char *dst, int *out_length = nullptr)
{
    std::memset(dst, 0x00, sizeof(u32char));

    const char *src_bytes = reinterpret_cast<char *>(&src);

    if (out_length) {
        *out_length = 0;
    }

    for (hyperion::SizeType i = 0; i < sizeof(u32char); i++) {
        if (src_bytes[i] == 0) {
            // stop reading
            break;
        }

        dst[i] = src_bytes[i];

        if (out_length) {
            (*out_length)++;
        }
    }
}

inline char *get_bytes(u32char &ch) { return reinterpret_cast<char*>(&ch); }

inline void utf8to32(const char *src, u32char *dst, int size)
{
    int max = size * (int)sizeof(u32char);
    char *dst_bytes = reinterpret_cast<char*>(dst);

    const char *pos = src;

    for (int i = 0; *pos && i < max; i += sizeof(u32char)) {
        unsigned char c = (unsigned char)*pos;

        if (c >= 0 && c <= 127) {
            dst_bytes[i] = *(pos++);
        } else if ((c & 0xE0) == 0xC0) {
            dst_bytes[i] = *(pos++);
            dst_bytes[i + 1] = *(pos++);
        } else if ((c & 0xF0) == 0xE0) {
            dst_bytes[i] = *(pos++);
            dst_bytes[i + 1] = *(pos++);
            dst_bytes[i + 2] = *(pos++);
        } else if ((c & 0xF8) == 0xF0) {
            dst_bytes[i] = *(pos++);
            dst_bytes[i + 1] = *(pos++);
            dst_bytes[i + 2] = *(pos++);
            dst_bytes[i + 3] = *(pos++);
        } else {
            // invalid utf-8
            break;
        }
    }
}


inline u32char utf8_charat(const char *str, hyperion::SizeType max, hyperion::SizeType index)
{
    hyperion::SizeType character_index = 0;

    for (hyperion::SizeType i = 0; i < max; character_index++) {
        u8char c(str[i]);

        union { u32char ret; char ret_bytes[sizeof(u32char)]; };
        ret = 0;

        if (c <= 0x7F) {
            ret_bytes[0] = str[i++];
        } else if ((c & 0xE0) == 0xC0) {
            ret_bytes[0] = str[i++];
            HYP_UTF8_CHECK_BOUNDS(i, max);
            ret_bytes[1] = str[i++];
        } else if ((c & 0xF0) == 0xE0) {
            ret_bytes[0] = str[i++];
            HYP_UTF8_CHECK_BOUNDS(i, max);
            ret_bytes[1] = str[i++];
            HYP_UTF8_CHECK_BOUNDS(i, max);
            ret_bytes[2] = str[i++];
        } else if ((c & 0xF8) == 0xF0) {
            ret_bytes[0] = str[i++];
            HYP_UTF8_CHECK_BOUNDS(i, max);
            ret_bytes[1] = str[i++];
            HYP_UTF8_CHECK_BOUNDS(i, max);
            ret_bytes[2] = str[i++];
            HYP_UTF8_CHECK_BOUNDS(i, max);
            ret_bytes[3] = str[i++];
        } else {
            // invalid utf-8
            return -1;
        }

        if (character_index == index) {
            // reached index
            return ret;
        }

        // end reached
        if (c == u8char('\0')) {
            return -1;
        }
    }

    // error
    return -1;
}

/*! \brief Get the UTF-8 char (array of code points) at the specific index of the string.
 *  \ref{dst} MUST have a size of at least the sizeof u32char, so 4 bytes.
 */
inline void utf8_charat(const char *str, char *dst, hyperion::SizeType max, hyperion::SizeType index)
    { char32to8(utf8_charat(str, max, index), dst); }

#define HYP_UTF_MASK16(ch)             ((uint16_t)(0xffff & (ch)))
#define HYP_UTF_IS_LEAD_SURROGATE(ch)  ((ch) >= 0xd800u && (ch) <= 0xdbffu)
#define HYP_UTF_IS_TRAIL_SURROGATE(ch) ((ch) >= 0xdc00u && (ch) <= 0xdfffu)
#define HYP_UTF_SURROGATE_OFFSET       (0x10000u - (0xd800u << 10) - 0xdc00u)

using namespace hyperion;

inline u8char *utf8_append(uint32_t cp, u8char *result)
{
    if (cp < 0x80) {
        *(result++) = static_cast<u8char>(cp);
    } else if (cp < 0x800) {
        *(result++) = static_cast<u8char>((cp >> 6)            | 0xc0);
        *(result++) = static_cast<u8char>((cp & 0x3f)          | 0x80);
    } else if (cp < 0x10000) {
        *(result++) = static_cast<u8char>((cp >> 12)           | 0xe0);
        *(result++) = static_cast<u8char>(((cp >> 6) & 0x3f)   | 0x80);
        *(result++) = static_cast<u8char>((cp & 0x3f)          | 0x80);
    } else {
        *(result++) = static_cast<u8char>((cp >> 18)           | 0xf0);
        *(result++) = static_cast<u8char>(((cp >> 12) & 0x3f)  | 0x80);
        *(result++) = static_cast<u8char>(((cp >> 6) & 0x3f)   | 0x80);
        *(result++) = static_cast<u8char>((cp & 0x3f)          | 0x80);
    }

    return result;
}


inline u8char *utf16to8(u16char *start, u16char *end, u8char *result)
{
    while (start != end) {
        UInt32 cp = HYP_UTF_MASK16(*start++);
        // Take care of surrogate pairs first
        if (HYP_UTF_IS_LEAD_SURROGATE(cp)) {
            const UInt32 trail_surrogate = HYP_UTF_MASK16(*start++);
            AssertThrow(HYP_UTF_IS_TRAIL_SURROGATE(trail_surrogate));
            cp = (cp << 10) + trail_surrogate + HYP_UTF_SURROGATE_OFFSET;
        } else {
            // Lone trail surrogate
            AssertThrow(!HYP_UTF_IS_TRAIL_SURROGATE(cp));
        }

        result = utf8_append(cp, result);
    }

    return result;
}

/*! \brief How to use:
    if buffer length is not known, pass nullptr for \ref{result}.
    buffer_length will be set to the size needed for \ref{result}.
    Next, call the function again, passing in the previously mentioned
    value for \ref{buffer_length}. The resulting string will be written into the provided
    param, \ref{result}, so it'll need to have \ref{buffer_length} bytes allocated to it. */
template <class T, class CharType>
inline void utf_to_str(T value, SizeType &buffer_length, CharType *result)
{
    T divisor = 1;
    bool is_negative = 0;
    SizeType buffer_index = 0;

    if (value < 0) {
        is_negative = true;
    }

    if (result == nullptr) {
        // first call
        buffer_length = 1;

        while (value / divisor >= 10) {
            divisor *= 10;
            ++buffer_length;
        }

        // for negative sign
        if (is_negative) {
            ++buffer_length;
        }

        // for null char
        ++buffer_length;

        return; // return after writing buffer_length
    }

    AssertThrow(buffer_length != 0);

    // don't modify passed in value any more
    SizeType buffer_length_remaining = buffer_length - 1;

    if (is_negative) {
        AssertThrow(buffer_length != 1);
        result[buffer_index++] = CharType('-');
        value *= -1;

        --buffer_length_remaining;
    }

    while (value / divisor >= 10) {
        divisor *= 10;
    }
    
    while (buffer_length_remaining) {
        // ASCII table has the number characters in sequence from 0-9 so use the
        // ASCII value of '0' as the base
        result[buffer_index++] = CharType(T('0') + value / divisor);
        
        // This removes the most significant digit converting 1337 to 337 because
        // 1337 % 1000 = 337
        value = value % divisor;
        
        // Adjust the divisor to next lowest position
        divisor = divisor / 10;

        --buffer_length_remaining;
    }
    
    // NULL terminate the string
    result[buffer_index] = 0;
}

#if 0

/*! \brief How to use:
    if buffer length is not known, pass nullptr for \ref{result}.
    buffer_length will be set to the size needed for \ref{result}.
    Next, call the function again, passing in the previously mentioned
    value for \ref{buffer_length}. The resulting string will be written into the provided
    param, \ref{result}, so it'll need to have \ref{buffer_length} bytes allocated to it. */
void utf8_base64encode(const char *src, SizeType src_length, char *dst, SizeType dst_length)
{
    static const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    SizeType buffer_index = 0;
    SizeType buffer_length_remaining = dst_length - 1;

    SizeType i = 0;
    while (i < src_length) {
        u8char bytes[3] = {0, 0, 0};
        SizeType bytes_read = 0;

        for (SizeType j = 0; j < 3; j++) {
            if (i < src_length) {
                bytes[j] = src[i++];
                bytes_read++;
            }
        }

        if (bytes_read) {
            u32char buffer = 0;
            for (SizeType j = 0; j < bytes_read; j++) {
                buffer = (buffer << 8) | bytes[j];
            }

            for (SizeType j = 0; j < 4; j++) {
                if (buffer_length_remaining) {
                    dst[buffer_index++] = base64_chars[(buffer >> 18) & 0x3F];
                    buffer <<= 6;
                    buffer_length_remaining--;
                }
            }
        }
    }

    if (buffer_length_remaining) {
        dst[buffer_index++] = 0;
    }
}

/*! \brief How to use:
    if buffer length is not known, pass nullptr for \ref{result}.
    buffer_length will be set to the size needed for \ref{result}.
    Next, call the function again, passing in the previously mentioned
    value for \ref{buffer_length}. The resulting string will be written into the provided
    param, \ref{result}, so it'll need to have \ref{buffer_length} bytes allocated to it. */
void utf8_base64decode(const char *src, SizeType src_length, char *dst, SizeType dst_length)
{
    static const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    SizeType buffer_index = 0;
    SizeType buffer_length_remaining = dst_length - 1;

    SizeType i = 0;
    while (i < src_length) {
        u8char bytes[4] = {0, 0, 0, 0};
        SizeType bytes_read = 0;

        for (SizeType j = 0; j < 4; j++) {
            if (i < src_length) {
                bytes[j] = src[i++];
                bytes_read++;
            }
        }

        if (bytes_read) {
            u32char buffer = 0;
            for (SizeType j = 0; j < bytes_read; j++) {
                buffer = (buffer << 6) | bytes[j];
            }

            for (SizeType j = 0; j < 3; j++) {
                if (buffer_length_remaining) {
                    dst[buffer_index++] = (buffer >> 16) & 0xFF;
                    buffer <<= 8;
                    buffer_length_remaining--;
                }
            }
        }
    }

    if (buffer_length_remaining) {
        dst[buffer_index++] = 0;
    }
}

#endif

} // namespace utf

#undef NOMINMAX

#endif
