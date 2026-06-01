/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/MathUtil.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FixedArray.hpp>
#include <Core/Containers/StringFwd.hpp>

#include <Core/Utilities/Span.hpp>
#include <Core/Utilities/StringView.hpp>

#include <Core/Functional/FunctionWrapper.hpp>

#include <Core/Memory/Memory.hpp>

#include <Core/Defines.hpp>
#include <Core/Utilities/Traits.hpp>

#include <Core/Types.hpp>
#include <Core/Constants.hpp>
#include <Core/HashCode.hpp>

#include <type_traits>

namespace Hyperion {

HYP_MAKE_HAS_METHOD(ToString);

namespace utilities {

template <int TStringType>
class StringView;

} // namespace utilities

namespace containers {

namespace detail {

template <class CharType, size_t Size, class = std::enable_if_t<IsValidCharType<CharType>>>
static inline constexpr CharType* ConvertChars(const char* src, CharType (&dst)[Size])
{
    if (Size == 0)
    {
        return dst;
    }

    for (size_t i = 0; i < Size - 1; ++i)
    {
        dst[i] = static_cast<CharType>(src[i]);
    }

    dst[Size - 1] = CharType('\0');

    return dst;
}

} // namespace detail

/*! \brief Dynamic string class that natively supports UTF-8, as well as UTF-16, UTF-32, wide chars and ANSI. */
template <int TStringType, class TAllocator>
class String : Array<typename StringTypeImpl<TStringType>::CharType, TAllocator>
{
public:
    using CharType = typename StringTypeImpl<TStringType>::CharType;
    using WidestCharType = typename StringTypeImpl<TStringType>::WidestCharType;

protected:
    using Base = Array<CharType, TAllocator>;

public:
    using ValueType = typename Base::ValueType;
    using KeyType = typename Base::KeyType;

    using Iterator = typename utilities::StringView<TStringType>::Iterator;
    using ConstIterator = typename utilities::StringView<TStringType>::ConstIterator;

    static constexpr bool isContiguous = true;

    static const String empty;

    static constexpr bool isAnsi = TStringType == ANSI;
    static constexpr bool isUtf8 = TStringType == UTF8;
    static constexpr bool isUtf16 = TStringType == UTF16;
    static constexpr bool isUtf32 = TStringType == UTF32;
    static constexpr bool isWide = TStringType == WIDE_CHAR;

    static_assert(!isAnsi || std::is_same_v<CharType, char>, "ANSI Strings must have CharType equal to char");
    static_assert(!isUtf8 || std::is_same_v<CharType, utf::Char8>, "UTF8 Strings must have CharType equal to utf::Char8");
    static_assert(!isUtf16 || std::is_same_v<CharType, utf::Char16>, "UTF-16 Strings must have CharType equal to utf::Char16");
    static_assert(!isUtf32 || std::is_same_v<CharType, utf::Char32>, "UTF-32 Strings must have CharType equal to utf::Char32");
    static_assert(!isWide || std::is_same_v<CharType, wchar_t>, "Wide Strings must have CharType equal to wchar_t");

    static constexpr int stringType = TStringType;

    static constexpr size_t NotFound = size_t(-1);

    String();
    String(const String& other);

    template <int TOtherStringType, class TOtherAllocator, class = std::enable_if_t<TOtherStringType != TStringType>>
    String(const String<TOtherStringType, TOtherAllocator>& other)
        : String()
    {
        *this = other;
    }

    String(const utilities::StringView<TStringType>& stringView)
        : Base(),
          m_length(stringView.Length())
    {
        if (m_length == 0)
        {
            Base::ResizeZeroed(1);

            return;
        }

        Base::ResizeUninitialized(stringView.Size() + 1);
        Memory::Copy(Base::Data(), stringView.Data(), stringView.Size() * sizeof(CharType));
        Base::Data()[Base::Size() - 1] = CharType('\0'); // Null-terminate the string
    }

    String(const CharType* str);
    String(const CharType* _begin, const CharType* _end);

    template <class OtherCharType, class = std::enable_if_t<IsValidCharType<OtherCharType> && !std::is_same_v<OtherCharType, CharType>>>
    String(const OtherCharType* str)
        : String()
    {
        Clear();
        Append(str);
    }

    explicit String(ConstByteView byteView);

    template <int TOtherStringType, class = std::enable_if_t<TOtherStringType != TStringType>>
    String(const utilities::StringView<TOtherStringType>& stringView)
        : String()
    {
        *this = stringView;
    }

    String(String&& other) noexcept;
    ~String();

    String& operator=(const String& other);
    String& operator=(String&& other) noexcept;

    template <int TOtherStringType, class = std::enable_if_t<TOtherStringType != TStringType>>
    HYP_FORCE_INLINE String& operator=(const utilities::StringView<TOtherStringType>& stringView)
    {
        Clear();
        Append(stringView.Data(), stringView.Data() + stringView.Size());

        return *this;
    }

    template <int TOtherStringType, class TOtherAllocator, class = std::enable_if_t<TOtherStringType != TStringType>>
    HYP_FORCE_INLINE String& operator=(const String<TOtherStringType, TOtherAllocator>& other)
    {
        Clear();
        Append(other.Data(), other.Data() + other.Size());

        return *this;
    }

    String& operator=(const CharType* str);

    HYP_NODISCARD HYP_FORCE_INLINE String operator+(const CharType* str) const
    {
        return String(*this) += str;
    }

    HYP_FORCE_INLINE String& operator+=(const CharType* str)
    {
        Append(str);
        return *this;
    }

    template <class OtherCharType, class = std::enable_if_t<IsValidCharType<OtherCharType> && !std::is_same_v<OtherCharType, CharType>>>
    HYP_NODISCARD HYP_FORCE_INLINE String operator+(const OtherCharType* str) const
    {
        return String(*this) += str;
    }

    template <class OtherCharType, class = std::enable_if_t<IsValidCharType<OtherCharType> && !std::is_same_v<OtherCharType, CharType>>>
    HYP_FORCE_INLINE String& operator+=(const OtherCharType* str)
    {
        Append(str);
        return *this;
    }

    template <int TOtherStringType, class TOtherAllocator, class = std::enable_if_t<TOtherStringType != TStringType>>
    HYP_NODISCARD HYP_FORCE_INLINE String operator+(const String<TOtherStringType, TOtherAllocator>& other) const
    {
        String result(*this);
        result.Append(other.Data(), other.Data() + other.Size());

        return result;
    }

    template <int TOtherStringType, class TOtherAllocator, class = std::enable_if_t<TOtherStringType != TStringType>>
    HYP_FORCE_INLINE String& operator+=(const String<TOtherStringType, TOtherAllocator>& other)
    {
        Append(other.Data(), other.Data() + other.Size());

        return *this;
    }

    HYP_NODISCARD String operator+(const utilities::StringView<TStringType>& stringView) const;
    String& operator+=(const utilities::StringView<TStringType>& stringView);

    HYP_NODISCARD String operator+(CharType ch) const;
    String& operator+=(CharType ch);

    template <class OtherCharType, class = std::enable_if_t<IsValidCharType<OtherCharType> && !std::is_same_v<OtherCharType, CharType>>>
    HYP_NODISCARD HYP_FORCE_INLINE String operator+(OtherCharType ch) const
    {
        return String(*this) += ch;
    }

