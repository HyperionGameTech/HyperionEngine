#ifndef HYPERION_V2_LIB_FIXED_STRING_H
#define HYPERION_V2_LIB_FIXED_STRING_H

#include <algorithm>
#include <cstring>

#include <core/Core.hpp>

namespace hyperion::v2 {

class StringView
{
public:
    StringView(const char str[])
        : m_str(&str[0]),
          m_length(0)
    {
        m_length = std::strlen(str);
    }

    StringView(const StringView &other)
        : m_str(other.m_str),
          m_length(other.m_length)
    {
    }

    StringView &operator=(const StringView &other)
    {
        m_str = other.m_str;
        m_length = other.m_length;

        return *this;
    }

    StringView(StringView &&other) noexcept
        : m_str(other.m_str),
          m_length(other.m_length)
    {
        other.m_str = nullptr;
        other.m_length = 0;
    }

    StringView &operator=(StringView &&other) noexcept
    {
        m_str = other.m_str;
        m_length = other.m_length;

        other.m_str = nullptr;
        other.m_length = 0;

        return *this;
    }

    ~StringView() = default;

    explicit operator const char *() const { return m_str; }

    bool operator<(const StringView &other) const
        { return m_str == nullptr ? true : bool(std::strcmp(m_str, other.m_str) < 0); }

    SizeType Length() const { return m_length; }
    const char *Data() const { return m_str; }

    const char *begin() { return m_str; }
    const char *end() { return m_str + m_length; }
    const char *cbegin() const { return m_str; }
    const char *cend() const { return m_str + m_length; }

private:
    const char *m_str;
    SizeType m_length;
};

} // namespace hyperion::v2

#endif