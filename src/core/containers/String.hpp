/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STRING_HPP
#define HYPERION_STRING_HPP

#include <math/MathUtil.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/StringFwd.hpp>
#include <core/utilities/Span.hpp>
#include <core/utilities/StringView.hpp>
#include <core/memory/Memory.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>

#include <type_traits>

namespace hyperion {

namespace utilities {
namespace detail {

template <int string_type>
class StringView;

} // namespace detail
} // namespace utilities

namespace containers {

namespace detail {
    
using namespace ::utf;

template <class CharType>
using CharArray = Array<CharType, 64u>;

/*! \brief Dynamic string class that natively supports UTF-8, as well as UTF-16, UTF-32, wide chars and ANSI. */
template <int StringType>
class String : Array<typename StringTypeImpl<StringType>::CharType, 64u>
{
public:
    using CharType = typename StringTypeImpl<StringType>::CharType;
    using WidestCharType = typename StringTypeImpl<StringType>::WidestCharType;

protected:
    using Base = Array<CharType, 64u>;

public:
    using ValueType = typename Base::ValueType;
    using KeyType = typename Base::KeyType;

    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    static constexpr bool is_contiguous = true;

    static const String empty;

    static constexpr bool is_ansi = StringType == ANSI;
    static constexpr bool is_utf8 = StringType == UTF8;
    static constexpr bool is_utf16 = StringType == UTF16;
    static constexpr bool is_utf32 = StringType == UTF32;
    static constexpr bool is_wide = StringType == WIDE_CHAR;

    static_assert(!is_utf8 || (std::is_same_v<CharType, char> || std::is_same_v<CharType, unsigned char>), "UTF-8 Strings must have CharType equal to char or unsigned char");
    static_assert(!is_ansi || (std::is_same_v<CharType, char> || std::is_same_v<CharType, unsigned char>), "ANSI Strings must have CharType equal to char or unsigned char");
    static_assert(!is_utf16 || std::is_same_v<CharType, utf::u16char>, "UTF-16 Strings must have CharType equal to utf::u16char");
    static_assert(!is_utf32 || std::is_same_v<CharType, utf::u32char>, "UTF-32 Strings must have CharType equal to utf::u32char");
    static_assert(!is_wide || std::is_same_v<CharType, wchar_t>, "Wide Strings must have CharType equal to wchar_t");

    static constexpr int string_type = StringType;

    static constexpr SizeType not_found = SizeType(-1);

    static_assert(!is_utf8 || std::is_same_v<CharType, char>, "UTF-8 Strings must have CharType equal to char");

    template <int FirstStringType, int SecondStringType>
    friend constexpr bool operator<(const containers::detail::String<FirstStringType> &lhs, const StringView<SecondStringType> &rhs);

    template <int FirstStringType, int SecondStringType>
    friend constexpr bool operator<(const StringView<FirstStringType> &lhs, const containers::detail::String<SecondStringType> &rhs);

    template <int FirstStringType, int SecondStringType>
    friend constexpr bool operator==(const containers::detail::String<FirstStringType> &lhs, const StringView<SecondStringType> &rhs);

    template <int FirstStringType, int SecondStringType>
    friend constexpr bool operator==(const StringView<FirstStringType> &lhs, const containers::detail::String<SecondStringType> &rhs);

    String();
    String(const String &other);
    String(const CharType *str);
    String(const CharType *_begin, const CharType *_end);
    explicit String(const CharArray<CharType> &char_array);
    explicit String(ConstByteView byte_view);

    template <int other_string_type>
    String(utilities::detail::StringView<other_string_type> string_view)
    {
        *this = string_view;
    }

    template <int other_string_type, std::enable_if_t<other_string_type != string_type, int> = 0>
    String(const String<other_string_type> &other)
        : String()
    {
        *this = other;
    }
    
    String(String &&other) noexcept;
    ~String();

    String &operator=(const CharType *str);
    String &operator=(const String &other);
    String &operator=(String &&other) noexcept;
    
    template <int other_string_type>
    HYP_FORCE_INLINE String &operator=(utilities::detail::StringView<other_string_type> string_view)
    {
        Clear();
        Append(string_view.Data(), string_view.Data() + string_view.Size());

        return *this;
    }
    
    template <int other_string_type, std::enable_if_t<other_string_type != string_type, int> = 0>
    HYP_FORCE_INLINE String &operator=(const String<other_string_type> &other)
    {
        Clear();
        Append(other.Data(), other.Data() + other.Size());

        return *this;
    }

    String operator+(const String &other) const;
    String operator+(String &&other) const;
    String operator+(CharType ch) const;
    
    template <class U32Char, typename = std::enable_if_t<is_utf8 && std::is_same_v<U32Char, u32char>, int>>
    HYP_FORCE_INLINE String operator+(U32Char ch) const
        { return String(*this) += ch; }
    
    String &operator+=(const String &other);
    String &operator+=(String &&other);
    String &operator+=(CharType ch);
    
