#ifndef HYPERION_V2_LIB_FIXED_STRING_H
#define HYPERION_V2_LIB_FIXED_STRING_H

#include <algorithm>
#include <cstring>

namespace hyperion::v2 {

class FixedString {
public:
    FixedString(const char *str = nullptr)
        : m_str(nullptr),
          m_length(0)
    {
        if (str != nullptr) {
            m_length = std::strlen(str);
            m_str = new char[m_length + 1];
            std::strcpy(m_str, str);
        }
    }

    FixedString(const FixedString &other)
        : m_str(nullptr),
          m_length(other.m_length)
    {
        if (other.m_str != nullptr) {
            m_str = new char[m_length + 1];
            std::strcpy(m_str, other.m_str);
        }
    }

    FixedString &operator=(const FixedString &other)
    {
        if (m_str != nullptr) {
            delete[] m_str;
            m_str = nullptr;
        }

        m_length = other.m_length;

        if (other.m_str != nullptr) {
            m_str = new char[m_length + 1];
            std::strcpy(m_str, other.m_str);
        }

        return *this;
    }

    FixedString(FixedString &&other) noexcept
        : m_str(other.m_str),
          m_length(other.m_length)
    {
        other.m_str    = nullptr;
        other.m_length = 0;
    }

    FixedString &operator=(FixedString &&other) noexcept
    {
        if (m_str != nullptr) {
            delete[] m_str;

            m_str    = nullptr;
            m_length = 0;
        }

        std::swap(m_str, other.m_str);
        std::swap(m_length, other.m_length);

        return *this;
    }

    ~FixedString()
    {
        if (m_str != nullptr) {
            delete[] m_str;
        }
    }

    operator const char *() const       { return m_str; }

    char &operator[](size_t index)      { return m_str[index]; }
    char operator[](size_t index) const { return m_str[index]; }

    bool operator<(const FixedString &other) const
        { return m_str == nullptr ? true : bool(std::strcmp(m_str, other.m_str) < 0); }

    size_t Length() const       { return m_length; }
    const char *Data() const    { return m_str; }
    const char *CString() const { return m_str; }

    char *begin() { return m_str; }
    char *end()   { return m_str + m_length; }
    char *cbegin() const { return m_str; }
    char *cend() const   { return m_str + m_length; }

private:
    char *m_str;
    size_t m_length;
};

} // namespace hyperion::v2

#endif