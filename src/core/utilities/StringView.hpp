/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/Memory.hpp>

#include <core/utilities/Span.hpp>

#include <core/containers/StringFwd.hpp>

#include <core/math/MathUtil.hpp>

#include <core/HashCode.hpp>

#include <core/Types.hpp>

#include <type_traits>

namespace hyperion {
namespace utilities {

template <int TStringType>
class StringView
{
public:
    using CharType = typename containers::StringTypeImpl<TStringType>::CharType;
    using WidestCharType = typename containers::StringTypeImpl<TStringType>::WidestCharType;

    friend class containers::String<TStringType>;

    template <int FirstStringType, int SecondStringType>
    friend constexpr bool operator<(const StringView<FirstStringType>& lhs, const StringView<SecondStringType>& rhs);

    template <int FirstStringType, int SecondStringType>
    friend constexpr bool operator==(const StringView<FirstStringType>& lhs, const StringView<SecondStringType>& rhs);

    static constexpr bool isContiguous = true;
    static constexpr SizeType notFound = SizeType(-1);
    static constexpr int stringType = TStringType;

    static constexpr bool isAnsi = TStringType == StringType::ANSI;
    static constexpr bool isUtf8 = TStringType == StringType::UTF8;
    static constexpr bool isUtf16 = TStringType == StringType::UTF16;
    static constexpr bool isUtf32 = TStringType == StringType::UTF32;
    static constexpr bool isWide = TStringType == StringType::WIDE_CHAR;

    static_assert(!isUtf8 || (std::is_same_v<CharType, char> || std::is_same_v<CharType, unsigned char>), "UTF-8 Strings must have CharType equal to char or unsigned char");
    static_assert(!isAnsi || (std::is_same_v<CharType, char> || std::is_same_v<CharType, unsigned char>), "ANSI Strings must have CharType equal to char or unsigned char");
    static_assert(!isUtf16 || std::is_same_v<CharType, utf::u16char>, "UTF-16 Strings must have CharType equal to utf::u16char");
    static_assert(!isUtf32 || std::is_same_v<CharType, utf::u32char>, "UTF-32 Strings must have CharType equal to utf::u32char");
    static_assert(!isWide || std::is_same_v<CharType, wchar_t>, "Wide Strings must have CharType equal to wchar_t");

private:
    template <bool IsConst>
    struct IteratorBase
    {
        std::conditional_t<IsConst, const CharType*, CharType*> ptr;

        constexpr IteratorBase(std::conditional_t<IsConst, const CharType*, CharType*> ptr)
            : ptr(ptr)
        {
        }

        HYP_FORCE_INLINE WidestCharType operator*() const
        {
            if constexpr (isUtf8)
            {
                return utf::Char8to32(ptr);
            }
            else
            {
                return *ptr;
            }
        }

        HYP_FORCE_INLINE IteratorBase& operator++()
        {
            if constexpr (isUtf8)
            {
                SizeType codepoints;
                utf::Char8to32(ptr, sizeof(utf::u32char), codepoints);

                ptr += codepoints;
            }
            else
            {
                ++ptr;
            }

            return *this;
        }

        HYP_FORCE_INLINE IteratorBase operator++(int) const
        {
            if constexpr (isUtf8)
            {
                SizeType codepoints;
                utf::Char8to32(ptr, sizeof(utf::u32char), codepoints);

                return { ptr + codepoints };
            }
            else
            {
                return { ptr + 1 };
            }
        }

        HYP_FORCE_INLINE bool operator==(const IteratorBase& other) const
        {
            return ptr == other.ptr;
        }

        HYP_FORCE_INLINE bool operator!=(const IteratorBase& other) const
        {
            return ptr != other.ptr;
        }
    };

public:
    using Iterator = IteratorBase<true>;
    using ConstIterator = Iterator;

private:
    constexpr StringView(const CharType* _begin, const CharType* _end, SizeType length)
        : m_begin(_begin),
          m_end(_end),
          m_length(length)
    {
    }

public:
    constexpr StringView() noexcept
        : m_begin(nullptr),
          m_end(nullptr),
          m_length(0)
    {
    }

