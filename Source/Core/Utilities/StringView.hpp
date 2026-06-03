/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Memory/Memory.hpp>

#include <Core/Utilities/Span.hpp>

#include <Core/Containers/StringFwd.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/HashCode.hpp>

#include <Core/Types.hpp>

#include <type_traits>

namespace Hyperion {
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

    static constexpr bool isContiguous = true;
    static constexpr size_t NotFound = size_t(-1);
    static constexpr int stringType = TStringType;

    static constexpr bool isAnsi = TStringType == StringType::ANSI;
    static constexpr bool isUtf8 = TStringType == StringType::UTF8;
    static constexpr bool isUtf16 = TStringType == StringType::UTF16;
    static constexpr bool isUtf32 = TStringType == StringType::UTF32;
    static constexpr bool isWide = TStringType == StringType::WIDE_CHAR;

    static_assert(!isUtf8 || (std::is_same_v<CharType, char> || std::is_same_v<CharType, unsigned char>), "UTF-8 Strings must have CharType equal to char or unsigned char");
    static_assert(!isAnsi || (std::is_same_v<CharType, char> || std::is_same_v<CharType, unsigned char>), "ANSI Strings must have CharType equal to char or unsigned char");
    static_assert(!isUtf16 || std::is_same_v<CharType, utf::Char16>, "UTF-16 Strings must have CharType equal to utf::Char16");
    static_assert(!isUtf32 || std::is_same_v<CharType, utf::Char32>, "UTF-32 Strings must have CharType equal to utf::Char32");
    static_assert(!isWide || std::is_same_v<CharType, wchar_t>, "Wide Strings must have CharType equal to wchar_t");

private:
    template <bool IsConst>
    struct IteratorBase
    {
        std::conditional_t<IsConst, const CharType*, CharType*> ptr;
        std::conditional_t<IsConst, const CharType*, CharType*> end;

        constexpr IteratorBase(std::conditional_t<IsConst, const CharType*, CharType*> ptr, std::conditional_t<IsConst, const CharType*, CharType*> end = nullptr)
            : ptr(ptr),
              end(end)
        {
        }

        HYP_FORCE_INLINE WidestCharType operator*() const
        {
            HYP_CORE_ASSERT(!end || ptr < end, "Dereferencing invalid iterator");

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
            if (end && ptr >= end)
            {
                return *this;
            }

            if constexpr (isUtf8)
            {
                size_t codepoints;
                utf::Char8to32(ptr, sizeof(utf::Char32), codepoints);

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
            if (end && ptr >= end)
            {
                return *this;
            }

            if constexpr (isUtf8)
            {
                size_t codepoints;
                utf::Char8to32(ptr, sizeof(utf::Char32), codepoints);

                return { ptr + codepoints, end };
            }
            else
            {
                return { ptr + 1, end };
            }
        }

        HYP_FORCE_INLINE IteratorBase operator+(size_t n) const
        {
            if constexpr (isUtf8)
            {
                auto it = *this;

                for (size_t i = 0; i < n; i++)
                {
                    if (end && it.ptr >= it.end)
                    {
                        break;
                    }

                    size_t codepoints;
                    utf::Char8to32(it.ptr, sizeof(utf::Char32), codepoints);

                    it.ptr += codepoints;
                }

                return it;
            }
            else
            {
                if (end && ptr >= end)
                {
                    return *this;
                }

                return { ptr + n, end };
            }
        }

        HYP_FORCE_INLINE IteratorBase& operator+=(size_t n)
        {
            if constexpr (isUtf8)
            {
                for (size_t i = 0; i < n; i++)
                {
                    if (end && ptr >= end)
                    {
                        break;
                    }

                    size_t codepoints;
                    utf::Char8to32(ptr, sizeof(utf::Char32), codepoints);

                    ptr += codepoints;
                }
            }
            else
            {
                if (end && ptr >= end)
                {
                    return *this;
                }

                ptr += n;
            }

            return *this;
        }

        HYP_FORCE_INLINE bool operator==(const IteratorBase& other) const
        {
            return ptr == other.ptr;
        }

        HYP_FORCE_INLINE bool operator!=(const IteratorBase& other) const
        {
            return ptr != other.ptr;
        }

        HYP_FORCE_INLINE bool operator<(const IteratorBase& other) const
        {
            return ptr < other.ptr;
        }

        HYP_FORCE_INLINE bool operator<=(const IteratorBase& other) const
        {
            return ptr <= other.ptr;
        }

        HYP_FORCE_INLINE bool operator>(const IteratorBase& other) const
        {
            return ptr > other.ptr;
        }

        HYP_FORCE_INLINE bool operator>=(const IteratorBase& other) const
        {
            return ptr >= other.ptr;
        }
    };

public:
    using Iterator = IteratorBase<true>;
    using ConstIterator = Iterator;

private:
    constexpr StringView(const CharType* _begin, const CharType* _end, uint32 length)
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

