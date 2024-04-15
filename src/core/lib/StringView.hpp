/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_LIB_STRING_VIEW_HPP
#define HYPERION_V2_LIB_STRING_VIEW_HPP

#include <core/lib/Memory.hpp>
#include <core/lib/String.hpp>
#include <core/lib/StaticString.hpp>

#include <HashCode.hpp>

#include <algorithm>
#include <cstring>

#include <Types.hpp>

namespace hyperion::v2 {

class StringView
{
public:
    using Iterator = const char *;
    using ConstIterator = const char *;

    StringView(const char str[])
        : m_str(&str[0]),
          m_length(0)
    {
        m_length = Memory::StrLen(str);
    }

    template <SizeType Sz>
    constexpr StringView(const StaticString<Sz> &str)
        : m_str(&str.data[0]),
          m_length(Sz)
    {
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

    [[nodiscard]]
    HYP_FORCE_INLINE
    explicit operator const char *() const
        { return m_str; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator==(const StringView &other) const
    {
        if (!m_str) {
            return !other.m_str;
        }

        return Memory::AreStaticStringsEqual(m_str, other.m_str);
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator!=(const StringView &other) const
        { return !operator==(other); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool operator<(const StringView &other) const
        { return m_str == nullptr ? true : bool(Memory::StrCmp(m_str, other.m_str) < 0); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    SizeType Length() const
        { return m_length; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const char *Data() const
        { return m_str; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(Begin(), End());
    }

    HYP_DEF_STL_BEGIN_END(
        m_str,
        m_str + m_length
    )

private:
    const char  *m_str;
    SizeType    m_length;
};

} // namespace hyperion::v2

#endif