    template <class OtherCharType, class = std::enable_if_t<IsValidCharType<OtherCharType> && !std::is_same_v<OtherCharType, CharType>>>
    HYP_FORCE_INLINE String& operator+=(CharType ch)
    {
        Append(ch);
        return *this;
    }

    bool operator==(const String& other) const;
    bool operator!=(const String& other) const;

    bool operator==(const CharType* str) const;
    bool operator!=(const CharType* str) const;

    bool operator<(const String& other) const;

    /*! \brief Raw access of the character data of the string at index.
     * \note For UTF-8 Strings, the returned value may not be a valid UTF-8 character,
     *  so in most cases, you'll want to use GetChar() instead.
     *  Returns a reference so &str[0] can be used for backwards compatibility
     *
     * \p index must be less than \ref Size().
     */
    const CharType& operator[](size_t index) const;

    /*! \brief Get a char from the String at the given index.
     * For UTF-8 strings, the character is encoded as a 32-bit value.
     * \note If needing to access raw character data, \ref operator[] should be used instead.
     *
     * \p index must be less than \ref Length().
     */
    WidestCharType GetChar(size_t index) const;

    /*! \brief Return the data size in characters. Note, UTF-8 strings can have a shorter length than size. */
    HYP_FORCE_INLINE size_t Size() const
    {
        return Base::Size() - 1; /* for NT char */
    }

    /*! \brief Return the length of the string in characters. Note, UTF-8 strings can have a shorter length than size. */
    HYP_FORCE_INLINE size_t Length() const
    {
        return size_t(m_length);
    }

    /*! \brief Access the raw data of the string.
        \note For UTF-8 strings, ensure proper care is taken when accessing the data, as indexing via a character index may not
              yield a valid character. */
    HYP_FORCE_INLINE typename Base::ValueType* Data()
    {
        return Base::Data();
    }

    /*! \brief Access the raw data of the string.
        \note For UTF-8 strings, ensure proper care is taken when accessing the data, as indexing via a character index may not
              yield a valid character. */
    HYP_FORCE_INLINE const typename Base::ValueType* Data() const
    {
        return Base::Data();
    }

    HYP_FORCE_INLINE operator utilities::StringView<TStringType>() const& // <-- to prevent binding to temporary
    {
        if (Base::Begin() == Base::End())
        {
            return utilities::StringView<TStringType>();
        }

        return utilities::StringView<TStringType>(Base::Begin(), Base::End() - 1, Length());
    }

    /*! \brief Conversion operator to return the raw data of the string. */
    HYP_FORCE_INLINE explicit operator const CharType*() const
    {
        return Base::Data();
    }

    /*! \brief Dereference operator overload to return the raw data of the string. */
    HYP_FORCE_INLINE const CharType* operator*() const
    {
        return Base::Data();
    }

    HYP_FORCE_INLINE typename Base::ValueType& Front()
    {
        return Base::Front();
    }

    HYP_FORCE_INLINE const typename Base::ValueType& Front() const
    {
        return Base::Front();
    }

    HYP_FORCE_INLINE typename Base::ValueType& Back()
    {
        return Base::Data()[Base::Size() - 2]; /* for NT char */
    }

    HYP_FORCE_INLINE const typename Base::ValueType& Back() const
    {
        return Base::Data()[Base::Size() - 2]; /* for NT char */
    }

    /*! \brief Check if the string contains the given character. */
    HYP_FORCE_INLINE bool Contains(WidestCharType ch) const
    {
        return ch != CharType { 0 } && utilities::StringView<TStringType>(*this).FindFirstIndex(ch) != NotFound;
    }

    /*! \brief Check if the string contains the given string. */
    HYP_FORCE_INLINE bool Contains(const utilities::StringView<TStringType>& substr) const
    {
        return FindFirstIndex(substr) != NotFound;
    }

    /*! \brief Find the index of the first occurrence of the character. */
    HYP_FORCE_INLINE size_t FindFirstIndex(WidestCharType ch) const
    {
        return utilities::StringView<TStringType>(*this).FindFirstIndex(ch);
    }

    /*! \brief Find the index of the first occurrence of the substring
     * \note For UTF-8 strings, ensure accessing the character with the returned value is done via the \ref GetChar method,
     *       as the index is the character index, not the byte index. */
    HYP_FORCE_INLINE size_t FindFirstIndex(const utilities::StringView<TStringType>& substr) const
    {
        return utilities::StringView<TStringType>(*this).FindFirstIndex(substr);
    }

    /*! \brief Find the index of the last occurrence of the character. */
    HYP_FORCE_INLINE size_t FindLastIndex(WidestCharType ch) const
    {
        return utilities::StringView<TStringType>(*this).FindLastIndex(ch);
    }

    /*! \brief Find the index of the last occurrence of the substring
     * \note For UTF-8 strings, ensure accessing the character with the returned value is done via the \ref GetChar method,
     *       as the index is the character index, not the byte index. */
    HYP_FORCE_INLINE size_t FindLastIndex(const utilities::StringView<TStringType>& substr) const
    {
        return utilities::StringView<TStringType>(*this).FindLastIndex(substr);
    }

    /*! \brief Check if the string is empty. */
    HYP_FORCE_INLINE bool Empty() const
    {
        return Size() == 0;
    }

    /*! \brief Check if the string contains any characters. */
    HYP_FORCE_INLINE bool Any() const
    {
        return Size() != 0;
    }

    /*! \brief Check if the string contains multi-byte characters. */
    HYP_FORCE_INLINE bool HasMultiByteChars() const
    {
        return Size() > Length();
    }

    /*! \brief Reserve space for the string. \p capacity + 1 is used, to make space for the null character. */
    HYP_FORCE_INLINE void Reserve(size_t capacity)
    {
        Base::Reserve(capacity + 1);
    }

    HYP_FORCE_INLINE void Refit()
    {
        Base::Refit();
    }

    void Append(const utilities::StringView<TStringType>& stringView);
    void Append(CharType value);

    void Append(const CharType* str);
    void Append(const CharType* _begin, const CharType* _end);

    template <class OtherCharType, class = std::enable_if_t<IsValidCharType<OtherCharType> && !std::is_same_v<OtherCharType, CharType>>>
    void Append(const OtherCharType* str)
    {
        const size_t size = utf::StringLength<OtherCharType, false>(str);

        Append(str, str + size);
    }