    // StringView(const String< TStringType > &str)
    //     : m_begin(str.Begin()),
    //       m_end(str.End() + 1 /* String class accounts for NUL char also */),
    //       m_length(str.Length())
    // {
    // }

    template <SizeType Sz>
    constexpr StringView(const CharType (&str)[Sz])
        : m_begin(&str[0]),
          m_end(&str[0] + Sz),
          m_length(utf::utfStrlen<CharType, isUtf8>(str))
    {
    }

    constexpr StringView(const CharType* str)
        : m_begin(str),
          m_end(nullptr),
          m_length(0)
    {
        SizeType codepoints = 0;
        m_length = utf::utfStrlen<CharType, isUtf8>(str, codepoints);
        m_end = m_begin + codepoints;
    }

    constexpr StringView(const CharType* _begin, const CharType* _end)
        : m_begin(_begin),
          m_end(_end),
          m_length(0)
    {
        if constexpr (isUtf8)
        {
            m_length = utf::utf8Strlen(_begin, _end);
        }
        else
        {
            m_length = SizeType(_end - _begin);
        }
    }

    // template <SizeType Sz, std::enable_if_t<std::is_same_v<CharType, typename StaticString<Sz>::CharType>, int> = 0>
    // constexpr StringView(const StaticString<Sz> &str)
    //     : m_begin(str.Begin()),
    //       m_end(str.Begin() + Sz),
    //       m_length(utf::utfStrlen< CharType, isUtf8 >(str.data))
    // {
    // }

    constexpr StringView(ConstByteView byteView)
        : m_begin(reinterpret_cast<const CharType*>(byteView.Data())),
          m_end(reinterpret_cast<const CharType*>(byteView.Data() + byteView.Size())),
          m_length(utf::utfStrlen<CharType, isUtf8>(reinterpret_cast<const CharType*>(byteView.Data())))
    {
    }

    constexpr StringView(const StringView& other) noexcept = default;
    constexpr StringView& operator=(const StringView& other) noexcept = default;
    constexpr StringView(StringView&& other) noexcept = default;
    constexpr StringView& operator=(StringView&& other) noexcept = default;

    // constexpr StringView(StringView &&other) noexcept
    //     : m_begin(other.m_begin),
    //       m_end(other.m_end),
    //       m_length(other.m_length)
    // {
    //     other.m_begin = nullptr;
    //     other.m_end = nullptr;
    //     other.m_length = 0;
    // }

    // StringView &operator=(StringView &&other) noexcept
    // {
    //     m_begin = other.m_begin;
    //     m_end = other.m_end;
    //     m_length = other.m_length;

    //     other.m_begin = nullptr;
    //     other.m_end = nullptr;
    //     other.m_length = 0;

    //     return *this;
    // }

    constexpr ~StringView() = default;

    // HYP_FORCE_INLINE operator containers::String<TStringType>() const
    //     { return containers::String<TStringType>(Data()); }

    /*! \brief Check if the StringView is in a valid state.
     *  \returns True if the StringView is valid, false otherwise. */
    HYP_FORCE_INLINE constexpr explicit operator bool() const
    {
        return uintptr_t(m_end) > uintptr_t(m_begin);
    }

    /*! \brief Conversion operator to return the charater pointer the StringView is pointing to.
     *  If the StringView is empty, this will return nullptr.
     *  \returns The beginning of the string. */
    HYP_FORCE_INLINE constexpr explicit operator const CharType*() const
    {
        return m_begin;
    }

    /*! \brief Convenience operator overload to return the raw string using the dereference operator
     *  \returns The raw string that the StringView has a pointer to. */
    HYP_FORCE_INLINE constexpr const CharType* operator*() const
    {
        return m_begin;
    }

