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
static utf8_ostream &cout = std::wcout;
static utf8_istream &cin = std::wcin;
static auto &printf = std::wprintf;
static auto &sprintf = wsprintf;
static auto &fputs = std::fputws;
#define PRIutf8s "ls"
#define UTF8_CSTR(str) L##str

inline std::vector<wchar_t> ToWide(const char *str)
{
    std::vector<wchar_t> buffer;
    buffer.resize(MultiByteToWideChar(CP_UTF8, 0, str, -1, 0, 0));
    MultiByteToWideChar(CP_UTF8, 0, str, -1, &buffer[0], buffer.size());
    return buffer;
}

#define UTF8_TOWIDE(str) utf::ToWide(str).data()

#else
typedef std::string stdstring;
typedef std::ostream utf8_ostream;
typedef std::ofstream utf8_ofstream;
typedef std::istream utf8_istream;
typedef std::wifstream utf8_ifstream;
static utf8_ostream &cout = std::cout;
static utf8_istream &cin = std::cin;
static auto &printf = std::printf;
static auto &sprintf = std::sprintf;
static auto &fputs = std::fputs;
#define PRIutf8s "s"
#define UTF8_CSTR(str) str
#define UTF8_TOWIDE(str) str
#endif

typedef uint32_t u32char;

inline void init()
{
#ifdef _WIN32
    _setmode(_fileno(stdout), 0x00020000/*_O_U16TEXT*/);
#endif
}

inline bool utf32_isspace(u32char ch)
{
    return ch == (u32char)' '  ||
           ch == (u32char)'\n' ||
           ch == (u32char)'\t' ||
           ch == (u32char)'\r';
}

inline bool utf32_isdigit(u32char ch) { return (ch >= (u32char)'0') && (ch <= (u32char)'9'); }

inline bool utf32_isalpha(u32char ch)
{
    return (ch >= 0xC0) || ((ch >= (u32char)'A' && ch <= (u32char)'Z') ||
                            (ch >= (u32char)'a' && ch <= (u32char)'z'));
}

