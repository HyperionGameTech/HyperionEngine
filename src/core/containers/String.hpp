/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STRING_HPP
#define HYPERION_STRING_HPP

#include <math/MathUtil.hpp>
#include <util/UTF8.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/memory/ByteBuffer.hpp>
#include <core/memory/Memory.hpp>
#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>
#include <core/Defines.hpp>

#include <algorithm>
#include <utility>
#include <type_traits>

namespace hyperion {
namespace containers {

enum StringType : int
{
    NONE    = 0,
    ANSI    = 1,
    UTF8    = 2,
    UTF16   = 3,
    UTF32   = 4,
    WIDE_CHAR    = 5
};

namespace detail {

/*! \brief Dynamic string class that natively supports UTF-8, as well as UTF-16, UTF-32, wide chars and ANSI. */
using namespace ::utf;

template <class CharType>
using CharArray = Array<CharType, 64u>;

template <int string_type>
struct StringTypeImpl { };

template <>
struct StringTypeImpl<ANSI>
{
    using CharType = char;
    using WidestCharType = char;
};

template <>
struct StringTypeImpl<UTF8>
{
    using CharType = char;
    using WidestCharType = utf::u32char;
};

template <>
struct StringTypeImpl<UTF16>
{
    using CharType = utf::u16char;
    using WidestCharType = utf::u16char;
};

template <>
struct StringTypeImpl<UTF32>
{
    using CharType = utf::u32char;
    using WidestCharType = utf::u32char;
};

template <>
struct StringTypeImpl<WIDE_CHAR>
{
    using CharType = wchar_t;
    using WidestCharType = wchar_t;
};


template <int string_type>
class String : Array<typename StringTypeImpl<string_type>::CharType, 64u>
{
public:
    using CharType = typename StringTypeImpl<string_type>::CharType;
    using WidestCharType = typename StringTypeImpl<string_type>::WidestCharType;

protected:
    using Base = Array<CharType, 64u>;

public:
    using ValueType = typename Base::ValueType;
    using KeyType = typename Base::KeyType;

    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    static const String empty;

    static constexpr bool is_ansi = string_type == ANSI;
    static constexpr bool is_utf8 = string_type == UTF8;
    static constexpr bool is_utf16 = string_type == UTF16;
    static constexpr bool is_utf32 = string_type == UTF32;
    static constexpr bool is_wide = string_type == WIDE_CHAR;

    static_assert(!is_utf8 || (std::is_same_v<CharType, char> || std::is_same_v<CharType, unsigned char>), "UTF-8 Strings must have CharType equal to char or unsigned char");
    static_assert(!is_ansi || (std::is_same_v<CharType, char> || std::is_same_v<CharType, unsigned char>), "ANSI Strings must have CharType equal to char or unsigned char");
    static_assert(!is_utf16 || std::is_same_v<CharType, utf::u16char>, "UTF-16 Strings must have CharType equal to utf::u16char");
    static_assert(!is_utf32 || std::is_same_v<CharType, utf::u32char>, "UTF-32 Strings must have CharType equal to utf::u32char");
    static_assert(!is_wide || std::is_same_v<CharType, wchar_t>, "Wide Strings must have CharType equal to wchar_t");

    static constexpr SizeType not_found = SizeType(-1);

    static_assert(!is_utf8 || std::is_same_v<CharType, char>, "UTF-8 Strings must have CharType equal to char");

    String();
    String(const String &other);
    String(const CharType *str);
    String(const CharType *str, int max_len);
    explicit String(const CharArray<CharType> &char_array);
    explicit String(const ByteBuffer &byte_buffer);

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
    
    template <int other_string_type, std::enable_if_t<other_string_type != string_type, int> = 0>
    String &operator=(const String<other_string_type> &other)
    {
        Clear();
        Reserve(other.Size());
        for (SizeType i = 0; i < other.Length(); i++) {
            Append(other.GetChar(i));
        }

        return *this;
    }

    String operator+(const String &other) const;
    String operator+(String &&other) const;
    String operator+(CharType ch) const;
    