    /*! \brief Inversion of the equality operator.
     *  \param other The other StringView object to compare against.
     *  \returns True if the strings are not equal, false otherwise. */
    HYP_FORCE_INLINE constexpr bool operator!=(const StringView& other) const
    {
        return !(*this == other);
    }

    /*! \brief Return the size of the string. For UTF-8 strings, this is the number of bytes.
     *  For other types, this is the number of characters.
     *  \note For UTF-8 strings, use the \ref{Length} function to get the number of characters.
     *  \returns The size of the string. */
    HYP_FORCE_INLINE constexpr SizeType Size() const
    {
        return m_end ? (SizeType(m_end - m_begin)) : 0;
    }

    /*! \brief Return the length of the string. For UTF-8 strings, this is the number of characters.
     *  For other types, this is the same as the \ref{Size} function.
     *  \returns The length of the string in characters. */
    HYP_FORCE_INLINE constexpr SizeType Length() const
    {
        return m_length;
    }

    /*! \brief Return the raw string pointer.
     *  \returns The raw string pointer. If the string is empty, this will return nullptr. */
    HYP_FORCE_INLINE constexpr const CharType* Data() const
    {
        return m_begin;
    }

    /*! \brief Get a char from the String at the given index.
     *  For UTF-8 strings, the character is encoded as a 32-bit value.
     *
     *  \ref{index} must be less than \ref{Length()}.
     *
     *  \returns The character at the given index. If the index is out of bounds, it returns a null character. */
    WidestCharType GetChar(SizeType index) const
    {
        const SizeType size = Size();

        if constexpr (isUtf8)
        {
            WidestCharType ch = utf::utf8Charat(reinterpret_cast<const utf::u8char*>(Data()), size, index);
            if (ch == WidestCharType(-1))
            {
                // invalid utf8
                return 0;
            }

            return ch;
        }
        else
        {
            return index >= size ? *(Data() + index) : 0;
        }
    }

    /*! \brief Check if the string contains the given character. */
    HYP_FORCE_INLINE constexpr bool Contains(WidestCharType ch) const
    {
        return ch != CharType { 0 } && FindFirstIndex(ch) != notFound;
    }

    /*! \brief Check if the string contains the given substring. */
    HYP_FORCE_INLINE constexpr bool Contains(const StringView& substr) const
    {
        return FindFirstIndex(substr) != notFound;
    }

    /*! \brief Find the first occurrence of the character
     *  \param ch The character to search for.
     *  \returns The index of the first occurrence of the character or notFound if it is not in the string. */
    HYP_FORCE_INLINE constexpr SizeType FindFirstIndex(WidestCharType ch) const
    {
        if (ch == 0)
        {
            return notFound;
        }

        SizeType chars = 0;

        for (auto it = Begin(); it != End(); ++it, ++chars)
        {
            if (*it == ch)
            {
                return chars;
            }
        }

        return notFound;
    }

    /*! \brief Find the last occurrence of the character
     *  \param ch The character to search for.
     *  \returns The index of the last occurrence of the character or notFound if it is not in the string. */
    HYP_FORCE_INLINE constexpr SizeType FindLastIndex(WidestCharType ch) const
    {
        if (ch == 0)
        {
            return notFound;
        }

        SizeType chars = 0;
        SizeType lastIndex = notFound;

        for (auto it = Begin(); it != End(); ++it, ++chars)
        {
            if (*it == ch)
            {
                lastIndex = chars;
            }
        }

        return lastIndex;
    }

    /*! \brief Find the first occurrence of the substring.
     *  \param substr The substring to search for.
     *  \returns The index of the first occurrence of the substring. */
    HYP_FORCE_INLINE constexpr SizeType FindFirstIndex(const StringView& substr) const
    {
        const StringView str = StrStr(substr);

        if (str.Size() != 0)
        {
            if constexpr (isUtf8)
            {
                return utf::utf8Strlen(m_begin, str.m_begin);
            }
            else
            {
                return SizeType(str.m_begin - m_begin);
            }
        }

        return notFound;
    }