    template <class OtherCharType, class = std::enable_if_t<IsValidCharType<OtherCharType> && !std::is_same_v<OtherCharType, CharType>>>
    void Append(const OtherCharType* _begin, const OtherCharType* _end)
    {
        if constexpr (isUtf8)
        {
            if constexpr (std::is_same_v<OtherCharType, Char32>)
            {
                const size_t len = utf::ToUtf8(_begin, _end, nullptr);

                if (len == 0)
                {
                    return;
                }

                Array<utf::Char8> buffer;
                buffer.Resize(len + 1);
                utf::ToUtf8(_begin, _end, buffer.Data());

                for (size_t i = 0; i < buffer.Size(); i++)
                {
                    Append(CharType(buffer[i]));
                }
            }
            else if constexpr (std::is_same_v<OtherCharType, Char16>)
            {
                const size_t len = utf::ToUtf8(_begin, _end, nullptr);

                if (len == 0)
                {
                    return;
                }

                Array<utf::Char8> buffer;
                buffer.Resize(len + 1);
                utf::ToUtf8(_begin, _end, buffer.Data());

                for (size_t i = 0; i < buffer.Size(); i++)
                {
                    Append(CharType(buffer[i]));
                }
            }
            else if constexpr (std::is_same_v<OtherCharType, wchar_t>)
            {
                const size_t len = utf::ToUtf8(_begin, _end, nullptr);

                if (len == 0)
                {
                    return;
                }

                Array<utf::Char8> buffer;
                buffer.Resize(len + 1);
                utf::ToUtf8(_begin, _end, buffer.Data());

                for (size_t i = 0; i < buffer.Size(); i++)
                {
                    Append(CharType(buffer[i]));
                }
            }
            else
            {
                static_assert(always_fail_v<OtherCharType>, "Invalid character type to append to UTF-8 encoded string");
            }
        }
        else if constexpr (isWide)
        {
            if constexpr (std::is_same_v<OtherCharType, utf::Char8>)
            {
                const size_t len = utf::ToWide(reinterpret_cast<const utf::Char8*>(_begin), reinterpret_cast<const utf::Char8*>(_end), nullptr);

                if (len == 0)
                {
                    return;
                }

                Array<wchar_t> buffer;
                buffer.Resize(len + 1);
                utf::ToWide(reinterpret_cast<const utf::Char8*>(_begin), reinterpret_cast<const utf::Char8*>(_end), buffer.Data());

                for (size_t i = 0; i < len; i++)
                {
                    Append(static_cast<CharType>(buffer[i]));
                }
            }
            else if constexpr (std::is_same_v<OtherCharType, utf::Char16>)
            {
                // Convert UTF-16 to Wide using Char16to32
                const utf::Char16* ptr = _begin;
                const utf::Char16* endPtr = _end;

                while (ptr < endPtr)
                {
                    size_t codeUnits = 0;
                    const utf::Char32 ch = utf::Char16to32(ptr, size_t(endPtr - ptr), codeUnits);

                    if (ch == utf::Char32(-1) || codeUnits == 0)
                    {
                        break;
                    }

                    ptr += codeUnits;
                    Append(static_cast<CharType>(ch));
                }
            }
            else if constexpr (std::is_same_v<OtherCharType, utf::Char32>)
            {
                // Convert UTF-32 to Wide
                for (const utf::Char32* ptr = _begin; ptr < _end; ++ptr)
                {
                    Append(static_cast<CharType>(*ptr));
                }
            }
            else
            {
                static_assert(always_fail_v<OtherCharType>, "Invalid character type to append to Wide string");
            }
        }
        else if constexpr (isUtf32)
        {
            if constexpr (std::is_same_v<OtherCharType, utf::Char8>)
            {
                const utf::Char8* ptr = reinterpret_cast<const utf::Char8*>(_begin);
                const utf::Char8* endPtr = reinterpret_cast<const utf::Char8*>(_end);

                while (ptr < endPtr)
                {
                    size_t codepoints = 0;
                    const utf::Char32 ch = utf::Char8to32(reinterpret_cast<const char*>(ptr), size_t(endPtr - ptr), codepoints);

                    if (ch == utf::Char32(-1) || codepoints == 0)
                    {
                        break;
                    }

                    ptr += codepoints;
                    Append(static_cast<CharType>(ch));
                }
            }
            else if constexpr (std::is_same_v<OtherCharType, utf::Char16>)
            {
                const utf::Char16* ptr = _begin;
                const utf::Char16* endPtr = _end;

                while (ptr < endPtr)
                {
                    size_t codeUnits = 0;
                    const utf::Char32 ch = utf::Char16to32(ptr, size_t(endPtr - ptr), codeUnits);

                    if (ch == utf::Char32(-1) || codeUnits == 0)
                    {
                        break;
                    }

                    ptr += codeUnits;
                    Append(static_cast<CharType>(ch));
                }
            }
            else if constexpr (std::is_same_v<OtherCharType, wchar_t>)
            {
                // For Win32 (UTF-16) this must combine surrogate pairs

                const wchar_t* ptr = _begin;
                const wchar_t* endPtr = _end;

                while (ptr < endPtr)
                {
                    size_t codeUnits = 0;
                    const utf::Char32 ch = utf::WideTo32(ptr, size_t(endPtr - ptr), codeUnits);

                    if (ch == utf::Char32(-1) || codeUnits == 0)
                    {
                        break;
                    }

                    ptr += codeUnits;
                    Append(static_cast<CharType>(ch));
                }
            }
            else
            {
                static_assert(always_fail_v<OtherCharType>, "Invalid character type to append to UTF-32 encoded string");
            }
        }
        else if constexpr (isUtf16)
        {
            if constexpr (std::is_same_v<OtherCharType, utf::Char8>)
            {
                const utf::Char8* ptr = reinterpret_cast<const utf::Char8*>(_begin);
                const utf::Char8* endPtr = reinterpret_cast<const utf::Char8*>(_end);

                while (ptr < endPtr)
                {
                    size_t codepoints = 0;
                    const utf::Char32 ch = utf::Char8to32(reinterpret_cast<const char*>(ptr), size_t(endPtr - ptr), codepoints);

                    if (ch == utf::Char32(-1) || codepoints == 0)
                    {
                        break;
                    }

                    ptr += codepoints;

                    if (ch <= 0xFFFF)
                    {
                        // single UTF-16 code unit
                        Append(static_cast<CharType>(ch));
                    }
                    else if (ch <= 0x10FFFF)
                    {
                        // surrogate pair
                        const utf::Char32 adjusted = ch - 0x10000;
                        Append(static_cast<CharType>((adjusted >> 10) + 0xD800));   // High surrogate
                        Append(static_cast<CharType>((adjusted & 0x3FF) + 0xDC00)); // Low surrogate
                    }
                }
            }
            else if constexpr (std::is_same_v<OtherCharType, utf::Char32>)
            {
                for (const utf::Char32* ptr = _begin; ptr < _end; ++ptr)
                {
                    const utf::Char32 ch = *ptr;

                    if (ch <= 0xFFFF)
                    {
                        // BMP char
                        Append(static_cast<CharType>(ch));
                    }
                    else if (ch <= 0x10FFFF)
                    {
                        // surrogate pair
                        const utf::Char32 adjusted = ch - 0x10000;
                        Append(static_cast<CharType>((adjusted >> 10) + 0xD800));   // High surrogate
                        Append(static_cast<CharType>((adjusted & 0x3FF) + 0xDC00)); // Low surrogate
                    }
                }
            }
            else if constexpr (std::is_same_v<OtherCharType, wchar_t>)
            {
                // Convert wchar_t to UTF-16
#ifdef HYP_WINDOWS
                static_assert(sizeof(OtherCharType) == sizeof(CharType));

                // Win32: Can just treat as UTF-16
                for (const wchar_t* ptr = _begin; ptr < _end; ++ptr)
                {
                    Append(static_cast<CharType>(*ptr));
                }
#else
                static_assert(sizeof(OtherCharType) == sizeof(utf::Char32));

                // Treat as UTF-32 on other (Unix) platforms
                for (const wchar_t* ptr = _begin; ptr < _end; ++ptr)
                {
                    const wchar_t ch = *ptr;

                    if (ch <= 0xFFFF)
                    {
                        // BMP char
                        Append(static_cast<CharType>(ch));
                    }
                    else if (ch <= 0x10FFFF)
                    {
                        // surrogate pair
                        const utf::Char32 adjusted = ch - 0x10000;
                        Append(static_cast<CharType>((adjusted >> 10) + 0xD800));   // High surrogate
                        Append(static_cast<CharType>((adjusted & 0x3FF) + 0xDC00)); // Low surrogate
                    }
                }
#endif
            }
            else
            {
                static_assert(always_fail_v<OtherCharType>, "Invalid character type to append to UTF-16 encoded string");
            }
        }
        else
        {
            static_assert(always_fail_v<CharType>, "Invalid string type for append");
        }
    }