    template <class U32Char, typename = std::enable_if_t<is_utf8 && std::is_same_v<U32Char, u32char>, int>>
    HYP_FORCE_INLINE String &operator+=(U32Char ch)
        { Append(ch); return *this; }

    bool operator==(const String &other) const;
    bool operator==(const CharType *str) const;
    bool operator!=(const String &other) const;
    bool operator!=(const CharType *str) const;

    bool operator<(const String &other) const;

    /*! \brief Raw access of the character data of the string at index.
     * \note For UTF-8 Strings, the returned value may not be a valid UTF-8 character,
     *      so in most cases, \ref{GetChar} should be used instead.
     *
     * \ref{index} must be less than \ref{Size()}.
     */
    const CharType operator[](SizeType index) const;

    /*! \brief Get a char from the String at the given index.
     * For UTF-8 strings, the character is encoded as a 32-bit value.
     * \note If needing to access raw character data, \ref{operator[]} should be used instead.
     *
     * \ref{index} must be less than \ref{Length()}.
     */
    WidestCharType GetChar(SizeType index) const;

    /*! \brief Return the data size in characters. Note, UTF-8 strings can have a shorter length than size. */
    HYP_FORCE_INLINE SizeType Size() const
        { return Base::Size() - 1; /* for NT char */ }

    /*! \brief Return the length of the string in characters. Note, UTF-8 strings can have a shorter length than size. */
    HYP_FORCE_INLINE SizeType Length() const
        { return m_length; }

    /*! \brief Access the raw data of the string.
        \note For UTF-8 strings, ensure proper care is taken when accessing the data, as indexing via a character index may not
              yield a valid character. */
    HYP_FORCE_INLINE typename Base::ValueType *Data()
        { return Base::Data(); }

    /*! \brief Access the raw data of the string.
        \note For UTF-8 strings, ensure proper care is taken when accessing the data, as indexing via a character index may not
              yield a valid character. */
    HYP_FORCE_INLINE const typename Base::ValueType *Data() const
        { return Base::Data(); }

    HYP_FORCE_INLINE operator utilities::detail::StringView<string_type>() const
        { return utilities::detail::StringView<string_type>(Begin(), End(), Length()); }

    /*! \brief Conversion operator to return the raw data of the string. */
    HYP_FORCE_INLINE explicit operator const CharType *() const
        { return Base::Data(); }

    /*! \brief Dereference operator overload to return the raw data of the string. */
    HYP_FORCE_INLINE const CharType * operator*() const
        { return Base::Data(); }

    HYP_FORCE_INLINE typename Base::ValueType &Front()
        { return Base::Front(); }

    HYP_FORCE_INLINE const typename Base::ValueType &Front() const
        { return Base::Front(); }

    HYP_FORCE_INLINE typename Base::ValueType &Back()
        { return Base::GetBuffer()[Base::m_size - 2]; /* for NT char */ }

    HYP_FORCE_INLINE const typename Base::ValueType &Back() const
        { return Base::GetBuffer()[Base::m_size - 2]; /* for NT char */ }

    /*! \brief Check if the string contains the given character. */
    HYP_FORCE_INLINE bool Contains(CharType ch) const
        { return ch != CharType { 0 } && Base::Contains(ch); }

    /*! \brief Check if the string contains the given string. */
    HYP_FORCE_INLINE bool Contains(containers::detail::StringView<StringType> str) const
        { return FindIndex(str) != not_found; }

    /*! \brief Find the index of the first occurrence of the character in the string.
     * \note For UTF-8 strings, ensure accessing the character with the returned value is done via the \ref{GetChar} method,
     *       as the index is the character index, not the byte index. */
    HYP_FORCE_INLINE SizeType FindIndex(containers::detail::StringView<StringType> str) const
        { return containers::detail::StringView<StringType>(*this).FindIndex(str); }

    /*! \brief Check if the string is empty. */
    HYP_FORCE_INLINE bool Empty() const
        { return Size() == 0; }

    /*! \brief Check if the string contains any characters. */
    HYP_FORCE_INLINE bool Any() const
        { return Size() != 0; }

    /*! \brief Check if the string contains multi-byte characters. */
    HYP_FORCE_INLINE bool HasMultiByteChars() const
        { return Size() > Length(); }

    /*! \brief Reserve space for the string. \ref{capacity} + 1 is used, to make space for the null character. */
    HYP_FORCE_INLINE void Reserve(SizeType capacity)
        { Base::Reserve(capacity + 1); }

    HYP_FORCE_INLINE void Refit()
        { Base::Refit(); }

    HYP_FORCE_INLINE void Append(const String &other);
    HYP_FORCE_INLINE void Append(String &&other);
    HYP_FORCE_INLINE void Append(CharType value);

    HYP_FORCE_INLINE void Append(const CharType *str);
    HYP_FORCE_INLINE void Append(const CharType *_begin, const CharType *_end);