    constexpr StringView Substr(SizeType first, SizeType last) const
    {
        first = MathUtil::Min(first, m_length);
        last = MathUtil::Min(MathUtil::Max(last, first), m_length);

        SizeType firstByteIndex = 0;
        SizeType lastByteIndex = 0;
        SizeType newLength = 0;

        if constexpr (isUtf8)
        {
            SizeType charIndex = 0;

            while (charIndex < first)
            {
                const CharType c = m_begin[firstByteIndex];

                if (c >= 0 && c <= 127)
                {
                    firstByteIndex += 1;
                }
                else if ((c & 0xE0) == 0xC0)
                {
                    firstByteIndex += 2;
                }
                else if ((c & 0xF0) == 0xE0)
                {
                    firstByteIndex += 3;
                }
                else if ((c & 0xF8) == 0xF0)
                {
                    firstByteIndex += 4;
                }

                ++charIndex;
            }

            while (charIndex < last)
            {
                const CharType c = m_begin[firstByteIndex + lastByteIndex];

                if (c >= 0 && c <= 127)
                {
                    lastByteIndex += 1;
                }
                else if ((c & 0xE0) == 0xC0)
                {
                    lastByteIndex += 2;
                }
                else if ((c & 0xF0) == 0xE0)
                {
                    lastByteIndex += 3;
                }
                else if ((c & 0xF8) == 0xF0)
                {
                    lastByteIndex += 4;
                }

                ++newLength;
                ++charIndex;
            }

            lastByteIndex += firstByteIndex;
        }
        else
        {
            firstByteIndex = first;
            lastByteIndex = last;
            newLength = last - first;
        }

        return StringView(m_begin + firstByteIndex, m_begin + lastByteIndex, newLength);
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode(::hyperion::FNV1::DoHashString(m_begin, m_end));
    }

    HYP_DEF_STL_BEGIN_END(m_begin, m_end)

protected:
    constexpr StringView StrStr(const StringView& other) const
    {
        const SizeType thisSize = Size();
        const SizeType otherSize = other.Size();

        if (thisSize < otherSize)
        {
            return StringView();
        }

        for (SizeType offset = 0, otherOffset = 0, tempOffset = 0; offset < thisSize && m_begin[offset] != '\0'; offset++)
        {
            if (m_begin[offset] != other.m_begin[otherOffset])
            {
                continue;
            }

            tempOffset = offset;

            for (;;)
            {
                if (otherOffset >= otherSize || other.m_begin[otherOffset] == '\0')
                {
                    return { m_begin + offset, m_end };
                }

                if (tempOffset >= thisSize || m_begin[tempOffset] == '\0')
                {
                    break;
                }

                if (m_begin[tempOffset++] != other.m_begin[otherOffset++])
                {
                    break;
                }
            }

            otherOffset = 0;
        }

        return StringView();
    }

private:
    const CharType* m_begin;
    const CharType* m_end;
    SizeType m_length;
};

template <int TStringType>
constexpr bool operator<(const StringView<TStringType>& lhs, const StringView<TStringType>& rhs)
{
    if (!lhs.Data())
    {
        return true;
    }

    if (!rhs.Data())
    {
        return false;
    }

    return utf::utfStrncmp<typename StringView<TStringType>::CharType, StringView<TStringType>::isUtf8>(lhs.Data(), rhs.Data(), MathUtil::Min(lhs.Length(), rhs.Length())) < 0;
}

template <int TStringType>
constexpr bool operator==(const StringView<TStringType>& lhs, const StringView<TStringType>& rhs)
{
    if (lhs.Data() == rhs.Data() && (!lhs.Data() || lhs.Size() == rhs.Size()))
    {
        return true;
    }

    return Memory::AreStaticStringsEqual(lhs.Data(), rhs.Data(), MathUtil::Min(lhs.Size(), rhs.Size()));
}

} // namespace utilities
} // namespace hyperion