    template <class U32Char, typename = std::enable_if_t<is_utf8 && std::is_same_v<U32Char, u32char>, int>>
    String operator+(U32Char ch) const
        { return String(*this) += ch; }
    
    String &operator+=(const String &other);
    String &operator+=(String &&other);
    String &operator+=(CharType ch);
    
    template <class U32Char, typename = std::enable_if_t<is_utf8 && std::is_same_v<U32Char, u32char>, int>>
    String &operator+=(U32Char ch)
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
    [[nodiscard]] const CharType operator[](SizeType index) const;

    /*! \brief Get a char from the String at the given index.
     * For UTF-8 strings, the character is encoded as a 32-bit value.
     * \note If needing to access raw character data, \ref{operator[]} should be used instead.
     *
     * \ref{index} must be less than \ref{Length()}.
     */
    [[nodiscard]] WidestCharType GetChar(SizeType index) const;

    /*! \brief Return the data size in characters. Note, UTF-8 strings can have a shorter length than size. */
    [[nodiscard]] SizeType Size() const { return Base::Size() - 1; /* for NT char */ }

    /*! \brief Return the length of the string in characters. Note, UTF-8 strings can have a shorter length than size. */
    [[nodiscard]] SizeType Length() const
        { return m_length; }

    /*! \brief Access the raw data of the string.
        \note For UTF-8 strings, ensure proper care is taken when accessing the data, as indexing via a character index may not
              yield a valid character.
    */
    [[nodiscard]] typename Base::ValueType *Data()
        { return Base::Data(); }

    [[nodiscard]] const typename Base::ValueType *Data() const
        { return Base::Data(); }

    [[nodiscard]] typename Base::ValueType &Front()
        { return Base::Front(); }

    [[nodiscard]] const typename Base::ValueType &Front() const
        { return Base::Front(); }

    [[nodiscard]] typename Base::ValueType &Back()
        { return Base::GetBuffer()[Base::m_size - 2]; /* for NT char */ }

    [[nodiscard]] const typename Base::ValueType &Back() const
        { return Base::GetBuffer()[Base::m_size - 2]; /* for NT char */ }

    /*! \brief Check if the string contains the given character. */
    [[nodiscard]] bool Contains(const CharType &ch) const
        { return ch != CharType { 0 } && Base::Contains(ch); }

    /*! \brief Check if the string contains the given string. */
    [[nodiscard]] bool Contains(const String &str) const
        { return FindIndex(str) != not_found; }

    /*! \brief Find the index of the first occurrence of the character in the string.
     * \note For UTF-8 strings, ensure accessing the character with the returned value is done via the \ref{GetChar} method,
     *       as the index is the character index, not the byte index. */
    [[nodiscard]] SizeType FindIndex(const String &str) const;

    /*! \brief Check if the string is empty. */
    [[nodiscard]] bool Empty() const
        { return Size() == 0; }

    /*! \brief Check if the string contains any characters. */
    [[nodiscard]] bool Any() const
        { return Size() != 0; }

    /*! \brief Check if the string contains multi-byte characters. */
    [[nodiscard]] bool HasMultiByteChars() const { return Size() > Length(); }

    /*! \brief Reserve space for the string. \ref{capacity} + 1 is used, to make space for the null character. */
    void Reserve(SizeType capacity)
        { Base::Reserve(capacity + 1); }

    void Refit() { Base::Refit(); }

    void Append(const String &other);
    void Append(String &&other);
    void Append(const CharType *str);
    void Append(CharType value);
    
    template <class LargeCharType, typename = std::enable_if_t<is_utf8 && !std::is_same_v<LargeCharType, CharType> && (std::is_same_v<LargeCharType, u32char> || std::is_same_v<LargeCharType, u16char> || std::is_same_v<LargeCharType, wchar_t>), int>>
    void Append(LargeCharType ch)
    {
        char parts[4] = { '\0' };
        utf::char32to8(ch, parts);

        Append(parts);
    }