    template <class OtherCharType, typename = std::enable_if_t<is_utf8 && !std::is_same_v<OtherCharType, CharType> && (std::is_same_v<OtherCharType, u32char> || std::is_same_v<OtherCharType, u16char> || std::is_same_v<OtherCharType, wchar_t>), int>>
    HYP_FORCE_INLINE void Append(const OtherCharType *str)
    {
        const int size = utf::utf_strlen<OtherCharType, false>(str);

        Append(str, str + size);
    }

    template <class OtherCharType, typename = std::enable_if_t<is_utf8 && !std::is_same_v<OtherCharType, CharType> && (std::is_same_v<OtherCharType, u32char> || std::is_same_v<OtherCharType, u16char> || std::is_same_v<OtherCharType, wchar_t>), int>>
    HYP_FORCE_INLINE void Append(const OtherCharType *_begin, const OtherCharType *_end)
    {
        // const int size = utf::utf_strlen<OtherCharType, false>(str);

        // const SizeType size = SizeType(_end - _begin);

        if constexpr (std::is_same_v<OtherCharType, u32char>) {
            const uint32 len = utf::utf32_to_utf8(_begin, _end, nullptr);

            if (len == 0) {
                return;
            }

            Array<utf::u8char> buffer;
            buffer.Resize(len + 1);
            utf::utf32_to_utf8(_begin, _end, buffer.Data());

            for (SizeType i = 0; i < buffer.Size(); i++) {
                Append(CharType(buffer[i]));
            }
        } else if constexpr (std::is_same_v<OtherCharType, u16char>) {
            const uint32 len = utf::utf16_to_utf8(_begin, _end, nullptr);

            if (len == 0) {
                return;
            }

            Array<utf::u8char> buffer;
            buffer.Resize(len + 1);
            utf::utf16_to_utf8(_begin, _end, buffer.Data());

            for (SizeType i = 0; i < buffer.Size(); i++) {
                Append(CharType(buffer[i]));
            }
        } else if constexpr (std::is_same_v<OtherCharType, wchar_t>) {
            const uint32 len = utf::wide_to_utf8(_begin, _end, nullptr);

            if (len == 0) {
                return;
            }

            Array<utf::u8char> buffer;
            buffer.Resize(len + 1);
            utf::wide_to_utf8(_begin, _end, buffer.Data());

            for (SizeType i = 0; i < buffer.Size(); i++) {
                Append(CharType(buffer[i]));
            }
        } else {
            static_assert(resolution_failure<OtherCharType>, "Invalid character type");
        }
    }
    
    template <class OtherCharType, typename = std::enable_if_t<is_utf8 && !std::is_same_v<OtherCharType, CharType> && (std::is_same_v<OtherCharType, u32char> || std::is_same_v<OtherCharType, u16char> || std::is_same_v<OtherCharType, wchar_t>), int>>
    HYP_FORCE_INLINE void Append(OtherCharType ch)
    {
        int cp = 0;
        utf::u8char buffer[sizeof(utf::u32char) + 1] = { '\0' };
        utf::char32to8(static_cast<utf::u32char>(ch), &buffer[0], cp);

        for (int i = 0; i < cp; i++) {
            Append(CharType(buffer[i]));
        }
    }

    /*template <int other_string_type>
    void Concat(const String<other_string_type> &other);
    
    template <int other_string_type>
    void Concat(String<other_string_type> &&other);*/

    typename Base::ValueType PopBack();
    typename Base::ValueType PopFront();
    void Clear();

    bool StartsWith(const String &other) const;
    bool EndsWith(const String &other) const;

    HYP_NODISCARD String ToLower() const;
    HYP_NODISCARD String ToUpper() const;
    
    HYP_NODISCARD String Trimmed() const;
    HYP_NODISCARD String TrimmedLeft() const;
    HYP_NODISCARD String TrimmedRight() const;

    HYP_NODISCARD String Substr(SizeType first, SizeType last = MathUtil::MaxSafeValue<SizeType>()) const;

    HYP_NODISCARD String Escape() const;
    HYP_NODISCARD String ReplaceAll(const String &search, const String &replace) const;

    template <class ... SeparatorType>
    HYP_NODISCARD Array<String> Split(SeparatorType ... separators) const 
    {
        hyperion::FixedArray<WidestCharType, sizeof...(separators)> separator_values { WidestCharType(separators)... };

        const CharType *data = Base::Data();
        const SizeType size = Size();

        Array<String> tokens;

        String working_string;
        working_string.Reserve(size);

        if (!is_utf8 || m_length == size) {
            for (SizeType i = 0; i < size; i++) {
                const CharType ch = data[i];

                if (separator_values.Contains(ch)) {
                    tokens.PushBack(std::move(working_string));
                    // working_string now cleared
                    continue;
                }

                working_string.Append(ch);
            }
        } else {
            for (SizeType i = 0; i < size;) {
                int cp = 0;

                const utf::u32char char_32 = utf::char8to32(
                    data + i,
                    MathUtil::Min(sizeof(utf::u32char), size - i),
                    cp
                );

                i += cp;

                if (separator_values.Contains(char_32)) {
                    tokens.PushBack(std::move(working_string));

                    continue;
                }

                working_string.Append(char_32);
            }
        }

        // finalize by pushing back remaining string
        if (working_string.Any()) {
            tokens.PushBack(std::move(working_string));
        }

        return tokens;
    }