    /// append single character
    template <class OtherCharType, class = std::enable_if_t<IsValidCharType<OtherCharType> && !std::is_same_v<OtherCharType, CharType>>>
    HYP_FORCE_INLINE void Append(OtherCharType ch)
    {
        size_t codepoints = 0;
        utf::Char8 buffer[sizeof(utf::Char32) + 1] = { '\0' };
        utf::Char32to8(static_cast<utf::Char32>(ch), &buffer[0], codepoints);

        for (size_t i = 0; i < codepoints; i++)
        {
            Append(CharType(buffer[i]));
        }
    }

    typename Base::ValueType PopBack();
    typename Base::ValueType PopFront();
    void Clear();

    bool StartsWith(const String& other) const;
    bool EndsWith(const String& other) const;

    HYP_NODISCARD String ToLower() const;
    HYP_NODISCARD String ToUpper() const;

    HYP_NODISCARD String Trimmed() const;
    HYP_NODISCARD String TrimmedLeft() const;
    HYP_NODISCARD String TrimmedRight() const;

    HYP_FORCE_INLINE utilities::StringView<TStringType> Substr(size_t first, size_t last = MathUtil::MaxSafeValue<size_t>()) const
    {
        return utilities::StringView<TStringType>(*this).Substr(first, last);
    }

    // HYP_NODISCARD String Substr(size_t first, size_t last = MathUtil::MaxSafeValue<size_t>()) const;

    HYP_NODISCARD String Escape() const;
    HYP_NODISCARD String ReplaceAll(const String& search, const String& replace) const;

    template <class... SeparatorType>
    HYP_NODISCARD auto Split(SeparatorType... separators) const
    {
        Hyperion::FixedArray<WidestCharType, sizeof...(separators)> separatorValues { WidestCharType(separators)... };

        const CharType* data = Base::Data();
        const size_t size = Size();

        Array<String> tokens;

        String workingString;
        workingString.Reserve(size);

        if constexpr (isAnsi)
        {
            for (size_t i = 0; i < size; i++)
            {
                const CharType ch = data[i];

                if (separatorValues.Contains(ch))
                {
                    tokens.PushBack(std::move(workingString));
                    // workingString now cleared
                    continue;
                }

                workingString.Append(ch);
            }
        }
        else
        {
            for (size_t i = 0; i < size;)
            {
                size_t codepoints = 0;
                utf::Char32 char32 = 0;

                if constexpr (isUtf8)
                {
                    char32 = utf::Char8to32(
                        data + i,
                        MathUtil::Min(sizeof(utf::Char32), size - i),
                        codepoints);
                }
                else if constexpr (isAnsi)
                {
                    char32 = static_cast<utf::Char32>(data[i]);
                    codepoints = 1;
                }
                else if constexpr (isUtf32)
                {
                    char32 = reinterpret_cast<const utf::Char32*>(data + i)[0];
                    codepoints = 1;
                }
                else if constexpr (isUtf16)
                {
                    char32 = utf::Char16to32(
                        reinterpret_cast<const utf::Char16*>(data + i),
                        MathUtil::Min(sizeof(utf::Char32) / sizeof(utf::Char16), size - i),
                        codepoints);
                }
                else if constexpr (isWide)
                {
                    char32 = utf::WideTo32(
                        reinterpret_cast<const wchar_t*>(data + i),
                        MathUtil::Min(sizeof(utf::Char32) / sizeof(wchar_t), size - i),
                        codepoints);
                }
                else
                {
                    static_assert(always_fail_v<CharType>, "Invalid string type for Split");
                }

                i += codepoints;

                if (separatorValues.Contains(char32))
                {
                    tokens.PushBack(std::move(workingString));

                    continue;
                }

                workingString.Append(char32);
            }
        }

        // finalize by pushing back remaining string
        if (workingString.Any())
        {
            tokens.PushBack(std::move(workingString));
        }

        return tokens;
    }

    template <class Container>
    static String Join(const Container& container)
    {
        String result;

        for (auto it = container.Begin(); it != container.End();)
        {
            result.Append(ToString(*it));

            ++it;
        }

        return result;
    }

    template <class Container>
    static String Join(const Container& container, const String& separator)
    {
        String result;

        for (auto it = container.Begin(); it != container.End();)
        {
            result.Append(ToString(*it));

            ++it;

            if (it != container.End())
            {
                result.Append(separator);
            }
        }

        return result;
    }

    template <class Container, class JoinByFunction>
    static String Join(const Container& container, const String& separator, JoinByFunction&& joinByFunction)
    {
        FunctionWrapper<NormalizedType<JoinByFunction>> joinByFunc(std::forward<JoinByFunction>(joinByFunction));

        String result;

        for (auto it = container.Begin(); it != container.End();)
        {
            result.Append(ToString(joinByFunc(*it)));

            ++it;

            if (it != container.End())
            {
                result.Append(separator);
            }
        }

        return result;
    }

    template <class Container>
    HYP_NODISCARD static String Join(const Container& container, WidestCharType separator)
    {
        String result;

        for (auto it = container.Begin(); it != container.End();)
        {
            result.Append(ToString(*it));

            ++it;

            if (it != container.End())
            {
                if constexpr (isUtf8 && std::is_same_v<utf::Char32, decltype(separator)>)
                {
                    size_t codepoints = 0;
                    utf::Char8 separatorBytes[sizeof(utf::Char32) + 1] = { '\0' };
                    utf::Char32to8(separator, separatorBytes, codepoints);

                    HYP_CORE_ASSERT(codepoints <= HYP_ARRAY_SIZE(separatorBytes));

                    for (size_t codepoint = 0; codepoint < codepoints; codepoint++)
                    {
                        result.Append(separatorBytes[codepoint]);
                    }
                }
                else
                {
                    result.Append(separator);
                }
            }
        }

        return result;
    }

    template <class Container, class JoinByFunction>
    HYP_NODISCARD static String Join(const Container& container, WidestCharType separator, JoinByFunction&& joinByFunction)
    {
        FunctionWrapper<NormalizedType<JoinByFunction>> joinByFunc(std::forward<JoinByFunction>(joinByFunction));

        String result;

        for (auto it = container.Begin(); it != container.End();)
        {
            result.Append(ToString(joinByFunc(*it)));

            ++it;

            if (it != container.End())
            {
                if constexpr (isUtf8 && std::is_same_v<utf::Char32, decltype(separator)>)
                {
                    size_t codepoints = 0;
                    utf::Char8 separatorBytes[sizeof(utf::Char32) + 1] = { '\0' };
                    utf::Char32to8(separator, separatorBytes, codepoints);

                    HYP_CORE_ASSERT(codepoints <= HYP_ARRAY_SIZE(separatorBytes));

                    for (size_t codepoint = 0; codepoint < codepoints; codepoint++)
                    {
                        result.Append(separatorBytes[codepoint]);
                    }
                }
                else
                {
                    result.Append(separator);
                }
            }
        }

        return result;
    }