    typename Base::ValueType PopBack();
    typename Base::ValueType PopFront();
    void Clear();

    bool StartsWith(const String &other) const;
    bool EndsWith(const String &other) const;

    String ToLower() const;
    String ToUpper() const;
    
    String Trimmed() const;
    String TrimmedLeft() const;
    String TrimmedRight() const;

    String Substr(SizeType first, SizeType last = MathUtil::MaxSafeValue<SizeType>()) const;

    String Escape() const;

    String ReplaceAll(const String &search, const String &replace) const;

    template <class ... SeparatorType>
    Array<String> Split(SeparatorType ... separators) const 
    {
        hyperion::FixedArray<WidestCharType, sizeof...(separators)> separator_values { WidestCharType(separators)... };

        const auto *data = Base::Data();
        const auto size = Size();

        Array<String> tokens;

        String working_string;
        working_string.Reserve(size);

        if (!is_utf8 || m_length == size) {
            for (SizeType i = 0; i < size; i++) {
                const auto ch = data[i];

                if (separator_values.Contains(ch)) {
                    tokens.PushBack(std::move(working_string));
                    // working_string now cleared
                    continue;
                }

                working_string.Append(ch);
            }
        } else {
            for (SizeType i = 0; i < size;) {
                uint8 num_bytes_read = 0;

                union { uint32 char_u32; uint8 char_u8[sizeof(utf::u32char)]; };
                char_u32 = 0;

                char_u32 = utf::char8to32(
                    Data() + i,
                    MathUtil::Min(sizeof(utf::u32char), size - i),
                    &num_bytes_read
                );

                i += num_bytes_read;

                if (separator_values.Contains(char_u32)) {
                    tokens.PushBack(std::move(working_string));

                    continue;
                }
                
                working_string.Append(char_u8[0]);

                if (num_bytes_read >= 2) {
                    working_string.Append(char_u8[1]);
                }
                
                if (num_bytes_read >= 3) {
                    working_string.Append(char_u8[2]);
                }

                if (num_bytes_read == 4) {
                    working_string.Append(char_u8[3]);
                }
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
    static String Join(const Container &container, WidestCharType separator)
    {
        String result;

        for (auto it = container.Begin(); it != container.End(); ++it) {
            result.Append(ToString(*it));

            if (it != container.End() - 1) {
                if constexpr (is_utf8 && std::is_same_v<utf::u32char, decltype(separator)>) {
                    int separator_length = 0;
                    char separator_bytes[sizeof(utf::u32char)] = { '\0' };

                    utf::char32to8(separator, separator_bytes, &separator_length);

#ifdef HYP_DEBUG_MODE
                    AssertThrow(separator_length <= sizeof(separator_bytes) / sizeof(separator_bytes[0]));
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

    static String Base64Encode(const Array<ubyte> &bytes)
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

    static Array<ubyte> Base64Decode(const String &in)
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
                out.PushBack(static_cast<ubyte>((i >> j) & 0xFF));
                j -= 8;
            }
        }

        return out;
    }

    String<UTF8> ToUTF8() const
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

            utf::u8char *buffer = new utf::u8char[len + 1];
            utf::utf16_to_utf8(Data(), Data() + Size(), buffer);

            String<UTF8> result(reinterpret_cast<const char *>(buffer));

            delete[] buffer;

            return result;
        } else if constexpr (is_utf32) {
            uint32 len = utf::utf32_to_utf8(Data(), Data() + Size(), nullptr);

            if (len == 0) {
                return String<UTF8>::empty;
            }

            utf::u8char *buffer = new utf::u8char[len + 1];
            utf::utf32_to_utf8(Data(), Data() + Size(), buffer);

            String<UTF8> result(reinterpret_cast<const char *>(buffer));

            delete[] buffer;

            return result;
        } else if constexpr (is_wide) {
            uint32 len = utf::wide_to_utf8(Data(), Data() + Size(), nullptr);

            if (len == 0) {
                return String<UTF8>::empty;
            }

            utf::u8char *buffer = new utf::u8char[len + 1];
            utf::wide_to_utf8(Data(), Data() + Size(), buffer);

            String<UTF8> result(reinterpret_cast<const char *>(buffer));

            delete[] buffer;

            return result;
        } else {
            return String<UTF8>();
        }
    }

    template <class Integral>
    static typename std::enable_if_t<std::is_integral_v<NormalizedType<Integral>>, String>
    ToString(Integral value)
    {
        SizeType result_size;
        utf::utf_to_str<Integral, CharType>(value, result_size, nullptr);

        AssertThrow(result_size >= 1);

        String result;
        result.Reserve(result_size - 1);  // String class automatically adds 1 for null character

        CharType *data = new CharType[result_size];
        utf::utf_to_str<Integral, CharType>(value, result_size, data);

        for (SizeType i = 0; i < result_size - 1; i++) {
            result.Append(data[i]);
        }

        delete[] data;

        return result;
    }
    
    static String ToString(const String &value)
        { return value; }

    static String ToString(String &&value)
        { return value; }

    [[nodiscard]] HashCode GetHashCode() const
        { return HashCode::GetHashCode(Data()); }

protected:
    const CharType *StrStr(const String &other) const;

    SizeType m_length;
};

template <int string_type>
const String<string_type> String<string_type>::empty = String();

template <int string_type>
String<string_type>::String()
    : Base(),
      m_length(0)
{
    // null-terminated char
    Base::PushBack(CharType { 0 });
}

template <int string_type>
String<string_type>::String(const CharType *str)
    : Base(),
      m_length(0)
{
    if (str == nullptr) {
        return;
    }
    
    int count;
    const int len = utf::utf_strlen<CharType, is_utf8>(str, &count);

    if (len == -1) {
        // invalid utf8 string
        // push back null terminated char
        Base::PushBack(CharType { 0 });

        return;
    }

    m_length = len;
    
    // reserves + 1 for null char
    Reserve(count);

    for (int i = 0; i < count; ++i) {
        Base::PushBack(str[i]);
    }
    
    // null-terminated char
    Base::PushBack(CharType { 0 });
}

template <int string_type>
String<string_type>::String(const CharType *str, int max_len)
    : Base(),
      m_length(0)
{
    if (str == nullptr) {
        return;
    }

    int count;
    const int len = MathUtil::Min(utf::utf_strlen<CharType, is_utf8>(str, &count), max_len);

    if (len == -1) {
        // invalid utf8 string
        // push back null terminated char
        Base::PushBack(CharType { 0 });

        return;
    }

    m_length = len;
    
    // reserves + 1 for null char
    Reserve(count);

    for (int i = 0; i < count; ++i) {
        Base::PushBack(str[i]);
    }
    
    // null-terminated char
    Base::PushBack(CharType { 0 });
}

template <int string_type>
String<string_type>::String(const String &other)
    : Base(other),
      m_length(other.m_length)
{
}

template <int string_type>
String<string_type>::String(String &&other) noexcept
    : Base(static_cast<Base &&>(std::move(other))),
      m_length(other.m_length)
{
    other.Clear();
}


template <int string_type>
String<string_type>::String(const CharArray<CharType> &char_array)
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

template <int string_type>
String<string_type>::String(const ByteBuffer &byte_buffer)
    : Base(),
      m_length(0)
{
    SizeType size = byte_buffer.Size();

    for (SizeType index = 0; index < size; ++index) {

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(byte_buffer.Data()[index] >= 0 && byte_buffer.Data()[index] <= 255, "Out of character range");
#endif

        if (byte_buffer.Data()[index] == 0x00) {
            size = index;

            break;
        }
    }

    Base::Resize((size / sizeof(CharType)) + 1); // +1 for null char
    Memory::MemCpy(Data(), byte_buffer.Data(), size / sizeof(CharType));

    m_length = utf::utf_strlen<CharType, is_utf8>(Base::Data());
}

template <int string_type>
String<string_type>::~String()
{
}

template <int string_type>
auto String<string_type>::operator=(const CharType *str) -> String&
{
    String<string_type>::operator=(String(str));

    return *this;
}

template <int string_type>
auto String<string_type>::operator=(const String &other) -> String&
{
    Base::operator=(other);
    m_length = other.m_length;

    return *this;
}

template <int string_type>
auto String<string_type>::operator=(String &&other) noexcept -> String&
{
    const auto len = other.m_length;
    Base::operator=(std::move(other));
    m_length = len;
    other.Clear();

    return *this;
}

template <int string_type>
auto String<string_type>::operator+(const String &other) const -> String
{
    String result(*this);
    result.Append(other);

    return result;
}

template <int string_type>
auto String<string_type>::operator+(CharType ch) const -> String
{
    String result(*this);
    result.Append(ch);

    return result;
}

template <int string_type>
auto String<string_type>::operator+(String &&other) const -> String
{
    String result(*this);
    result.Append(std::move(other));

    return result;
}

template <int string_type>
auto String<string_type>::operator+=(const String &other) -> String&
{
    Append(other);

    return *this;
}

template <int string_type>
auto String<string_type>::operator+=(String &&other) -> String&
{
    Append(std::move(other));

    return *this;
}

template <int string_type>
auto String<string_type>::operator+=(CharType ch) -> String&
{
    Append(ch);

    return *this;
}

template <int string_type>
bool String<string_type>::operator==(const String &other) const
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

template <int string_type>
bool String<string_type>::operator==(const CharType *str) const
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

template <int string_type>
bool String<string_type>::operator!=(const String &other) const
{
    return !operator==(other);
}

template <int string_type>
bool String<string_type>::operator!=(const CharType *str) const
{
    return !operator==(str);
}

template <int string_type>
bool String<string_type>::operator<(const String &other) const
{
    return utf::utf_strcmp<CharType, is_utf8>(Base::Data(), other.Data()) < 0;
}

template <int string_type>
auto String<string_type>::operator[](SizeType index) const -> const CharType
{
    return Base::operator[](index);
}

template <int string_type>
auto String<string_type>::GetChar(SizeType index) const -> WidestCharType
{
    if constexpr (is_utf8) {
        const SizeType size = Size();

        AssertThrow(index < size);

        return utf::utf8_charat(Data(), size, index);
    } else {
        return Base::operator[](index);
    }
}

template <int string_type>
void String<string_type>::Append(const String &other)
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

template <int string_type>
void String<string_type>::Append(String &&other)
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

template <int string_type>
void String<string_type>::Append(const CharType *str)
{
    Append(String(str));
}

template <int string_type>
void String<string_type>::Append(CharType value)
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

template <int string_type>
auto String<string_type>::PopFront() -> typename Base::ValueType
{
    --m_length;
    return Base::PopFront();
}

template <int string_type>
auto String<string_type>::PopBack() -> typename Base::ValueType
{
    --m_length;
    Base::PopBack(); // pop NT-char
    auto &&res = Base::PopBack();
    Base::PushBack(CharType { 0 }); // add NT-char
    return res;
}

template <int string_type>
void String<string_type>::Clear()
{
    Base::Clear();
    Base::PushBack(CharType { 0 }); // NT char
    m_length = 0;
}

template <int string_type>
bool String<string_type>::StartsWith(const String &other) const
{
    if (Size() < other.Size()) {
        return false;
    }
    
    return std::equal(Base::Begin(), Base::Begin() + other.Size(), other.Base::Begin());
}

template <int string_type>
bool String<string_type>::EndsWith(const String &other) const
{
    if (Size() < other.Size()) {
        return false;
    }
    
    return std::equal(Base::Begin() + Size() - other.Size(), Base::End(), other.Base::Begin());
}

template <int string_type>
auto String<string_type>::ToLower() const -> String
{
    String result;
    result.Reserve(Size());

    for (SizeType i = 0; i < Size();) {
        if constexpr (is_utf8) {
            uint8 num_bytes_read = 0;

            union
            {
                utf::u32char char_u32;
                int char_i32;
            };

            // evil union byte magic
            char_u32 = utf::char8to32(Data() + i, sizeof(utf::u32char), &num_bytes_read);
            char_i32 = std::tolower(char_i32);

            result.Append(char_u32);

            i += num_bytes_read;
        } else {
            result.Append(std::tolower(result[i]));

            i++;
        }
    }

    return result;
}

template <int string_type>
auto String<string_type>::ToUpper() const -> String
{
    String result(*this);

    std::transform(result.Begin(), result.End(), result.Begin(), [](auto ch) {
        return std::toupper(ch);
    });

    return result;
}

template <int string_type>
auto String<string_type>::Trimmed() const -> String
{
    return TrimmedLeft().TrimmedRight();
}

template <int string_type>
auto String<string_type>::TrimmedLeft() const -> String
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

template <int string_type>
auto String<string_type>::TrimmedRight() const -> String
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
template <int string_type>
String<string_type> String<string_type>::ReplaceAll(const String &search, const String &replace) const
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

template <int string_type>
String<string_type> String<string_type>::Escape() const
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
            uint8 num_bytes_read = 0;

            union { uint32 char_u32; uint8 char_u8[sizeof(utf::u32char)]; };
            char_u32 = 0;

            char_u32 = utf::char8to32(
                Data() + i,
                MathUtil::Min(sizeof(utf::u32char), size - i),
                &num_bytes_read
            );

            i += num_bytes_read;

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

                if (num_bytes_read >= 2) {
                    result.Append(char_u8[1]);
                }
                
                if (num_bytes_read >= 3) {
                    result.Append(char_u8[2]);
                }

                if (num_bytes_read == 4) {
                    result.Append(char_u8[3]);
                }

                break;
            }
        }
    }