    template <class Container>
    static String Join(const Container &container, const String &separator)
    {
        String result;

        for (auto it = container.Begin(); it != container.End(); ++it) {
            result.Append(ToString(*it));

            if (it != container.End() - 1) {
                result.Append(separator);
            }
        }

        return result;
    }

    template <class Container>
    HYP_NODISCARD static String Join(const Container &container, WidestCharType separator)
    {
        String result;

        for (auto it = container.Begin(); it != container.End(); ++it) {
            result.Append(ToString(*it));

            if (it != container.End() - 1) {
                if constexpr (is_utf8 && std::is_same_v<utf::u32char, decltype(separator)>) {
                    int separator_length = 0;
                    utf::u8char separator_bytes[sizeof(utf::u32char) + 1] = { '\0' };
                    utf::char32to8(separator, separator_bytes, separator_length);

#ifdef HYP_DEBUG_MODE
                    AssertThrow(separator_length <= HYP_ARRAY_SIZE(separator_bytes));
#endif

                    for (int separator_byte_index = 0; separator_byte_index < separator_length; separator_byte_index++) {
                        result.Append(separator_bytes[separator_byte_index]);
                    }
                } else {
                    result.Append(separator);
                }
            }
        }

        return result;
    }

    HYP_NODISCARD static String Base64Encode(const Array<ubyte> &bytes)
    {
        static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        String out;

        uint i = 0;
        int j = -6;

        for (auto &&c : bytes) {
            i = (i << 8) + static_cast<ValueType>(c);
            j += 8;

            while (j >= 0) {
                out.Append(alphabet[(i >> j) & 0x3F]);
                j -= 6;
            }
        }

        if (j > -6) {
            out.Append(alphabet[((i << 8) >> (j + 8)) & 0x3F]);
        }

        while (out.Size() % 4 != 0) {
            out.Append('=');
        }

        return out;
    }