    HYP_NODISCARD static String Base64Encode(const Array<ubyte>& bytes)
    {
        static const utf::Char8 alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        String out;

        uint32 i = 0;
        int j = -6;

        for (auto&& c : bytes)
        {
            i = (i << 8) + static_cast<ValueType>(c);
            j += 8;

            while (j >= 0)
            {
                out.Append(alphabet[(i >> j) & 0x3F]);
                j -= 6;
            }
        }

        if (j > -6)
        {
            out.Append(alphabet[((i << 8) >> (j + 8)) & 0x3F]);
        }

        while (out.Size() % 4 != 0)
        {
            out.Append('=');
        }

        return out;
    }

    HYP_NODISCARD static Array<ubyte> Base64Decode(const String& in)
    {
        static const int lookupTable[] = {
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, 62, -1, -1, -1, 63,
            52, 53, 54, 55, 56, 57, 58, 59,
            60, 61, -1, -1, -1, -1, -1, -1,
            -1, 0, 1, 2, 3, 4, 5, 6,
            7, 8, 9, 10, 11, 12, 13, 14,
            15, 16, 17, 18, 19, 20, 21, 22,
            23, 24, 25, -1, -1, -1, -1, -1,
            -1, 26, 27, 28, 29, 30, 31, 32,
            33, 34, 35, 36, 37, 38, 39, 40,
            41, 42, 43, 44, 45, 46, 47, 48,
            49, 50, 51, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1
        };

        Array<ubyte> out;

        uint32 i = 0;
        int j = -8;

        for (auto&& c : in)
        {
            if (lookupTable[c] == -1)
            {
                break;
            }

            i = (i << 6) + lookupTable[c];
            j += 6;

            if (j >= 0)
            {
                out.PushBack(ubyte((i >> j) & 0xFF));
                j -= 8;
            }
        }

        return out;
    }

    HYP_NODISCARD String<UTF8, TAllocator> ToUtf8() const
    {
        if constexpr (isUtf8)
        {
            return *this;
        }
        else if constexpr (isAnsi)
        {
            return String<UTF8, TAllocator>(Data());
        }
        else if constexpr (isUtf16)
        {
            size_t len = utf::ToUtf8(Data(), Data() + Size(), nullptr);

            if (len == 0)
            {
                return String<UTF8, TAllocator>::empty;
            }

            Array<utf::Char8> buffer;
            buffer.Resize(len + 1);

            utf::ToUtf8(Data(), Data() + Size(), buffer.Data());

            return String<UTF8, TAllocator>(buffer.ToByteView());
        }
        else if constexpr (isUtf32)
        {
            size_t len = utf::ToUtf8(Data(), Data() + Size(), nullptr);

            if (len == 0)
            {
                return String<UTF8, TAllocator>::empty;
            }

            Array<utf::Char8> buffer;
            buffer.Resize(len + 1);

            utf::ToUtf8(Data(), Data() + Size(), buffer.Data());

            return String<UTF8, TAllocator>(buffer.ToByteView());
        }
        else if constexpr (isWide)
        {
            size_t len = utf::ToUtf8(Data(), Data() + Size(), nullptr);

            if (len == 0)
            {
                return String<UTF8, TAllocator>::empty;
            }

            Array<utf::Char8> buffer;
            buffer.Resize(len + 1);

            utf::ToUtf8(Data(), Data() + Size(), buffer.Data());

            return String<UTF8, TAllocator>(buffer.ToByteView());
        }
        else
        {
            return String<UTF8, TAllocator>::empty;
        }
    }

    /*! \brief Converts the string to ANSI encoding.
     * Characters that cannot be represented in ANSI will be replaced with \p fallbackCharacter.
     *  If \p fallbackCharacter is not 0, the character will be used as a replacement for characters that cannot be represented in ANSI.
     *  Otherwise, the character will be skipped in the resulting string.
     */
    HYP_NODISCARD String<ANSI, TAllocator> ToAnsi(char fallbackCharacter = '?') const
    {
        if constexpr (isAnsi)
        {
            return *this;
        }
        else
        {
            const utilities::StringView<TStringType> view(*this);

            String<ANSI, TAllocator> result;
            result.Reserve(view.Length());

            for (auto it = view.Begin(); it != view.End(); ++it)
            {
                const WidestCharType ch = *it;

                if (ch <= 0xFF)
                {
                    result.Append(static_cast<typename String<ANSI, TAllocator>::CharType>(ch));
                }
                else if (fallbackCharacter != 0)
                {
                    result.Append(static_cast<typename String<ANSI, TAllocator>::CharType>(fallbackCharacter));
                }
            }

            return result;
        }
    }

    /*! \brief Converts the string to ANSI encoding.
     * Characters that cannot be represented in ANSI will be replaced using the provided \p fallbackMap.
     *  If a character is not found in the \p fallbackMap, and \p fallbackCharacter is not 0, the character will be used as a replacement.
     *  Otherwise, the character will be skipped in the resulting string.
     */
    template <class FallbackMapType>
    HYP_NODISCARD String<ANSI, TAllocator> ToAnsi(const FallbackMapType& fallbackMap, char fallbackCharacter = '?') const
    {
        if constexpr (isAnsi)
        {
            return *this;
        }
        else
        {
            const utilities::StringView<TStringType> view(*this);

            String<ANSI, TAllocator> result;
            result.Reserve(view.Length());

            for (auto it = view.Begin(); it != view.End(); ++it)
            {
                const WidestCharType ch = *it;

                if (ch <= 0xFF)
                {
                    result.Append(static_cast<typename String<ANSI, TAllocator>::CharType>(ch));
                }
                else
                {
                    auto fallbackIt = fallbackMap.Find(ch);

                    if (fallbackIt != fallbackMap.End())
                    {
                        result.Append(static_cast<typename String<ANSI, TAllocator>::CharType>(fallbackIt->second));
                    }
                    else if (fallbackCharacter != 0)
                    {
                        result.Append(static_cast<typename String<ANSI, TAllocator>::CharType>(fallbackCharacter));
                    }
                }
            }

            return result;
        }
    }

    HYP_NODISCARD String<WIDE_CHAR, TAllocator> ToWide() const
    {
        if constexpr (isWide)
        {
            return *this;
        }
        else if constexpr (isUtf8 || isAnsi)
        {
            size_t len = utf::ToWide(reinterpret_cast<const utf::Char8*>(Data()), reinterpret_cast<const utf::Char8*>(Data()) + Size(), nullptr);

            if (len == 0)
            {
                return String<WIDE_CHAR, TAllocator>::empty;
            }

            Array<wchar_t> buffer;
            buffer.Resize(len + 1);

            utf::ToWide(reinterpret_cast<const utf::Char8*>(Data()), reinterpret_cast<const utf::Char8*>(Data()) + Size(), buffer.Data());

            return String<WIDE_CHAR, TAllocator>(buffer.Data());
        }
        else if constexpr (isUtf16)
        {
            size_t len = utf::ToWide(Data(), Data() + Size(), nullptr);

            if (len == 0)
            {
                return String<WIDE_CHAR, TAllocator>::empty;
            }

            Array<wchar_t> buffer;
            buffer.Resize(len + 1);

            utf::ToWide(Data(), Data() + Size(), buffer.Data());

            return String<WIDE_CHAR, TAllocator>(buffer.Data());
        }
        else if constexpr (isUtf32)
        {
            size_t len = utf::ToWide(Data(), Data() + Size(), nullptr);

            if (len == 0)
            {
                return String<WIDE_CHAR, TAllocator>::empty;
            }

            Array<wchar_t> buffer;
            buffer.Resize(len + 1);

            utf::ToWide(Data(), Data() + Size(), buffer.Data());

            return String<WIDE_CHAR, TAllocator>(buffer.Data());
        }
        else
        {
            return String<WIDE_CHAR, TAllocator>::empty;
        }
    }

