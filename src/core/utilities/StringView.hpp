/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STRING_VIEW_HPP
#define HYPERION_STRING_VIEW_HPP

#include <core/memory/Memory.hpp>
#include <core/containers/String.hpp>
#include <core/containers/StaticString.hpp>
#include <core/memory/ByteBuffer.hpp>

#include <HashCode.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {
namespace utilities {

namespace detail {

using namespace containers::detail;

template <int string_type>
class StringView
{
public:
    using CharType = typename containers::detail::StringTypeImpl< string_type >::CharType;
    using WidestCharType = typename containers::detail::StringTypeImpl< string_type >::WidestCharType;

    template <int FirstStringType, int SecondStringType>
    friend constexpr bool operator<(const StringView<FirstStringType> &lhs, const StringView<SecondStringType> &rhs);

    template <int FirstStringType, int SecondStringType>
    friend constexpr bool operator<(const containers::detail::String<FirstStringType> &lhs, const StringView<SecondStringType> &rhs);

    template <int FirstStringType, int SecondStringType>
    friend constexpr bool operator<(const StringView<FirstStringType> &lhs, const containers::detail::String<SecondStringType> &rhs);

    template <int FirstStringType, int SecondStringType>
    friend constexpr bool operator==(const StringView<FirstStringType> &lhs, const StringView<SecondStringType> &rhs);

    template <int FirstStringType, int SecondStringType>
    friend constexpr bool operator==(const containers::detail::String<FirstStringType> &lhs, const StringView<SecondStringType> &rhs);

    template <int FirstStringType, int SecondStringType>
    friend constexpr bool operator==(const StringView<FirstStringType> &lhs, const containers::detail::String<SecondStringType> &rhs);

    using Iterator = const CharType *;
    using ConstIterator = const CharType *;

    static constexpr bool is_contiguous = true;

    static constexpr bool is_ansi = string_type == StringType::ANSI;
    static constexpr bool is_utf8 = string_type == StringType::UTF8;
    static constexpr bool is_utf16 = string_type == StringType::UTF16;
    static constexpr bool is_utf32 = string_type == StringType::UTF32;
    static constexpr bool is_wide = string_type == StringType::WIDE_CHAR;

    static_assert(!is_utf8 || (std::is_same_v<CharType, char> || std::is_same_v<CharType, unsigned char>), "UTF-8 Strings must have CharType equal to char or unsigned char");
    static_assert(!is_ansi || (std::is_same_v<CharType, char> || std::is_same_v<CharType, unsigned char>), "ANSI Strings must have CharType equal to char or unsigned char");
    static_assert(!is_utf16 || std::is_same_v<CharType, utf::u16char>, "UTF-16 Strings must have CharType equal to utf::u16char");
    static_assert(!is_utf32 || std::is_same_v<CharType, utf::u32char>, "UTF-32 Strings must have CharType equal to utf::u32char");
    static_assert(!is_wide || std::is_same_v<CharType, wchar_t>, "Wide Strings must have CharType equal to wchar_t");

    constexpr StringView()
        : m_begin(nullptr),
          m_end(nullptr),
          m_length(0)
    {
    }

    StringView(const detail::String< string_type > &str)
        : m_begin(str.Begin()),
          m_end(str.End()),
          m_length(str.Length())
    {
    }

    template <SizeType Sz>
    constexpr StringView(const CharType (&str)[Sz])
        : m_begin(&str[0]),
          m_end(&str[0] + Sz),
          m_length(utf::utf_strlen<CharType, is_utf8>(str))
    {
    }

    StringView(const CharType *str)
        : m_begin(str),
          m_end(nullptr),
          m_length(0)
    {
        int num_bytes = 0;
        m_length = utf::utf_strlen< CharType, is_utf8 >(str, num_bytes);
        m_end = m_begin + num_bytes;
    }

    template <SizeType Sz, std::enable_if_t<std::is_same_v<CharType, typename StaticString<Sz>::CharType>, int> = 0>
    constexpr StringView(const StaticString<Sz> &str)
        : m_begin(str.Begin()),
          m_end(str.End()),
          m_length(utf::utf_strlen< CharType, is_utf8 >(str.data))
    {
    }

    constexpr StringView(const ByteBuffer &byte_buffer)
        : m_begin(reinterpret_cast<const CharType *>(byte_buffer.Data())),
          m_end(reinterpret_cast<const CharType *>(byte_buffer.Data() + byte_buffer.Size())),
          m_length(utf::utf_strlen<CharType, is_utf8>(reinterpret_cast<const CharType *>(byte_buffer.Data())))
    {
    }

    constexpr StringView(ConstByteView byte_view)
        : m_begin(reinterpret_cast<const CharType *>(byte_view.Data())),
          m_end(reinterpret_cast<const CharType *>(byte_view.Data() + byte_view.Size())),
          m_length(utf::utf_strlen<CharType, is_utf8>(reinterpret_cast<const CharType *>(byte_view.Data())))
    {
    }

    constexpr StringView(const StringView &other)   = default;
    StringView &operator=(const StringView &other)  = default;

    constexpr StringView(StringView &&other) noexcept
        : m_begin(other.m_begin),
          m_end(other.m_end),
          m_length(other.m_length)
    {
        other.m_begin = nullptr;
        other.m_end = nullptr;
        other.m_length = 0;
    }

    StringView &operator=(StringView &&other) noexcept
    {
        m_begin = other.m_begin;
        m_end = other.m_end;
        m_length = other.m_length;

        other.m_begin = nullptr;
        other.m_end = nullptr;
        other.m_length = 0;

        return *this;
    }

    constexpr ~StringView() = default;