    template <uint32 Sz>
    constexpr StringView(const CharType (&str)[Sz])
        : m_begin(&str[0]),
          m_end(&str[0] + Sz),
          m_length(0)
    {
        size_t len = utf::StringLength<CharType, isUtf8>(str);
        
        if (len >= UINT32_MAX)
        {
            // Invalid UTF8 or bad length
            m_begin = nullptr;
            m_end = nullptr;
            m_length = 0;

            return;
        }

        m_length = uint32(len);
    }

    constexpr StringView(const CharType* str)
        : m_begin(nullptr),
          m_end(nullptr),
          m_length(0)
    {
        size_t codepoints;
        size_t len = utf::StringLength<CharType, isUtf8>(str, codepoints);
        
        if (len >= UINT32_MAX)
        {
            // Invalid UTF8 or bad length
            return;
        }

        m_begin = str;
        m_end = str + codepoints;
        m_length = uint32(len);
    }

    constexpr StringView(const CharType* _begin, const CharType* _end)
        : m_begin(_begin),
          m_end(_end),
          m_length(0)
    {
        if constexpr (isUtf8)
        {
            size_t len = utf::StringLength(_begin, _end);
            
            if (len >= UINT32_MAX)
            {
                // Invalid UTF8 or bad length
                m_begin = nullptr;
                m_end = nullptr;
                m_length = 0;

                return;
            }

            m_length = uint32(len);
        }
        else
        {
            size_t len = size_t(_end - _begin);
            
            if (len >= UINT32_MAX)
            {
                // Some corrupt pointer probably.
                m_begin = nullptr;
                m_end = nullptr;
                m_length = 0;

                return;
            }

            m_length = uint32(len);
        }
    }

    // template <size_t Sz, std::enable_if_t<std::is_same_v<CharType, typename StaticString<Sz>::CharType>, int> = 0>
    // constexpr StringView(const StaticString<Sz> &str)
    //     : m_begin(str.Begin()),
    //       m_end(str.Begin() + Sz),
    //       m_length(utf::StringLength< CharType, isUtf8 >(str.data))
    // {
    // }

    constexpr StringView(ConstByteView byteView)
        : m_begin(nullptr),
          m_end(nullptr),
          m_length(0)
    {
        size_t len = utf::StringLength<CharType, isUtf8>(reinterpret_cast<const CharType*>(byteView.Data()));

        if (len >= UINT32_MAX)
        {
            // Invalid UTF8 or bad length
            return;
        }

        m_begin = reinterpret_cast<const CharType*>(byteView.Data());
        m_end = reinterpret_cast<const CharType*>(byteView.Data() + byteView.Size());
        m_length = len;
    }

    constexpr StringView(const StringView& other) noexcept = default;
    constexpr StringView& operator=(const StringView& other) noexcept = default;

    constexpr StringView(StringView&& other) noexcept = default;
    constexpr StringView& operator=(StringView&& other) noexcept = default;

    constexpr ~StringView() = default;

    // HYP_FORCE_INLINE operator containers::String<TStringType>() const
    //     { return containers::String<TStringType>(Data()); }