inline int utf8_strlen(const char *str)
{
    int max = std::strlen(str);
    int count = 0;

    for (int i = 0; i < max; i++, count++) {
        unsigned char c = (unsigned char)str[i];
        if (c >= 0 && c <= 127) i += 0;
        else if ((c & 0xE0) == 0xC0) i += 1;
        else if ((c & 0xF0) == 0xE0) i += 2;
        else if ((c & 0xF8) == 0xF0) i += 3;
        else return -1;//invalid utf8
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

inline u32char char8to32(const char *str)
{
    size_t num_bytes = std::strlen(str);
    if (num_bytes > sizeof(u32char)) {
        // not wide enough to store the character
        return -1;
    }

    u32char result = 0;

    char *result_bytes = reinterpret_cast<char*>(&result);
    for (size_t i = 0; i < num_bytes; i++) {
        result_bytes[i] = str[i];
    }

    return result;
}

inline void char32to8(u32char src, char *dst)
{
    char *src_bytes = reinterpret_cast<char*>(&src);

    for (size_t i = 0; i < sizeof(u32char); i++) {
        if (src_bytes[i] == 0) {
            // stop reading
            break;
        }

        dst[i] = src_bytes[i];
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

inline u32char utf8_charat(const char *str, int index)
{
    int max = std::strlen(str);
    int count = 0;

    for (int i = 0; i < max; i++, count++) {
        unsigned char c = (unsigned char)str[i];

        u32char ret = 0;
        char *ret_bytes = reinterpret_cast<char*>(&ret);

        if (c >= 0 && c <= 127) {
            ret_bytes[0] = str[i];
        } else if ((c & 0xE0) == 0xC0) {
            ret_bytes[0] = str[i];
            ret_bytes[1] = str[i + 1];
            i += 1;
        } else if ((c & 0xF0) == 0xE0) {
            ret_bytes[0] = str[i];
            ret_bytes[1] = str[i + 1];
            ret_bytes[2] = str[i + 2];
            i += 2;
        } else if ((c & 0xF8) == 0xF0) {
            ret_bytes[0] = str[i];
            ret_bytes[1] = str[i + 1];
            ret_bytes[2] = str[i + 2];
            ret_bytes[3] = str[i + 3];
            i += 3;
        } else {
            // invalid utf-8
            break;
        }

        if (index == count) {
            // reached index
            return ret;
        }
    }
    // error
    return -1;
}

inline void utf8_charat(const char *str, char *dst, int index)
    { char32to8(utf8_charat(str, index), dst); }



class Utf8String {
public:
    Utf8String()
        : m_data(new char[1]),
          m_size(1),
          m_length(0)
    {
        m_data[0] = '\0';
    }

    explicit Utf8String(size_t size)
        : m_data(new char[size + 1]),
          m_size(size + 1),
          m_length(0)
    {
        std::memset(m_data, 0, m_size);
    }

    Utf8String(const char *str)
    {
        if (str == nullptr) {
            m_data = new char[1];
            m_data[0] = '\0';
            m_size = 1;
            m_length = 0;
        } else {
            // copy raw bytes
            m_size = std::strlen(str) + 1;
            m_data = new char[m_size];
            std::strcpy(m_data, str);
            // recalculate length
            m_length = utf8_strlen(m_data);
        }
    }

    Utf8String(const char *str, size_t size)
    {
        if (str == nullptr) {
            m_size = size;
            m_data = new char[m_size];
            m_data[0] = '\0';
            m_length = 0;
        } else {
            // copy raw bytes
            m_size = std::max(std::strlen(str), size) + 1;
            m_data = new char[m_size];
            std::strcpy(m_data, str);
            // recalculate length
            m_length = utf8_strlen(m_data);
        }
    }

    Utf8String(const Utf8String &other)
    {
        // copy raw bytes
        m_size = other.m_size;
        m_data = new char[m_size];
        std::strcpy(m_data, other.m_data);
        m_length = other.m_length;
    }

    ~Utf8String()
    {
        if (m_data != nullptr) {
            delete[] m_data;
        }
    }

    inline char *GetData() { return m_data; }
    inline char *GetData() const { return m_data; }
    inline size_t GetBufferSize() const { return m_size; }
    inline size_t GetLength() const { return m_length; }

    Utf8String &operator=(const char *str)
    {
        // check if there is enough space to not have to delete the data
        size_t len = std::strlen(str) + 1;
        if (m_data != nullptr && m_size >= len) {
            std::strcpy(m_data, str);
            m_length = utf8_strlen(m_data);
        } else {
            // must delete the data if not null
            if (m_data) {
                delete[] m_data;
            }

            if (!str) {
                m_size = 1;
                m_data = new char[m_size];
                m_data[0] = '\0';
                m_length = 0;
            } else {
                // copy raw bytes
                m_size = len;
                m_data = new char[m_size];
                std::strcpy(m_data, str);
                // recalculate length
                m_length = utf8_strlen(m_data);
            }
        }


        /*if (m_data != nullptr) {
            delete[] m_data;
        }

        if (str == nullptr) {
            m_data = new char[1];
            m_data[0] = '\0';
            m_size = 1;
            m_length = 0;
        } else {
            // copy raw bytes
            m_size = std::strlen(str) + 1;
            m_data = new char[m_size];
            std::strcpy(m_data, str);
            // recalculate length
            m_length = utf8_strlen(m_data);
        }*/

        return *this;
    }

    Utf8String &operator=(const Utf8String &other)
    {
        /*if (m_data != nullptr) {
            delete[] m_data;
        }

        // copy raw bytes
        m_size = std::strlen(other.m_data) + 1;
        m_data = new char[m_size];
        std::strcpy(m_data, other.m_data);
        m_length = other.m_length;

        return *this;*/

        return operator=(other.m_data);
    }

    inline bool operator==(const char *str) const
        { return !(strcmp(m_data, str)); }
    inline bool operator==(const Utf8String &other) const
        { return !(strcmp(m_data, other.m_data)); }
    inline bool operator<(const char *str) const
        { return (utf8_strcmp(m_data, str) == -1); }
    inline bool operator<(const Utf8String &other) const
        { return (utf8_strcmp(m_data, other.m_data) == -1); }
    inline bool operator>(const char *str) const
        { return (utf8_strcmp(m_data, str) == 1); }
    inline bool operator>(const Utf8String &other) const
        { return (utf8_strcmp(m_data, other.m_data) == 1); }

    inline bool operator<=(const char *str) const
    {
        int i = utf8_strcmp(m_data, str);
        return i == 0 || i == -1;
    }

    inline bool operator<=(const Utf8String &other) const
    {
        int i = utf8_strcmp(m_data, other.m_data);
        return i == 0 || i == -1;
    }

    inline bool operator>=(const char *str) const
    {
        int i = utf8_strcmp(m_data, str);
        return i == 0 || i == 1;
    }

    inline bool operator>=(const Utf8String &other) const
    {
        int i = utf8_strcmp(m_data, other.m_data);
        return i == 0 || i == 1;
    }

    Utf8String operator+(const char *str) const
    {
        Utf8String result(m_length + strlen(str));

        utf8_strcpy(result.m_data, m_data);
        utf8_strcat(result.m_data, str);

        // calculate length
        result.m_length = utf8_strlen(result.m_data);

        return result;
    }

    Utf8String operator+(const Utf8String &other) const
    {
        Utf8String result(m_length + other.m_length);

        utf8_strcpy(result.m_data, m_data);
        utf8_strcat(result.m_data, other.m_data);

        // calculate length
        result.m_length = utf8_strlen(result.m_data);

        return result;
    }

    Utf8String &operator+=(const char *str)
    {
        size_t this_len = std::strlen(m_data);
        size_t other_len = std::strlen(str);
        size_t new_size = this_len + other_len + 1;

        if (new_size <= m_size) {
            std::strcat(m_data, str);
        } else {
            // we must delete and recreate the array
            m_size = new_size;

            char *new_data = new char[m_size];
            std::strcpy(new_data, m_data);
            std::strcat(new_data, str);
            delete[] m_data;
            m_data = new_data;
        }

        // recalculate length
        m_length = utf8_strlen(m_data);

        return *this;
    }

    Utf8String &operator+=(const Utf8String &other)
    {
        return operator+=(other.m_data);
    }

    u32char operator[](size_t index) const
    {
        u32char result;
        if (m_data == nullptr || ((result = utf8_charat(m_data, index) == (u32char(-1))))) {
            throw std::out_of_range("index out of range");
        }
        return result;
    }

    friend utf8_ostream &operator<<(utf8_ostream &os, const Utf8String &str)
    {
    #ifdef _WIN32
        std::vector<wchar_t> buffer;
        buffer.resize(MultiByteToWideChar(CP_UTF8, 0, str.m_data, -1, 0, 0));
        MultiByteToWideChar(CP_UTF8, 0, str.m_data, -1, &buffer[0], buffer.size());
        os << buffer.data();
    #else
        os << str.m_data;
    #endif
        return os;
    }

private:
    char *m_data;
    size_t m_size; // buffer size (not length)
    size_t m_length;
};

} // namespace utf

#undef NOMINMAX

#endif