    return result;
}


template <int string_type>
auto String<string_type>::Substr(SizeType first, SizeType last) const -> String
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

template <int string_type>
auto String<string_type>::StrStr(const String &other) const -> const CharType*
{
    if (Size() < other.Size()) {
        return nullptr;
    }

    const CharType *other_str = other.Data();

    for (const CharType *str = Data(); *str != 0; ++str) {
        if (*str != *other_str) {
            continue;
        }

        const CharType *this_str = str;

        for (;;) {
            if (*other_str == '\0') {
                return str;
            }

            if (*this_str++ != *other_str++) {
                break;
            }
        }

        other_str = other.Data();
    }

    return nullptr;
}

template <int string_type>
SizeType String<string_type>::FindIndex(const String &other) const
{
    if (auto *ptr = StrStr(other)) {
        if constexpr (is_utf8) {
            int *count;
            const int len = utf8_strlen(Data(), ptr, count);

            return SizeType(len);
        } else {
            return static_cast<SizeType>(ptr - Data());
        }
    }

    return not_found;
}

#if 0
template <int string_type>
String<string_type> operator+(const CharType *str, const String<string_type> &other)
{
    return String<string_type>(str) + other;
}
#endif

} // namespace detail
} // namespace containers

using StringType = containers::StringType;

using CharArray = containers::detail::CharArray<char>;

using String = containers::detail::String<StringType::UTF8>;
using ANSIString = containers::detail::String<StringType::ANSI>;
using WideString = containers::detail::String<StringType::WIDE_CHAR>;
using UTF32String = containers::detail::String<StringType::UTF32>;
using UTF16String = containers::detail::String<StringType::UTF16>;
using PlatformString = containers::detail::String<std::is_same_v<TChar, wchar_t> ? StringType::WIDE_CHAR : StringType::UTF8>;

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

template <int string_type, typename = std::enable_if_t<std::is_same_v<typename containers::detail::String<string_type>::CharType, char>, int>>
std::ostream &operator<<(std::ostream &os, const containers::detail::String<string_type> &str)
{
    os << str.Data();

    return os;
}

template <int string_type, typename = std::enable_if_t<std::is_same_v<typename containers::detail::String<string_type>::CharType, wchar_t>, int>>
std::wostream &operator<<(std::wostream &os, const containers::detail::String<string_type> &str)
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