    /*! \brief Check if the StringView is in a valid state.
     *  \returns True if the StringView is valid, false otherwise. */
    HYP_FORCE_INLINE constexpr explicit operator bool() const
    {
        return UIntPtr(m_end) > UIntPtr(m_begin);
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

    HYP_FORCE_INLINE constexpr bool operator==(const StringView& other) const
    {
        if (Data() == other.Data() && (!Data() || Size() == other.Size()))
        {
            return true;
        }

        return Memory::StrEqual(Data(), other.Data(), MathUtil::Min(Size(), other.Size()));
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
     *  \note For UTF-8 strings, use the \ref Length function to get the number of characters.
     *  \returns The size of the string. */
    HYP_FORCE_INLINE constexpr size_t Size() const
    {
        return m_end ? (size_t(m_end - m_begin)) : 0;
    }

    /*! \brief Return the length of the string. For UTF-8 strings, this is the number of characters.
     *  For other types, this is the same as the \ref Size function.
     *  \returns The length of the string in characters. */
    HYP_FORCE_INLINE constexpr size_t Length() const
    {
        return size_t(m_length);
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
     *  \p index must be less than \ref{Length()}.
     *
     *  \returns The character at the given index. If the index is out of bounds, it returns a null character. */
    WidestCharType GetChar(size_t index) const
    {
        const size_t size = Size();

        if constexpr (isUtf8)
        {
            WidestCharType ch = utf::CharAt(reinterpret_cast<const utf::Char8*>(Data()), size, index);
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
        return ch != CharType { 0 } && FindFirstIndex(ch) != NotFound;
    }

    /*! \brief Check if the string contains the given substring. */
    HYP_FORCE_INLINE constexpr bool Contains(const StringView& substr) const
    {
        return FindFirstIndex(substr) != NotFound;
    }

    /*! \brief Find the first occurrence of the character
     *  \param ch The character to search for.
     *  \returns The index of the first occurrence of the character or NotFound if it is not in the string. */
    HYP_FORCE_INLINE constexpr size_t FindFirstIndex(WidestCharType ch) const
    {
        if (ch == 0)
        {
            return NotFound;
        }

        size_t chars = 0;

        for (auto it = Begin(); it != End(); ++it, ++chars)
        {
            if (*it == ch)
            {
                return chars;
            }
        }

        return NotFound;
    }

    /*! \brief Find the last occurrence of the character
     *  \param ch The character to search for.
     *  \returns The index of the last occurrence of the character or NotFound if it is not in the string. */
    HYP_FORCE_INLINE constexpr size_t FindLastIndex(WidestCharType ch) const
    {
        if (ch == 0)
        {
            return NotFound;
        }

        size_t chars = 0;
        size_t lastIndex = NotFound;

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
    HYP_FORCE_INLINE constexpr size_t FindFirstIndex(const StringView& substr) const
    {
        const StringView str = StrStr(substr);

        if (str.Size() != 0)
        {
            if constexpr (isUtf8)
            {
                return utf::StringLength(m_begin, str.m_begin);
            }
            else
            {
                return size_t(str.m_begin - m_begin);
            }
        }

        return NotFound;
    }

    /*! \brief Find the last occurrence of the substring.
     *  \param substr The substring to search for.
     *  \returns The index of the last occurrence of the substring or NotFound if it is not in the string. */
    HYP_FORCE_INLINE constexpr size_t FindLastIndex(const StringView& substr) const
    {
        const size_t thisSize = Size();
        const size_t otherSize = substr.Size();

        if (thisSize < otherSize || otherSize == 0)
        {
            return NotFound;
        }

        size_t lastFound = NotFound;

        for (size_t offset = 0; offset <= thisSize - otherSize; ++offset)
        {
            size_t i = 0;
            for (; i < otherSize; i++)
            {
                if (m_begin[offset + i] != substr.m_begin[i])
                {
                    break;
                }
            }

            if (i == otherSize)
            {
                lastFound = offset;
            }
        }

        if (lastFound != NotFound && isUtf8)
        {
            lastFound = utf::StringLength(m_begin, m_begin + lastFound);
        }

        return lastFound;
    }

    constexpr StringView Substr(size_t first, size_t last) const
    {
        first = MathUtil::Min(first, m_length);
        last = MathUtil::Min(MathUtil::Max(last, first), m_length);

        size_t firstByteIndex = 0;
        size_t lastByteIndex = 0;
        uint32 newLength = 0;

        if constexpr (isUtf8)
        {
            size_t charIndex = 0;

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
        else if ((last - first) <= UINT32_MAX)
        {
            firstByteIndex = first;
            lastByteIndex = last;
            newLength = uint32(last - first);
        }

        return StringView(m_begin + firstByteIndex, m_begin + lastByteIndex, newLength);
    }

    constexpr StringView Substr(ConstIterator first, ConstIterator last) const
    {
        if (first.ptr < m_begin || last.ptr > m_end || first.ptr > last.ptr)
        {
            return StringView();
        }

        size_t newLength = 0;

        if constexpr (isUtf8)
        {
            newLength = utf::StringLength(first.ptr, last.ptr);
        }
        else
        {
            newLength = size_t(last.ptr - first.ptr);
        }

        if (newLength >= UINT32_MAX)
        {
            return {};
        }

        return StringView(first.ptr, last.ptr, uint32(newLength));
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode(::Hyperion::FNV1::DoHashString(m_begin, m_end));
    }

    HYP_NODISCARD constexpr Iterator Begin() const
    {
        return Iterator(m_begin, m_end);
    }

    HYP_NODISCARD constexpr Iterator End() const
    {
        return Iterator(m_end, m_end);
    }

    HYP_NODISCARD constexpr ConstIterator begin() const
    {
        return ConstIterator(m_begin, m_end);
    }

    HYP_NODISCARD constexpr ConstIterator end() const
    {
        return ConstIterator(m_end, m_end);
    }

protected:
    constexpr StringView StrStr(const StringView& other) const
    {
        const size_t thisSize = Size();
        const size_t otherSize = other.Size();

        if (thisSize < otherSize)
        {
            return StringView();
        }

        for (size_t offset = 0, otherOffset = 0, tempOffset = 0; offset < thisSize && m_begin[offset] != '\0'; offset++)
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

    uint32 m_length;
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

    return utf::StringCompare<typename StringView<TStringType>::CharType, StringView<TStringType>::isUtf8>(lhs.Data(), rhs.Data(), MathUtil::Min(lhs.Length(), rhs.Length())) < 0;
}

} // namespace utilities
} // namespace Hyperion