    template <class Integral, class = std::enable_if_t<std::is_integral_v<NormalizedType<Integral>>>>
    HYP_NODISCARD static String ToString(Integral value)
    {
        size_t resultSize;
        utf::ToString<Integral, CharType>(value, resultSize, nullptr);

        HYP_CORE_ASSERT(resultSize >= 1);

        String result;
        result.Reserve(resultSize - 1); // String class automatically adds 1 for null character

        Array<CharType> buffer;
        buffer.Resize(resultSize);

        utf::ToString<Integral, CharType>(value, resultSize, buffer.Data());

        for (size_t i = 0; i < resultSize - 1; i++)
        {
            result.Append(buffer[i]);
        }

        return result;
    }

    template <class Ty, class TyN = NormalizedType<Ty>, class = std::enable_if_t<!std::is_integral_v<TyN> && HYP_HAS_METHOD(TyN, ToString)>>
    HYP_NODISCARD static String ToString(Ty&& value)
    {
        return String(value.ToString());
    }

    HYP_NODISCARD HYP_FORCE_INLINE static String ToString(const String& value)
    {
        return value;
    }

    HYP_NODISCARD HYP_FORCE_INLINE static String ToString(String&& value)
    {
        return value;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode(::Hyperion::FNV1::DoHashString(Data()));
    }