    HYP_NODISCARD HYP_FORCE_INLINE
    operator containers::detail::String<string_type>() const
        { return containers::detail::String<string_type>(Data()); }

    HYP_NODISCARD HYP_FORCE_INLINE
    constexpr explicit operator const CharType *() const
        { return m_begin; }

    /*! \brief Convenience operator overload to return the raw string using the dereference operator
     *  \returns The raw string that the StringView has a pointer to. */
    HYP_NODISCARD HYP_FORCE_INLINE
    constexpr const CharType *operator*() const
        { return m_begin; }

    /*! \brief Inversion of the equality operator.
     *  \param other The other StringView object to compare against.
     *  \returns True if the strings are not equal, false otherwise. */
    HYP_NODISCARD HYP_FORCE_INLINE
    constexpr bool operator!=(const StringView &other) const
        { return !operator==(other); }

    /*! \brief Return the size of the string. For UTF-8 strings, this is the number of bytes.
     *  For other types, this is the number of characters.
     *  \note For UTF-8 strings, use the \ref{Length} function to get the number of characters.
     *  \returns The size of the string. */
    HYP_NODISCARD HYP_FORCE_INLINE
    constexpr SizeType Size() const
        { return SizeType(m_end - m_begin); }

    /*! \brief Return the length of the string. For UTF-8 strings, this is the number of characters.
     *  For other types, this is the same as the \ref{Size} function.
     *  \returns The length of the string in characters. */
    HYP_NODISCARD HYP_FORCE_INLINE
    constexpr SizeType Length() const
        { return m_length; }

    /*! \brief Return the raw string pointer.
     *  \returns The raw string pointer. */
    HYP_NODISCARD HYP_FORCE_INLINE
    constexpr const CharType *Data() const
        { return m_begin; }

    /*! \brief Get a char from the String at the given index.
     *  For UTF-8 strings, the character is encoded as a 32-bit value.
     *
     *  \ref{index} must be less than \ref{Length()}. */
    [[nodiscard]]
    WidestCharType GetChar(SizeType index) const
    {
        const SizeType size = Size();

#ifdef HYP_DEBUG_MODE
        AssertThrow(index < size);
#endif

        if constexpr (is_utf8) {
            return utf::utf8_charat(Data(), size, index);
        } else {
            return *(Data() + index);
        }
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr HashCode GetHashCode() const
        { return HashCode::GetHashCode(m_begin, m_end); }

    HYP_DEF_STL_BEGIN_END_CONSTEXPR(
        m_begin,
        m_end
    )

private:
    const CharType  *m_begin;
    const CharType  *m_end;
    SizeType        m_length;
};

template <int string_type>
constexpr bool operator<(const StringView<string_type> &lhs, const StringView<string_type> &rhs)
{
    if (!lhs.Begin()) {
        return true;
    }

    if (!rhs.Begin()) {
        return false;
    }

    return utf::utf_strcmp<typename StringView<string_type>::CharType, StringView<string_type>::is_utf8>(lhs.Begin(), rhs.Begin()) < 0;
}

template <int string_type>
constexpr bool operator<(const containers::detail::String<string_type> &lhs, const StringView<string_type> &rhs)
{
    if (!lhs.Begin()) {
        return true;
    }

    if (!rhs.Begin()) {
        return false;
    }

    return utf::utf_strcmp<typename StringView<string_type>::CharType, StringView<string_type>::is_utf8>(lhs.Begin(), rhs.Begin()) < 0;
}

template <int string_type>
constexpr bool operator<(const StringView<string_type> &lhs, const containers::detail::String<string_type> &rhs)
{
    if (!lhs.Begin()) {
        return true;
    }

    if (!rhs.Begin()) {
        return false;
    }

    return utf::utf_strcmp<typename StringView<string_type>::CharType, StringView<string_type>::is_utf8>(lhs.Begin(), rhs.Begin()) < 0;
}

template <int string_type>
constexpr bool operator==(const StringView<string_type> &lhs, const StringView<string_type> &rhs)
{
    if (lhs.Begin() == rhs.Begin() && (!lhs.Begin() || lhs.Size() == rhs.Size())) {
        return true;
    }

    return Memory::AreStaticStringsEqual(lhs.Begin(), rhs.Begin());
}

template <int string_type>
constexpr bool operator==(const containers::detail::String<string_type> &lhs, const StringView<string_type> &rhs)
{
    if (lhs.Begin() == rhs.Begin() && (!lhs.Begin() || lhs.Size() == rhs.Size())) {
        return true;
    }

    return Memory::AreStaticStringsEqual(lhs.Begin(), rhs.Begin());
}

template <int string_type>
constexpr bool operator==(const StringView<string_type> &lhs, const containers::detail::String<string_type> &rhs)
{
    if (lhs.Begin() == rhs.Begin() && (!lhs.Begin() || lhs.Size() == rhs.Size())) {
        return true;
    }

    return Memory::AreStaticStringsEqual(lhs.Begin(), rhs.Begin());
}

} // namespace detail

template <int string_type>
using StringView = detail::StringView< string_type >;

using ANSIStringView = StringView<StringType::ANSI>;
using UTF8StringView = StringView<StringType::UTF8>;
using UTF16StringView = StringView<StringType::UTF16>;
using UTF32StringView = StringView<StringType::UTF32>;
using WideStringView = StringView<StringType::WIDE_CHAR>;

} // namespace utilities

using utilities::StringView;
using utilities::ANSIStringView;
using utilities::UTF8StringView;
using utilities::UTF16StringView;
using utilities::UTF32StringView;
using utilities::WideStringView;

} // namespace hyperion

#endif