    HYP_NODISCARD static Array<ubyte> Base64Decode(const String &in)
    {
        static const int lookup_table[] = {
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

        uint i = 0;
        int j = -8;

        for (auto &&c : in) {
            if (lookup_table[c] == -1) {
                break;
            }

            i = (i << 6) + lookup_table[c];
            j += 6;

            if (j >= 0) {
                out.PushBack(ubyte((i >> j) & 0xFF));
                j -= 8;
            }
        }

        return out;
    }

    HYP_NODISCARD String<UTF8> ToUTF8() const
    {
        if constexpr (is_utf8) {
            return *this;
        } else if constexpr (is_ansi) {
            return String<UTF8>(Data());
        } else if constexpr (is_utf16) {
            uint32 len = utf::utf16_to_utf8(Data(), Data() + Size(), nullptr);

            if (len == 0) {
                return String<UTF8>::empty;
            }

            Array<utf::u8char> buffer;
            buffer.Resize(len + 1);

            utf::utf16_to_utf8(Data(), Data() + Size(), buffer.Data());

            return String<UTF8>(buffer.ToByteView());
        } else if constexpr (is_utf32) {
            uint32 len = utf::utf32_to_utf8(Data(), Data() + Size(), nullptr);

            if (len == 0) {
                return String<UTF8>::empty;
            }

            Array<utf::u8char> buffer;
            buffer.Resize(len + 1);

            utf::utf32_to_utf8(Data(), Data() + Size(), buffer.Data());

            return String<UTF8>(buffer.ToByteView());
        } else if constexpr (is_wide) {
            uint32 len = utf::wide_to_utf8(Data(), Data() + Size(), nullptr);

            if (len == 0) {
                return String<UTF8>::empty;
            }

            Array<utf::u8char> buffer;
            buffer.Resize(len + 1);

            utf::wide_to_utf8(Data(), Data() + Size(), buffer.Data());

            return String<UTF8>(buffer.ToByteView());
        } else {
            return String<UTF8>::empty;
        }
    }

    HYP_NODISCARD String<WIDE_CHAR> ToWide() const
    {
        if constexpr (is_wide) {
            return *this;
        } else if constexpr (is_utf8 || is_ansi) {
            uint32 len = utf::utf8_to_wide(Data(), Data() + Size(), nullptr);

            if (len == 0) {
                return String<WIDE_CHAR>::empty;
            }

            Array<wchar_t> buffer;
            buffer.Resize(len + 1);

            utf::utf8_to_wide(Data(), Data() + Size(), buffer.Data());

            return String<WIDE_CHAR>(buffer.ToByteView());
        } else if constexpr (is_utf16) {
            uint32 len = utf::utf16_to_wide(Data(), Data() + Size(), nullptr);

            if (len == 0) {
                return String<WIDE_CHAR>::empty;
            }

            Array<wchar_t> buffer;
            buffer.Resize(len + 1);

            utf::utf16_to_wide(Data(), Data() + Size(), buffer.Data());

            return String<WIDE_CHAR>(buffer.ToByteView());
        } else if constexpr (is_utf32) {
            uint32 len = utf::utf32_to_wide(Data(), Data() + Size(), nullptr);

            if (len == 0) {
                return String<WIDE_CHAR>::empty;
            }
            
            Array<wchar_t> buffer;
            buffer.Resize(len + 1);

            utf::utf32_to_wide(Data(), Data() + Size(), buffer.Data());

            return String<WIDE_CHAR>(buffer.ToByteView());
        } else {
            return String<WIDE_CHAR>::empty;
        }
    }

    template <class Integral>
    HYP_NODISCARD
    static typename std::enable_if_t<std::is_integral_v<NormalizedType<Integral>>, String>
    ToString(Integral value)
    {
        SizeType result_size;
        utf::utf_to_str<Integral, CharType>(value, result_size, nullptr);

        AssertThrow(result_size >= 1);

        String result;
        result.Reserve(result_size - 1);  // String class automatically adds 1 for null character

        Array<CharType> buffer;
        buffer.Resize(result_size);
        
        utf::utf_to_str<Integral, CharType>(value, result_size, buffer.Data());

        for (SizeType i = 0; i < result_size - 1; i++) {
            result.Append(buffer[i]);
        }

        return result;
    }
    
    HYP_NODISCARD HYP_FORCE_INLINE static String ToString(const String &value)
        { return value; }

    HYP_NODISCARD HYP_FORCE_INLINE  static String ToString(String &&value)
        { return value; }

    HYP_FORCE_INLINE HashCode GetHashCode() const
        { return HashCode(::hyperion::detail::FNV1::HashString(Data())); }

    HYP_DEF_STL_BEGIN_END(
        Base::Begin(),
        Base::End() - 1
    )

protected:
    SizeType m_length;
};

template <int StringType>
const String<StringType> String<StringType>::empty = String();

template <int StringType>
String<StringType>::String()
    : Base(),
      m_length(0)
{
    // null-terminated char
    Base::PushBack(CharType { 0 });
}

template <int StringType>
String<StringType>::String(const CharType *str)
    : Base(),
      m_length(0)
{
    if (str == nullptr) {
        return;
    }
    
    int cp;
    const int len = utf::utf_strlen<CharType, is_utf8>(str, cp);

    if (len == -1) {
        // invalid utf8 string
        // push back null terminated char
        Base::PushBack(CharType { 0 });

        return;
    }

    m_length = len;
    
    Base::Resize(cp + 1);

    CharType *data = Data();

    for (int i = 0; i < cp; ++i) {
        data[i] = str[i];
    }
}

template <int StringType>
String<StringType>::String(const CharType *_begin, const CharType *_end)
    : String()
{
    if (_begin >= _end) {
        return;
    }

    const SizeType cp = SizeType(_end - _begin);

    if constexpr (is_utf8) {
        const int len = utf::utf8_strlen(_begin, _end);

        if (len == -1) {
            // invalid utf8 string
            return;
        }

        m_length = len;
    } else {
        m_length = cp;
    }
    
    Base::Resize(cp + 1);

    CharType *data = Data();

    for (int i = 0; i < cp; ++i) {
        data[i] = _begin[i];
    }
}

template <int StringType>
String<StringType>::String(const String &other)
    : Base(other),
      m_length(other.m_length)
{
}

template <int StringType>
String<StringType>::String(String &&other) noexcept
    : Base(static_cast<Base &&>(std::move(other))),
      m_length(other.m_length)
{
    other.Clear();
}


template <int StringType>
String<StringType>::String(const CharArray<CharType> &char_array)
    : Base(),
      m_length(0)
{
    Base::Resize(char_array.Size());
    Memory::MemCpy(Data(), char_array.Data(), char_array.Size());
    
    // add null terminator char if it does not exist yet.
    if (char_array.Empty() || char_array.Back() != 0) {
        Base::PushBack(0);
    }

    m_length = utf::utf_strlen<CharType, is_utf8>(Base::Data());
}

template <int StringType>
String<StringType>::String(ConstByteView byte_view)
    : Base(),
      m_length(0)
{
    SizeType size = byte_view.Size();

    for (SizeType index = 0; index < size; ++index) {

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(byte_view.Data()[index] >= 0 && byte_view.Data()[index] <= 255, "Out of character range");
#endif

        if (byte_view.Data()[index] == 0x0) {
            size = index;

            break;
        }
    }

    Base::Resize((size / sizeof(CharType)) + 1); // +1 for null char
    Memory::MemCpy(Data(), byte_view.Data(), size / sizeof(CharType));

    m_length = utf::utf_strlen<CharType, is_utf8>(Base::Data());
}

template <int StringType>
String<StringType>::~String()
{
}

template <int StringType>
auto String<StringType>::operator=(const CharType *str) -> String&
{
    String<StringType>::operator=(String(str));

    return *this;
}

template <int StringType>
auto String<StringType>::operator=(const String &other) -> String&
{
    Base::operator=(other);
    m_length = other.m_length;

    return *this;
}

template <int StringType>
auto String<StringType>::operator=(String &&other) noexcept -> String&
{
    const auto len = other.m_length;
    Base::operator=(std::move(other));
    m_length = len;
    other.Clear();

    return *this;
}

template <int StringType>
auto String<StringType>::operator+(const String &other) const -> String
{
    String result(*this);
    result.Append(other);

    return result;
}

template <int StringType>
auto String<StringType>::operator+(CharType ch) const -> String
{
    String result(*this);
    result.Append(ch);

    return result;
}

template <int StringType>
auto String<StringType>::operator+(String &&other) const -> String
{
    String result(*this);
    result.Append(std::move(other));

    return result;
}

template <int StringType>
auto String<StringType>::operator+=(const String &other) -> String&
{
    Append(other);

    return *this;
}

template <int StringType>
auto String<StringType>::operator+=(String &&other) -> String&
{
    Append(std::move(other));

    return *this;
}

template <int StringType>
auto String<StringType>::operator+=(CharType ch) -> String&
{
    Append(ch);

    return *this;
}

template <int StringType>
bool String<StringType>::operator==(const String &other) const
{
    if (this == std::addressof(other)) {
        return true;
    }

    if (Size() != other.Size() || m_length != other.m_length) {
        return false;
    }

    if (Empty() && other.Empty()) {
        return true;
    }

    return utf::utf_strcmp<CharType, is_utf8>(Base::Data(), other.Data()) == 0;
}

template <int StringType>
bool String<StringType>::operator==(const CharType *str) const
{
    if (!str) {
        return *this == empty;
    }

    const auto len = utf::utf_strlen<CharType, is_utf8>(str);

    if (len == -1) {
        return false; // invalid utf string
    }

    if (m_length != static_cast<SizeType>(len)) {
        return false;
    }

    if (Empty() && len == 0) {
        return true;
    }

    return utf::utf_strcmp<CharType, is_utf8>(Base::Data(), str) == 0;
}

template <int StringType>
bool String<StringType>::operator!=(const String &other) const
{
    return !operator==(other);
}

template <int StringType>
bool String<StringType>::operator!=(const CharType *str) const
{
    return !operator==(str);
}

template <int StringType>
bool String<StringType>::operator<(const String &other) const
{
    return utf::utf_strcmp<CharType, is_utf8>(Base::Data(), other.Data()) < 0;
}

template <int StringType>
auto String<StringType>::operator[](SizeType index) const -> const CharType
{
    return Base::operator[](index);
}

template <int StringType>
auto String<StringType>::GetChar(SizeType index) const -> WidestCharType
{
    const SizeType size = Size();

#ifdef HYP_DEBUG_MODE
    AssertThrow(index < size);
#endif

    if constexpr (is_utf8) {
        return utf::utf8_charat(reinterpret_cast<const utf::u8char *>(Data()), size, index);
    } else {
        return Base::operator[](index);
    }
}

template <int StringType>
void String<StringType>::Append(const String &other)
{
    if (Size() + other.Size() + 1 >= Base::m_capacity) {
        if (Base::m_capacity >= Size() + other.Size() + 1) {
            Base::ResetOffsets();
        } else {
            Base::SetCapacity(Base::GetCapacity(Size() + other.Size() + 1));
        }
    }

    Base::PopBack(); // current NT char

    auto *buffer = Base::GetStorage();

    for (SizeType i = 0; i < other.Size(); i++) {
        // set item at index
        new (&buffer[Base::m_size++].data_buffer) CharType(other[i]);
    }

    Base::PushBack(CharType { 0 });

    m_length += other.m_length;
}

template <int StringType>
void String<StringType>::Append(String &&other)
{
    if (Size() + other.Size() + 1 >= Base::m_capacity) {
        if (Base::m_capacity >= Size() + other.Size() + 1) {
            Base::ResetOffsets();
        } else {
            Base::SetCapacity(Base::GetCapacity(Size() + other.Size() + 1));
        }
    }

    Base::PopBack(); // current NT char

    auto *buffer = Base::GetStorage();

    for (SizeType i = 0; i < other.Size(); i++) {
        // set item at index
        new (&buffer[Base::m_size++].data_buffer) CharType(std::move(other[i]));
    }

    Base::PushBack(CharType { 0 });

    m_length += other.m_length;

    other.Clear();
}

template <int StringType>
void String<StringType>::Append(const CharType *str)
{
    Append(String(str));
}

template <int StringType>
void String<StringType>::Append(const CharType *_begin, const CharType *_end)
{
    Append(String(_begin, _end));
}

template <int StringType>
void String<StringType>::Append(CharType value)
{
    if (Size() + 2 >= Base::m_capacity) {
        if (Base::m_capacity >= Size() + 2) {
            Base::ResetOffsets();
        } else {
            Base::SetCapacity(Base::GetCapacity(Size() + 2));
        }
    }

    Base::PopBack(); // current NT char
    Memory::MemCpy(
        &Base::GetStorage()[Base::m_size++].data_buffer[0],
        &value,
        sizeof(CharType)
    );
    Base::PushBack(CharType { 0 });

    ++m_length;
}

template <int StringType>
auto String<StringType>::PopFront() -> typename Base::ValueType
{
    --m_length;
    return Base::PopFront();
}

template <int StringType>
auto String<StringType>::PopBack() -> typename Base::ValueType
{
    --m_length;
    Base::PopBack(); // pop NT-char
    auto &&res = Base::PopBack();
    Base::PushBack(CharType { 0 }); // add NT-char
    return res;
}

template <int StringType>
void String<StringType>::Clear()
{
    Base::Clear();
    Base::PushBack(CharType { 0 }); // NT char
    m_length = 0;
}

template <int StringType>
bool String<StringType>::StartsWith(const String &other) const
{
    if (Size() < other.Size()) {
        return false;
    }
    
    return std::equal(Base::Begin(), Base::Begin() + other.Size(), other.Base::Begin());
}

template <int StringType>
bool String<StringType>::EndsWith(const String &other) const
{
    if (Size() < other.Size()) {
        return false;
    }
    
    return std::equal(Base::Begin() + Size() - other.Size(), Base::End(), other.Base::Begin());
}

template <int StringType>
auto String<StringType>::ToLower() const -> String
{
    String result;
    result.Reserve(Size());

    for (SizeType i = 0; i < Size();) {
        if constexpr (is_utf8) {
            int cp = 0;

            union
            {
                utf::u32char char_u32;
                int char_i32;
            };

            // evil union byte magic
            char_u32 = utf::char8to32(Data() + i, sizeof(utf::u32char), cp);
            char_i32 = std::tolower(char_i32);

            result.Append(char_u32);

            i += cp;
        } else {
            result.Append(std::tolower(result[i]));

            i++;
        }
    }

    return result;
}

template <int StringType>
auto String<StringType>::ToUpper() const -> String
{
    String result(*this);

    std::transform(result.Begin(), result.End(), result.Begin(), [](auto ch) {
        return std::toupper(ch);
    });

    return result;
}

template <int StringType>
auto String<StringType>::Trimmed() const -> String
{
    return TrimmedLeft().TrimmedRight();
}

template <int StringType>
auto String<StringType>::TrimmedLeft() const -> String
{
    String res;
    res.Reserve(Size());

    SizeType start_index;

    for (start_index = 0; start_index < Size(); ++start_index) {
        if (!std::isspace(Data()[start_index])) {
            break;
        }
    }

    for (SizeType index = start_index; index < Size(); ++index) {
        res.Append(Data()[index]);
    }

    return res;
}

template <int StringType>
auto String<StringType>::TrimmedRight() const -> String
{
    String res;
    res.Reserve(Size());

    SizeType start_index;

    for (start_index = Size(); start_index > 0; --start_index) {
        if (!std::isspace(Data()[start_index - 1])) {
            break;
        }
    }

    for (SizeType index = 0; index < start_index; ++index) {
        res.Append(Data()[index]);
    }

    return res;
}
template <int StringType>
String<StringType> String<StringType>::ReplaceAll(const String &search, const String &replace) const
{
    String tmp(*this);

    String result;
    result.Reserve(Size());

    SizeType index = 0;

    while (index < Length()) {
        auto found_index = tmp.FindIndex(search);

        if (found_index == not_found) {
            result.Append(tmp);
            break;
        }

        result.Append(tmp.Substr(0, found_index));
        result.Append(replace);

        tmp = tmp.Substr(found_index + search.Length());
        index += found_index + search.Length();
    }

    return result;
}

template <int StringType>
String<StringType> String<StringType>::Escape() const
{
    const SizeType size = Size();
    const auto *data = Base::Data();

    String result;
    result.Reserve(size);

    if (!is_utf8 || m_length == size) {
        for (SizeType i = 0; i < size; i++) {
            auto ch = data[i];

            switch (ch) {
            case '\n':
                result.Append("\\n");
                break;
            case '\r':
                result.Append("\\r");
                break;
            case '\t':
                result.Append("\\t");
                break;
            case '\v':
                result.Append("\\v");
                break;
            case '\b':
                result.Append("\\b");
                break;
            case '\f':
                result.Append("\\f");
                break;
            case '\a':
                result.Append("\\a");
                break;
            case '\\':
                result.Append("\\\\");
                break;
            case '\"':
                result.Append("\\\"");
                break;
            case '\'':
                result.Append("\\\'");
                break;
            default:
                result.Append(ch);
                break;
            }
        }
    } else {
        for (SizeType i = 0; i < size;) {
            int cp = 0;

            union { uint32 char_u32; uint8 char_u8[sizeof(utf::u32char)]; };
            
            char_u32 = utf::char8to32(
                Data() + i,
                MathUtil::Min(sizeof(utf::u32char), size - i),
                cp
            );

            i += cp;

            switch (char_u32) {
            case uint32('\n'):
                result.Append("\\n");
                break;
            case uint32('\r'):
                result.Append("\\r");
                break;
            case uint32('\t'):
                result.Append("\\t");
                break;
            case uint32('\v'):
                result.Append("\\v");
                break;
            case uint32('\b'):
                result.Append("\\b");
                break;
            case uint32('\f'):
                result.Append("\\f");
                break;
            case uint32('\a'):
                result.Append("\\a");
                break;
            case uint32('\\'):
                result.Append("\\\\");
                break;
            case uint32('\"'):
                result.Append("\\\"");
                break;
            case uint32('\''):
                result.Append("\\\'");
                break;
            default:
                result.Append(char_u8[0]);

                if (cp >= 2) {
                    result.Append(char_u8[1]);
                }
                
                if (cp >= 3) {
                    result.Append(char_u8[2]);
                }

                if (cp == 4) {
                    result.Append(char_u8[3]);
                }

                break;
            }
        }
    }

    return result;
}


template <int StringType>
auto String<StringType>::Substr(SizeType first, SizeType last) const -> String
{
    if (first == SizeType(-1)) {
        return *this;
    }

    last = MathUtil::Max(last, first);

    const auto size = Size();

    if constexpr (is_utf8) {
        String result;

        SizeType char_index = 0;

        for (SizeType i = 0; i < size;) {
            auto c = Base::operator[](i);

            if (char_index >= last) {
                break;
            }

            if (char_index >= first) {
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

            ++char_index;
        }

        return result;
    } else {
        if (first >= size) {
            return String::empty;
        }

        String result;
        result.Reserve(MathUtil::Min(size, last) - first);

        for (SizeType i = first; i < size; i++) {
            auto c = Base::operator[](i);

            if (i >= last) {
                break;
            }

            result.Append(c);
        }

        return result;
    }
}

#if 0
template <int StringType>
String<StringType> operator+(const CharType *str, const String<StringType> &other)
{
    return String<StringType>(str) + other;
}
#endif

template <int string_type>
constexpr bool operator<(const String<string_type> &lhs, const utilities::detail::StringView<string_type> &rhs)
{
    if (!lhs.Data()) {
        return true;
    }

    if (!rhs.Data()) {
        return false;
    }

    return utf::utf_strcmp<typename utilities::detail::StringView<string_type>::CharType, utilities::detail::StringView<string_type>::is_utf8>(lhs.Data(), rhs.Data()) < 0;
}

template <int string_type>
constexpr bool operator<(const utilities::detail::StringView<string_type> &lhs, const String<string_type> &rhs)
{
    if (!lhs.Data()) {
        return true;
    }

    if (!rhs.Data()) {
        return false;
    }

    return utf::utf_strcmp<typename utilities::detail::StringView<string_type>::CharType, utilities::detail::StringView<string_type>::is_utf8>(lhs.Data(), rhs.Data()) < 0;
}

template <int string_type>
constexpr bool operator==(const String<string_type> &lhs, const utilities::detail::StringView<string_type> &rhs)
{
    if (lhs.Data() == rhs.Data() && (!lhs.Data() || lhs.Size() == rhs.Size())) {
        return true;
    }

    return Memory::AreStaticStringsEqual(lhs.Data(), rhs.Data(), MathUtil::Min(lhs.Size(), rhs.Size()));
}

template <int string_type>
constexpr bool operator==(const utilities::detail::StringView<string_type> &lhs, const String<string_type> &rhs)
{
    if (lhs.Data() == rhs.Data() && (!lhs.Data() || lhs.Size() == rhs.Size())) {
        return true;
    }

    return Memory::AreStaticStringsEqual(lhs.Data(), rhs.Data(), MathUtil::Min(lhs.Size(), rhs.Size()));
}

} // namespace detail
} // namespace containers

using CharArray = containers::detail::CharArray<char>;

inline String operator+(const char *str, const String &other)
    { return String(str) + other; }

inline ANSIString operator+(const char *str, const ANSIString &other)
    { return ANSIString(str) + other; }

inline UTF16String operator+(const utf::u16char *str, const UTF16String &other)
    { return UTF16String(str) + other; }

inline UTF32String operator+(const utf::u32char *str, const UTF32String &other)
    { return UTF32String(str) + other; }

inline WideString operator+(const wchar_t *str, const WideString &other)
    { return WideString(str) + other; }

template <int StringType, typename = std::enable_if_t<std::is_same_v<typename containers::detail::String<StringType>::CharType, char>, int>>
std::ostream &operator<<(std::ostream &os, const containers::detail::String<StringType> &str)
{
    os << str.Data();

    return os;
}

template <int StringType, typename = std::enable_if_t<std::is_same_v<typename containers::detail::String<StringType>::CharType, wchar_t>, int>>
std::wostream &operator<<(std::wostream &os, const containers::detail::String<StringType> &str)
{
    os << str.Data();

    return os;
}

} // namespace hyperion

HYP_DEF_STL_HASH(hyperion::String);
HYP_DEF_STL_HASH(hyperion::ANSIString);
HYP_DEF_STL_HASH(hyperion::WideString);
HYP_DEF_STL_HASH(hyperion::UTF16String);
HYP_DEF_STL_HASH(hyperion::UTF32String);

#endif