    HYP_DEF_STL_BEGIN_END(Base::Data(), Base::Data() + Size())

protected:
    uint32 m_length;
};

template <int TStringType, class TAllocator>
const String<TStringType, TAllocator> String<TStringType, TAllocator>::empty = String();

template <int TStringType, class TAllocator>
String<TStringType, TAllocator>::String()
    : Base(),
      m_length(0)
{
    // add null character for easy conversion to C-style strings via Data() and operator*()
    Base::ResizeZeroed(1);
}

template <int TStringType, class TAllocator>
String<TStringType, TAllocator>::String(const CharType* str)
    : String()
{
    if (str == nullptr)
    {
        return;
    }

    size_t codepoints;
    const size_t len = utf::StringLength<CharType, isUtf8>(str, codepoints);

    if (len == -1)
    {
        // invalid utf8 string
        return;
    }

    m_length = len;

    Base::ResizeZeroed(codepoints + 1);

    CharType* data = Data();

    for (size_t i = 0; i < codepoints; ++i)
    {
        data[i] = str[i];
    }
}

template <int TStringType, class TAllocator>
String<TStringType, TAllocator>::String(const CharType* _begin, const CharType* _end)
    : String()
{
    if (_begin >= _end)
    {
        return;
    }

    const size_t size = size_t(_end - _begin);

    if constexpr (isUtf8)
    {
        const size_t len = utf::StringLength(_begin, _end);

        if (len == -1)
        {
            // invalid utf8 string
            return;
        }

        HYP_CORE_ASSERT(len <= UINT32_MAX);

        m_length = uint32(len);
    }
    else
    {
        HYP_CORE_ASSERT(size <= UINT32_MAX);

        m_length = uint32(size);
    }

    Base::ResizeZeroed(size + 1);

    CharType* data = Data();

    for (size_t i = 0; i < size; ++i)
    {
        data[i] = _begin[i];
    }
}

template <int TStringType, class TAllocator>
String<TStringType, TAllocator>::String(const String& other)
    : Base(static_cast<const Base&>(other)),
      m_length(other.m_length)
{
}

template <int TStringType, class TAllocator>
String<TStringType, TAllocator>::String(String&& other) noexcept
    : Base(static_cast<Base&&>(std::move(other))),
      m_length(other.m_length)
{
    other.Clear();
}

template <int TStringType, class TAllocator>
String<TStringType, TAllocator>::String(ConstByteView byteView)
    : Base(),
      m_length(0)
{
    size_t size = byteView.Size();

    for (size_t index = 0; index < size; ++index)
    {

        HYP_CORE_ASSERT(byteView.Data()[index] >= 0 && byteView.Data()[index] <= 255, "Out of character range");

        if (byteView.Data()[index] == '\0')
        {
            size = index;

            break;
        }
    }

    Base::ResizeZeroed((size / sizeof(CharType)) + 1); // +1 for null char
    Memory::Copy(Data(), byteView.Data(), size / sizeof(CharType));

    size_t len = utf::StringLength<CharType, isUtf8>(Base::Data());
    HYP_CORE_ASSERT(len <= UINT32_MAX);

    m_length = uint32(len);
}

template <int TStringType, class TAllocator>
String<TStringType, TAllocator>::~String()
{
}

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::operator=(const CharType* str) -> String&
{
    if (str == nullptr)
    {
        Clear();
        return *this;
    }

    String<TStringType, TAllocator>::operator=(String(str));

    return *this;
}

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::operator=(const String& other) -> String&
{
    if (this == &other)
    {
        return *this;
    }

    Base::operator=(static_cast<const Base&>(other));
    m_length = other.m_length;

    return *this;
}

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::operator=(String&& other) noexcept -> String&
{
    if (this == &other)
    {
        return *this;
    }

    Base::operator=(static_cast<Base&&>(std::move(other)));

    m_length = other.m_length;

    other.Clear();

    return *this;
}

// template <int TStringType, class TAllocator>
// auto String<TStringType, TAllocator>::operator+(const String &other) const -> String
// {
//     String result(*this);
//     result.Append(other);

//     return result;
// }

// template <int TStringType, class TAllocator>
// auto String<TStringType, TAllocator>::operator+=(const String &other) -> String&
// {
//     Append(other);

//     return *this;
// }

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::operator+(const utilities::StringView<TStringType>& stringView) const -> String
{
    String result(*this);
    result.Append(stringView);

    return result;
}

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::operator+=(const utilities::StringView<TStringType>& stringView) -> String&
{
    Append(stringView);

    return *this;
}

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::operator+(CharType ch) const -> String
{
    String result(*this);
    result.Append(ch);

    return result;
}

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::operator+=(CharType ch) -> String&
{
    Append(ch);

    return *this;
}

template <int TStringType, class TAllocator>
bool String<TStringType, TAllocator>::operator==(const String& other) const
{
    if (this == &other)
    {
        return true;
    }

    if (Size() != other.Size() || m_length != other.m_length)
    {
        return false;
    }

    if (Empty() && other.Empty())
    {
        return true;
    }

    return utf::StringCompare<CharType, isUtf8>(Base::Data(), other.Data()) == 0;
}

template <int TStringType, class TAllocator>
bool String<TStringType, TAllocator>::operator!=(const String& other) const
{
    return !operator==(other);
}

template <int TStringType, class TAllocator>
bool String<TStringType, TAllocator>::operator==(const CharType* str) const
{
    if (!str)
    {
        return *this == empty;
    }

    const size_t len = utf::StringLength<CharType, isUtf8>(str);

    if (len == SIZE_MAX)
    {
        return false; // invalid utf string
    }

    if (size_t(m_length) != len)
    {
        return false;
    }

    if (Empty() && len == 0)
    {
        return true;
    }

    return utf::StringCompare<CharType, isUtf8>(Base::Data(), str) == 0;
}

template <int TStringType, class TAllocator>
bool String<TStringType, TAllocator>::operator!=(const CharType* str) const
{
    return !operator==(str);
}

template <int TStringType, class TAllocator>
bool String<TStringType, TAllocator>::operator<(const String& other) const
{
    return utf::StringCompare<CharType, isUtf8>(Base::Data(), other.Data()) < 0;
}

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::operator[](size_t index) const -> const CharType&
{
    return Base::operator[](index);
}

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::GetChar(size_t index) const -> WidestCharType
{
    const size_t size = Size();

#if HYP_DEBUG_MODE
    HYP_CORE_ASSERT(index < size);
#endif

    if constexpr (isUtf8)
    {
        return utf::CharAt(reinterpret_cast<const utf::Char8*>(Data()), size, index);
    }
    else
    {
        return Base::operator[](index);
    }
}

template <int TStringType, class TAllocator>
void String<TStringType, TAllocator>::Append(const utilities::StringView<TStringType>& stringView)
{
    if (Size() + stringView.Size() + 1 >= Base::Capacity())
    {
#if !defined(HYP_USE_SLIM_ARRAY) || !HYP_USE_SLIM_ARRAY
        if (Base::Capacity() >= Size() + stringView.Size() + 1)
        {
            Base::ResetOffsets();
        }
        else
#endif // !HYP_USE_SLIM_ARRAY
        {
            Base::SetCapacity(Base::CalculateDesiredCapacity(Size() + stringView.Size() + 1));
        }
    }

    Base::PopBack(); // current NT char

    auto* buffer = Base::Data();

    Memory::Copy(buffer + Base::Size(), stringView.Data(), stringView.Size() * sizeof(CharType));

#if defined(HYP_USE_SLIM_ARRAY) && HYP_USE_SLIM_ARRAY
    uint32& backingArraySize = Base::size;
#else
    size_t& backingArraySize = Base::m_size;
#endif

    backingArraySize += stringView.Size();

    Base::PushBack(CharType { 0 });

    m_length += stringView.m_length;
}

template <int TStringType, class TAllocator>
void String<TStringType, TAllocator>::Append(const CharType* str)
{
    Append(utilities::StringView<TStringType>(str));
}

template <int TStringType, class TAllocator>
void String<TStringType, TAllocator>::Append(const CharType* _begin, const CharType* _end)
{
    Append(utilities::StringView<TStringType>(_begin, _end));
}

template <int TStringType, class TAllocator>
void String<TStringType, TAllocator>::Append(CharType value)
{
    // @FIXME: Don't actually need +2 if null char exists
    if (Size() + 2 >= Base::Capacity())
    {
#if !defined(HYP_USE_SLIM_ARRAY) || !HYP_USE_SLIM_ARRAY
        if (Base::Capacity() >= Size() + 2)
        {
            Base::ResetOffsets();
        }
        else
#endif // !HYP_USE_SLIM_ARRAY
        {
            Base::SetCapacity(Base::CalculateDesiredCapacity(Size() + 2));
        }
    }

#if defined(HYP_USE_SLIM_ARRAY) && HYP_USE_SLIM_ARRAY
    uint32& backingArraySize = Base::size;
#else
    size_t& backingArraySize = Base::m_size;
#endif

    // swap null char with value and add null char at the end
    Base::Data()[backingArraySize - 1] = value;
    Base::Data()[backingArraySize++] = 0;

    ++m_length;
}

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::PopFront() -> typename Base::ValueType
{
    --m_length;
    return Base::PopFront();
}

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::PopBack() -> typename Base::ValueType
{
    HYP_CORE_ASSERT(!Base::Empty() > 1, "Cannot pop back from an empty string");

    CharType lastChar = 0;
    std::swap(Base::Data()[Base::Size() - 2], lastChar); // -2 because we want the last character before the null terminator

    Base::PopBack(); // remove current null terminator, leaving the last character in place

    --m_length;

    return lastChar;
}

template <int TStringType, class TAllocator>
void String<TStringType, TAllocator>::Clear()
{
    Base::Resize(1);
    Base::Back() = CharType { 0 }; // null-terminate
    m_length = 0;
}

template <int TStringType, class TAllocator>
bool String<TStringType, TAllocator>::StartsWith(const String& other) const
{
    if (Size() < other.Size())
    {
        return false;
    }

    return std::equal(Base::Begin(), Base::Begin() + other.Size(), other.Base::Begin());
}

template <int TStringType, class TAllocator>
bool String<TStringType, TAllocator>::EndsWith(const String& other) const
{
    if (Size() < other.Size())
    {
        return false;
    }

    return std::equal(Base::Begin() + Size() - other.Size(), Base::End(), other.Base::Begin());
}

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::ToLower() const -> String
{
    String result;

    if constexpr (isUtf8)
    {
        result.Reserve(Size());
    }
    else
    {
        result = *this;
    }

    for (size_t i = 0; i < Size();)
    {
        if constexpr (isUtf8)
        {
            size_t codepoints = 0;

            union
            {
                utf::Char32 charU32;
                int charI32;
            };

            // evil union byte magic
            charU32 = utf::Char8to32(Data() + i, sizeof(utf::Char32), codepoints);
            charI32 = tolower(charI32);

            result.Append(charU32);

            i += codepoints;
        }
        else
        {
            result.Data()[i] = CharType(tolower(Data()[i]));

            i++;
        }
    }

    return result;
}

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::ToUpper() const -> String
{
    String result;

    if constexpr (isUtf8)
    {
        result.Reserve(Size());
    }
    else
    {
        result = *this;
    }

    for (size_t i = 0; i < Size();)
    {
        if constexpr (isUtf8)
        {
            size_t codepoints = 0;

            union
            {
                utf::Char32 charU32;
                int charI32;
            };

            // evil union byte magic
            charU32 = utf::Char8to32(Data() + i, sizeof(utf::Char32), codepoints);
            charI32 = toupper(charI32);

            result.Append(charU32);

            i += codepoints;
        }
        else
        {
            result.Data()[i] = CharType(toupper(Data()[i]));

            i++;
        }
    }

    return result;
}

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::Trimmed() const -> String
{
    return TrimmedLeft().TrimmedRight();
}

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::TrimmedLeft() const -> String
{
    String res;
    res.Reserve(Size());

    size_t startIndex;

    for (startIndex = 0; startIndex < Size(); ++startIndex)
    {
        if (!utf::IsWhitespace(Data()[startIndex]))
        {
            break;
        }
    }

    for (size_t index = startIndex; index < Size(); ++index)
    {
        res.Append(Data()[index]);
    }

    return res;
}

template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::TrimmedRight() const -> String
{
    String res;
    res.Reserve(Size());

    size_t startIndex;

    for (startIndex = Size(); startIndex > 0; --startIndex)
    {
        if (!utf::IsWhitespace(Data()[startIndex - 1]))
        {
            break;
        }
    }

    for (size_t index = 0; index < startIndex; ++index)
    {
        res.Append(Data()[index]);
    }

    return res;
}

template <int TStringType, class TAllocator>
String<TStringType, TAllocator> String<TStringType, TAllocator>::ReplaceAll(const String& search, const String& replace) const
{
    String tmp(*this);

    String result;
    result.Reserve(Size());

    size_t index = 0;

    while (index < Length())
    {
        auto foundIndex = tmp.FindFirstIndex(search);

        if (foundIndex == NotFound)
        {
            result.Append(tmp);
            break;
        }

        result.Append(tmp.Substr(0, foundIndex));
        result.Append(replace);

        tmp = tmp.Substr(foundIndex + search.Length());
        index += foundIndex + search.Length();
    }

    return result;
}

template <int TStringType, class TAllocator>
String<TStringType, TAllocator> String<TStringType, TAllocator>::Escape() const
{
    const size_t size = Size();

    String result;
    result.Reserve(size);

    for (WidestCharType ch : *this)
    {
        if (ch == 0)
        {
            break;
        }

        CharType charBuffer[3] = { 0 };

        switch (ch)
        {
        case '\n':
            result.Append(detail::ConvertChars("\\n", charBuffer));
            break;
        case '\r':
            result.Append(detail::ConvertChars("\\r", charBuffer));
            break;
        case '\t':
            result.Append(detail::ConvertChars("\\t", charBuffer));
            break;
        case '\v':
            result.Append(detail::ConvertChars("\\v", charBuffer));
            break;
        case '\b':
            result.Append(detail::ConvertChars("\\b", charBuffer));
            break;
        case '\f':
            result.Append(detail::ConvertChars("\\f", charBuffer));
            break;
        case '\a':
            result.Append(detail::ConvertChars("\\a", charBuffer));
            break;
        case '\\':
            result.Append(detail::ConvertChars("\\\\", charBuffer));
            break;
        case '\"':
            result.Append(detail::ConvertChars("\\\"", charBuffer));
            break;
        case '\'':
            result.Append(detail::ConvertChars("\\\'", charBuffer));
            break;
        default:
            result.Append(ch);
            break;
        }
    }

    return result;
}

#if 0
template <int TStringType, class TAllocator>
auto String<TStringType, TAllocator>::Substr(size_t first, size_t last) const -> String
{
    if (first == size_t(-1)) {
        return *this;
    }

    last = MathUtil::Max(last, first);

    const auto size = Size();

    if constexpr (isUtf8) {
        String result;

        size_t charIndex = 0;

        for (size_t i = 0; i < size;) {
            auto c = Base::operator[](i);

            if (charIndex >= last) {
                break;
            }

            if (charIndex >= first) {
                if (c >= 0 && c <= 127) {
                    result.Append(Base::operator[](i++));
                } else if ((c & 0xE0) == 0xC0) {
                    if (i + 1 > size) {
                        break;
                    }

                    result.Append(Base::operator[](i++));
                    result.Append(Base::operator[](i++));
                } else if ((c & 0xF0) == 0xE0) {
                    if (i + 2 > size) {
                        break;
                    }

                    result.Append(Base::operator[](i++));
                    result.Append(Base::operator[](i++));
                    result.Append(Base::operator[](i++));
                } else if ((c & 0xF8) == 0xF0) {
                    if (i + 3 > size) {
                        break;
                    }

                    result.Append(Base::operator[](i++));
                    result.Append(Base::operator[](i++));
                    result.Append(Base::operator[](i++));
                    result.Append(Base::operator[](i++));
                }
            } else {
                if (c >= 0 && c <= 127) {
                    i++;
                } else if ((c & 0xE0) == 0xC0) {
                    i += 2;
                } else if ((c & 0xF0) == 0xE0) {
                    i += 3;
                } else if ((c & 0xF8) == 0xF0) {
                    i += 4;
                }
            }

            ++charIndex;
        }

        return result;
    } else {
        if (first >= size) {
            return String::empty;
        }

        String result;
        result.Reserve(MathUtil::Min(size, last) - first);

        for (size_t i = first; i < size; i++) {
            auto c = Base::operator[](i);

            if (i >= last) {
                break;
            }

            result.Append(c);
        }

        return result;
    }
}
#endif

#if 0
template <int TStringType, class TAllocator>
String<TStringType, TAllocator> operator+(const CharType *str, const String<TStringType, TAllocator> &other)
{
    return String<TStringType, TAllocator>(str) + other;
}
#endif

template <int TStringType, class TAllocator>
constexpr bool operator<(const String<TStringType, TAllocator>& lhs, const utilities::StringView<TStringType>& rhs)
{
    if (!lhs.Data())
    {
        return true;
    }

    if (!rhs.Data())
    {
        return false;
    }

    // @FIXME: Use strncmp instead as stringView may not be null-terminated
    return utf::StringCompare<typename utilities::StringView<TStringType>::CharType, utilities::StringView<TStringType>::isUtf8>(lhs.Data(), rhs.Data(), MathUtil::Min(lhs.Length(), rhs.Length())) < 0;
}

template <int TStringType, class TAllocator>
constexpr bool operator<(const utilities::StringView<TStringType>& lhs, const String<TStringType, TAllocator>& rhs)
{
    if (!lhs.Data())
    {
        return true;
    }

    if (!rhs.Data())
    {
        return false;
    }

    // @FIXME: Use strncmp instead as stringView may not be null-terminated
    return utf::StringCompare<typename utilities::StringView<TStringType>::CharType, utilities::StringView<TStringType>::isUtf8>(lhs.Data(), rhs.Data(), MathUtil::Min(lhs.Length(), rhs.Length())) < 0;
}

template <int TStringType, class TAllocator>
inline constexpr bool operator==(const String<TStringType, TAllocator>& lhs, const utilities::StringView<TStringType>& rhs)
{
    if (lhs.Size() != rhs.Size())
    {
        return false;
    }

    if (lhs.Data() == rhs.Data())
    {
        return true;
    }

    return Memory::StrEqual(lhs.Data(), rhs.Data(), lhs.Size());
}

template <int TStringType, class TAllocator>
inline constexpr bool operator==(const utilities::StringView<TStringType>& lhs, const String<TStringType, TAllocator>& rhs)
{
    if (lhs.Size() != rhs.Size())
    {
        return false;
    }

    if (lhs.Data() == rhs.Data())
    {
        return true;
    }

    return Memory::StrEqual(lhs.Data(), rhs.Data(), lhs.Size());
}

} // namespace containers

// StringView + String
template <int TStringType, class TAllocator>
inline containers::String<TStringType, TAllocator> operator+(const utilities::StringView<TStringType>& lhs, const containers::String<TStringType, TAllocator>& rhs)
{
    return containers::String<TStringType, TAllocator>(lhs) + rhs;
}

// StringView + StringView
template <int TStringType>
inline containers::String<TStringType, DynamicAllocator> operator+(const utilities::StringView<TStringType>& lhs, const utilities::StringView<TStringType>& rhs)
{
    return containers::String<TStringType, DynamicAllocator>(lhs) + rhs;
}

// StringView + char pointer
template <int TStringType>
inline containers::String<TStringType, DynamicAllocator> operator+(const utilities::StringView<TStringType>& lhs, const typename containers::String<TStringType, DynamicAllocator>::CharType* rhs)
{
    return containers::String<TStringType, DynamicAllocator>(lhs) + rhs;
}

// char pointer + StringView
template <int TStringType>
inline containers::String<TStringType, DynamicAllocator> operator+(const typename containers::String<TStringType, DynamicAllocator>::CharType* lhs, const utilities::StringView<TStringType>& rhs)
{
    return containers::String<TStringType, DynamicAllocator>(lhs) + rhs;
}

// char pointer + String
template <int TStringType, class TAllocator>
inline containers::String<TStringType, TAllocator> operator+(const typename containers::String<TStringType, TAllocator>::CharType* lhs, const containers::String<TStringType, TAllocator>& rhs)
{
    return containers::String<TStringType, TAllocator>(lhs) + rhs;
}

} // namespace Hyperion
