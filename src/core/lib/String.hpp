#ifndef HYPERION_V2_LIB_STRING_H
#define HYPERION_V2_LIB_STRING_H

#include <math/MathUtil.hpp>
#include <util/UTF8.hpp>

#include "DynArray.hpp"
#include "FixedArray.hpp"
#include "ByteBuffer.hpp"
#include "CMemory.hpp"
#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>
#include <util/Defines.hpp>

#include <algorithm>
#include <utility>
#include <type_traits>

namespace hyperion {
namespace containers {
namespace detail {
/*! UTF-8 supporting dynamic string class */
using namespace ::utf;

enum StringType
{
    STRING_TYPE_NONE,
    STRING_TYPE_ANSI,
    STRING_TYPE_UTF8,
    STRING_TYPE_UTF16,
    STRING_TYPE_UTF32,
    STRING_TYPE_WIDE
};

template <class T>
using CharArray = Array<T, 64u>;

template <class T, bool IsUtf8>
class DynString : Array<T, 64u>
{
protected:
    using Base = Array<T, 64u>;

public:
    using ValueType = typename Base::ValueType;
    using KeyType = typename Base::KeyType;

    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    static const DynString empty;

    static constexpr bool is_utf8   = IsUtf8;
    static constexpr bool is_ansi   = !is_utf8 && (std::is_same_v<T, char> || std::is_same_v<T, unsigned char>);
    static constexpr bool is_utf16  = !is_utf8 && std::is_same_v<T, utf::u16char>;
    static constexpr bool is_utf32  = !is_utf8 && std::is_same_v<T, utf::u32char>;
    static constexpr bool is_wide   = !is_utf8 && std::is_same_v<T, wchar_t>;

    static constexpr StringType string_type =
        (is_ansi ? STRING_TYPE_ANSI :
        (is_utf8 ? STRING_TYPE_UTF8 :
        (is_utf16 ? STRING_TYPE_UTF16 :
        (is_utf32 ? STRING_TYPE_UTF32 :
        (is_wide ? STRING_TYPE_WIDE : STRING_TYPE_NONE)))));

    static constexpr SizeType not_found = SizeType(-1);
    
    using CharType          = T;
    using WidestCharType    = std::conditional_t<is_utf8, utf::u32char, T>;

    DynString();
    DynString(const DynString &other);
    DynString(const T *str);
    DynString(const T *str, Int max_len);
    explicit DynString(const CharArray<T> &char_array);
    explicit DynString(const ByteBuffer &byte_buffer);

    template <bool OtherIsUtf8>
    explicit DynString(const DynString<T, OtherIsUtf8> &other)
        : DynString(other.Data())
    {
    }
    
    DynString(DynString &&other) noexcept;
    ~DynString();

    DynString &operator=(const T *str);
    DynString &operator=(const DynString &other);
    DynString &operator=(DynString &&other) noexcept;

    DynString operator+(const DynString &other) const;
    DynString operator+(DynString &&other) const;
    DynString operator+(T ch) const;
    
    template <class U32Char, typename = std::enable_if_t<is_utf8 && std::is_same_v<U32Char, u32char>, int>>
    DynString operator+(U32Char ch) const
        { return DynString(*this) += ch; }
    
    DynString &operator+=(const DynString &other);
    DynString &operator+=(DynString &&other);
    DynString &operator+=(T ch);
    
    template <class U32Char, typename = std::enable_if_t<is_utf8 && std::is_same_v<U32Char, u32char>, int>>
    DynString &operator+=(U32Char ch)
        { Append(ch); return *this; }

    bool operator==(const DynString &other) const;
    bool operator==(const T *str) const;
    bool operator!=(const DynString &other) const;
    bool operator!=(const T *str) const;

    bool operator<(const DynString &other) const;

    /*! \brief Raw access of the character data of the string at index.
     * For UTF-8 Strings, the character may not be a valid UTF-8 character,
     * so for appropriate cases, \ref{GetChar} should be used instead.
     *
     * \ref{index} must be less than \ref{Size()}.
     */
    [[nodiscard]] const T operator[](SizeType index) const;

    /*! \brief Get a char from the String at the given index.
     * For UTF-8 strings, a UTF-32 decoded character is returned.
     * If needing to access raw character data, \ref{operator[]} should be used instead.
     *
     * \ref{index} must be less than \ref{Length()}.
     */
    [[nodiscard]] std::conditional_t<IsUtf8, u32char, T> GetChar(SizeType index) const;

    /*! \brief Return the data size in characters. Note, utf-8 strings can have a shorter length than size. */
    [[nodiscard]] SizeType Size() const { return Base::Size() - 1; /* for NT char */ }
    /*! \brief Return the length of the string in characters. Note, utf-8 strings can have a shorter length than size. */
    [[nodiscard]] SizeType Length() const { return m_length; }

    [[nodiscard]] typename Base::ValueType *Data() { return Base::Data(); }
    [[nodiscard]] const typename Base::ValueType *Data() const { return Base::Data(); }

    [[nodiscard]] typename Base::ValueType &Front() { return Base::Front(); }
    [[nodiscard]] const typename Base::ValueType &Front() const { return Base::Front(); }

    [[nodiscard]] typename Base::ValueType &Back() { return Base::GetBuffer()[Base::m_size - 2]; /* for NT char */ }
    [[nodiscard]] const typename Base::ValueType &Back() const { return Base::GetBuffer()[Base::m_size - 2]; /* for NT char */ }

    [[nodiscard]] bool Contains(const T &ch) const { return ch != T{0} && Base::Contains(ch); }
    [[nodiscard]] bool Contains(const DynString &str) const { return FindIndex(str) != not_found; }

    [[nodiscard]] SizeType FindIndex(const DynString &str) const;

    [[nodiscard]] bool Empty() const { return Size() == 0; }
    [[nodiscard]] bool Any() const { return Size() != 0; }

    [[nodiscard]] bool HasMultiByteChars() const { return Size() > Length(); }

    /*! \brief Reserve space for the string. {capacity} + 1 is used, to make space for the null character. */
    void Reserve(SizeType capacity) { Base::Reserve(capacity + 1); }

    // NOTE: no Resize(), because utf8 strings can have different length than size
    // /*! \brief Resizes to {new_size} + 1 to make space for the null character. */
    // void Resize(SizeType new_size)                      { Base::Resize(new_size + 1); }

    void Refit() { Base::Refit(); }

    void Append(const DynString &other);
    void Append(DynString &&other);
    void Append(const T *str);
    void Append(T &&value);
    void Append(const T &value);
    
    template <class U32Char, typename = std::enable_if_t<is_utf8 && std::is_same_v<U32Char, u32char>, int>>
    void Append(U32Char ch)
    {
        char parts[4] = { '\0' };
        utf::char32to8(ch, parts);

        Append(parts);
    }

    typename Base::ValueType PopBack();
    typename Base::ValueType PopFront();
    void Clear();

    bool StartsWith(const DynString &other) const;
    bool EndsWith(const DynString &other) const;

    DynString ToLower() const;
    DynString ToUpper() const;
    
    DynString Trimmed() const;
    DynString TrimmedLeft() const;
    DynString TrimmedRight() const;

    DynString Substr(SizeType first, SizeType last = MathUtil::MaxSafeValue<SizeType>()) const;

    DynString Escape() const;
    
    template <class ... SeparatorType>
    Array<DynString> Split(SeparatorType ... separators) const 
    {
        hyperion::FixedArray<WidestCharType, sizeof...(separators)> separator_values { WidestCharType(separators)... };

        const auto *data = Base::Data();
        const auto size = Size();

        Array<DynString> tokens;

        DynString working_string;
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
                UInt8 num_bytes_read = 0;

                union { UInt32 char_u32; UInt8 char_u8[sizeof(utf::u32char)]; };
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
    static DynString Join(const Container &container, const DynString &separator)
    {
        DynString result;

        for (auto it = container.Begin(); it != container.End(); ++it) {
            result.Append(ToString(*it));

            if (it != container.End() - 1) {
                result.Append(separator);
            }
        }

        return result;
    }

    template <class Container>
    static DynString Join(const Container &container, WidestCharType separator)
    {
        DynString result;

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

    static DynString Base64Encode(const Array<UByte> &bytes)
    {
        static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        DynString out;

        UInt i = 0;
        Int j = -6;

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

    static Array<UByte> Base64Decode(const DynString &in)
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

        Array<UByte> out;

        UInt i = 0;
        Int j = -8;

        for (auto &&c : in) {
            if (lookup_table[c] == -1) {
                break;
            }

            i = (i << 6) + lookup_table[c];
            j += 6;

            if (j >= 0) {
                out.PushBack(static_cast<UByte>((i >> j) & 0xFF));
                j -= 8;
            }
        }

        return out;
    }

    template <class Integral>
    static typename std::enable_if_t<std::is_integral_v<NormalizedType<Integral>>, DynString>
    ToString(Integral value)
    {
        SizeType result_size;
        utf::utf_to_str<Integral, T>(value, result_size, nullptr);

        AssertThrow(result_size >= 1);

        DynString result;
        result.Reserve(result_size - 1);  // String class automatically adds 1 for null character

        T *data = new T[result_size];
        utf::utf_to_str<Integral, T>(value, result_size, data);

        for (SizeType i = 0; i < result_size - 1; i++) {
            result.Append(data[i]);
        }

        delete[] data;

        return result;
    }
    
    static DynString ToString(const DynString &value) { return value; }
    static DynString ToString(DynString &&value)      { return value; }

    [[nodiscard]] HashCode GetHashCode() const
        { return HashCode::GetHashCode(Data()); }

protected:
    const T *StrStr(const DynString &other) const;

    SizeType m_length;
};

template <class T, bool IsUtf8>
const DynString<T, IsUtf8> DynString<T, IsUtf8>::empty = DynString();

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::DynString()
    : Base(),
      m_length(0)
{
    // null-terminated char
    Base::PushBack(T { 0 });
}

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::DynString(const T *str)
    : Base(),
      m_length(0)
{
    if (str == nullptr) {
        return;
    }
    
    int count;
    int len = utf::utf_strlen<T, IsUtf8>(str, &count);

    if (len == -1) {
        // invalid utf8 string
        // push back null terminated char
        Base::PushBack(T { 0 });

        return;
    }

    m_length = static_cast<SizeType>(len);

    const auto size = static_cast<SizeType>(count);
    
    // reserves + 1 for null char
    Reserve(size);

    for (SizeType i = 0; i < size; ++i) {
        Base::PushBack(str[i]);
    }
    
    // null-terminated char
    Base::PushBack(T { 0 });
}

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::DynString(const T *str, Int max_len)
    : Base(),
      m_length(0)
{
    if (str == nullptr) {
        return;
    }

    int count;
    int len = MathUtil::Min(utf::utf_strlen<T, IsUtf8>(str, &count), max_len);

    if (len == -1) {
        // invalid utf8 string
        // push back null terminated char
        Base::PushBack(T { 0 });

        return;
    }

    m_length = static_cast<SizeType>(len);

    const auto size = static_cast<SizeType>(count);
    
    // reserves + 1 for null char
    Reserve(size);

    for (SizeType i = 0; i < size; ++i) {
        Base::PushBack(str[i]);
    }
    
    // null-terminated char
    Base::PushBack(T { 0 });
}

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::DynString(const DynString &other)
    : Base(other),
      m_length(other.m_length)
{
}

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::DynString(DynString &&other) noexcept
    : Base(static_cast<Base &&>(std::move(other))),
      m_length(other.m_length)
{
    other.Clear();
}


template <class T, bool IsUtf8>
DynString<T, IsUtf8>::DynString(const CharArray<T> &char_array)
    : Base(),
      m_length(0)
{
    Base::Resize(char_array.Size());
    Memory::MemCpy(Data(), char_array.Data(), char_array.Size());
    
    // add null terminator char if it does not exist yet.
    if (char_array.Empty() || char_array.Back() != 0) {
        Base::PushBack(0);
    }

    m_length = utf::utf_strlen<T, IsUtf8>(Base::Data());
}

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::DynString(const ByteBuffer &byte_buffer)
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

    Base::Resize((size / sizeof(T)) + 1); // +1 for null char
    Memory::MemCpy(Data(), byte_buffer.Data(), size / sizeof(T));

    m_length = utf::utf_strlen<T, IsUtf8>(Base::Data());
}

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::~DynString()
{
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator=(const T *str) -> DynString&
{
    DynString<T, IsUtf8>::operator=(DynString(str));

    return *this;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator=(const DynString &other) -> DynString&
{
    Base::operator=(other);
    m_length = other.m_length;

    return *this;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator=(DynString &&other) noexcept -> DynString&
{
    const auto len = other.m_length;
    Base::operator=(std::move(other));
    m_length = len;
    other.Clear();

    return *this;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator+(const DynString &other) const -> DynString
{
    DynString result(*this);
    result.Append(other);

    return result;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator+(T ch) const -> DynString
{
    DynString result(*this);
    result.Append(ch);

    return result;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator+(DynString &&other) const -> DynString
{
    DynString result(*this);
    result.Append(std::move(other));

    return result;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator+=(const DynString &other) -> DynString&
{
    Append(other);

    return *this;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator+=(DynString &&other) -> DynString&
{
    Append(std::move(other));

    return *this;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator+=(T ch) -> DynString&
{
    Append(ch);

    return *this;
}

template <class T, bool IsUtf8>
bool DynString<T, IsUtf8>::operator==(const DynString &other) const
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

    return utf::utf_strcmp<T, IsUtf8>(Base::Data(), other.Data()) == 0;
}

template <class T, bool IsUtf8>
bool DynString<T, IsUtf8>::operator==(const T *str) const
{
    if (!str) {
        return *this == empty;
    }

    const auto len = utf::utf_strlen<T, IsUtf8>(str);

    if (len == -1) {
        return false; // invalid utf string
    }

    if (m_length != static_cast<SizeType>(len)) {
        return false;
    }

    if (Empty() && len == 0) {
        return true;
    }

    return utf::utf_strcmp<T, IsUtf8>(Base::Data(), str) == 0;
}

template <class T, bool IsUtf8>
bool DynString<T, IsUtf8>::operator!=(const DynString &other) const
{
    return !operator==(other);
}

template <class T, bool IsUtf8>
bool DynString<T, IsUtf8>::operator!=(const T *str) const
{
    return !operator==(str);
}

template <class T, bool IsUtf8>
bool DynString<T, IsUtf8>::operator<(const DynString &other) const
{
    return utf::utf_strcmp<T, IsUtf8>(Base::Data(), other.Data()) < 0;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator[](SizeType index) const -> const T
{
    return Base::operator[](index);
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::GetChar(SizeType index) const -> std::conditional_t<IsUtf8, u32char, T>
{
    if constexpr (is_utf8) {
        const SizeType size = Size();

        AssertThrow(index < size);

        return utf::utf8_charat(Data(), size, index);
    } else {
        return Base::operator[](index);
    }
}

template <class T, bool IsUtf8>
void DynString<T, IsUtf8>::Append(const DynString &other)
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
        new (&buffer[Base::m_size++].data_buffer) T(other[i]);
    }

    Base::PushBack(T { 0 });

    m_length += other.m_length;
}

template <class T, bool IsUtf8>
void DynString<T, IsUtf8>::Append(DynString &&other)
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
        new (&buffer[Base::m_size++].data_buffer) T(std::move(other[i]));
    }

    Base::PushBack(T { 0 });

    m_length += other.m_length;

    other.Clear();
}

template <class T, bool IsUtf8>
void DynString<T, IsUtf8>::Append(const T *str)
{
    Append(DynString(str));
}

template <class T, bool IsUtf8>
void DynString<T, IsUtf8>::Append(const T &value)
{
    if (Size() + 2 >= Base::m_capacity) {
        if (Base::m_capacity >= Size() + 2) {
            Base::ResetOffsets();
        } else {
            Base::SetCapacity(Base::GetCapacity(Size() + 2));
        }
    }

    Base::PopBack(); // current NT char

    new (&Base::GetStorage()[Base::m_size++].data_buffer) T(value);
    Base::PushBack(T { 0 });

    ++m_length;
}

template <class T, bool IsUtf8>
void DynString<T, IsUtf8>::Append(T &&value)
{
    // +2 to include NT char and new char being appended
    if (Size() + 2 >= Base::m_capacity) {
        if (Base::m_capacity >= Size() + 2) {
            Base::ResetOffsets();
        } else {
            Base::SetCapacity(Base::GetCapacity(Size() + 2));
        }
    }
    
    Base::PopBack(); // current NT char

    new (&Base::GetStorage()[Base::m_size++].data_buffer) T(std::forward<T>(value));
    Base::PushBack(T { 0 });

    ++m_length;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::PopFront() -> typename Base::ValueType
{
    --m_length;
    return Base::PopFront();
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::PopBack() -> typename Base::ValueType
{
    --m_length;
    Base::PopBack(); // pop NT-char
    auto &&res = Base::PopBack();
    Base::PushBack(T { 0 }); // add NT-char
    return res;
}

template <class T, bool IsUtf8>
void DynString<T, IsUtf8>::Clear()
{
    Base::Clear();
    Base::PushBack(T { 0 }); // NT char
    m_length = 0;
}

template <class T, bool IsUtf8>
bool DynString<T, IsUtf8>::StartsWith(const DynString &other) const
{
    if (Size() < other.Size()) {
        return false;
    }
    
    return std::equal(Base::Begin(), Base::Begin() + other.Size(), other.Base::Begin());
}

template <class T, bool IsUtf8>
bool DynString<T, IsUtf8>::EndsWith(const DynString &other) const
{
    if (Size() < other.Size()) {
        return false;
    }
    
    return std::equal(Base::Begin() + Size() - other.Size(), Base::End(), other.Base::Begin());
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::ToLower() const -> DynString
{
    DynString result(*this);

    std::transform(result.Begin(), result.End(), result.Begin(), [](auto ch) {
        return std::tolower(ch);
    });

    return result;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::ToUpper() const -> DynString
{
    DynString result(*this);

    std::transform(result.Begin(), result.End(), result.Begin(), [](auto ch) {
        return std::toupper(ch);
    });

    return result;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::Trimmed() const -> DynString
{
    return TrimmedLeft().TrimmedRight();
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::TrimmedLeft() const -> DynString
{
    DynString res;
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

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::TrimmedRight() const -> DynString
{
    DynString res;
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

template <class T, bool IsUtf8>
DynString<T, IsUtf8> DynString<T, IsUtf8>::Escape() const
{
    const SizeType size = Size();
    const auto *data = Base::Data();

    DynString result;
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
            UInt8 num_bytes_read = 0;

            union { UInt32 char_u32; UInt8 char_u8[sizeof(utf::u32char)]; };
            char_u32 = 0;

            char_u32 = utf::char8to32(
                Data() + i,
                MathUtil::Min(sizeof(utf::u32char), size - i),
                &num_bytes_read
            );

            i += num_bytes_read;

            switch (char_u32) {
            case UInt32('\n'):
                result.Append("\\n");
                break;
            case UInt32('\r'):
                result.Append("\\r");
                break;
            case UInt32('\t'):
                result.Append("\\t");
                break;
            case UInt32('\v'):
                result.Append("\\v");
                break;
            case UInt32('\b'):
                result.Append("\\b");
                break;
            case UInt32('\f'):
                result.Append("\\f");
                break;
            case UInt32('\a'):
                result.Append("\\a");
                break;
            case UInt32('\\'):
                result.Append("\\\\");
                break;
            case UInt32('\"'):
                result.Append("\\\"");
                break;
            case UInt32('\''):
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


template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::Substr(SizeType first, SizeType last) const -> DynString
{
    if (first == SizeType(-1)) {
        return *this;
    }

    last = MathUtil::Max(last, first);

    const auto size = Size();

    if constexpr (IsUtf8) {
        DynString result;

        for (SizeType i = 0; i < size;) {
            auto c = Base::operator[](i);

            if (i >= last) {
                break;
            }

            if (i >= first) {
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
        }

        return result;
    } else {
        if (first >= size) {
            return DynString::empty;
        }

        DynString result;
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

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::StrStr(const DynString &other) const -> const T*
{
    if (Size() < other.Size()) {
        return nullptr;
    }

    const T *other_str = other.Data();

    for (const T *str = Data(); *str != 0; ++str) {
        if (*str != *other_str) {
            continue;
        }

        const T *this_str = str;

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

template <class T, bool IsUtf8>
SizeType DynString<T, IsUtf8>::FindIndex(const DynString &other) const
{
    if (auto *ptr = StrStr(other)) {
        return static_cast<SizeType>(ptr - Data());
    }

    return not_found;
}

#if 0
template <class T, bool IsUtf8>
DynString<T, IsUtf8> operator+(const T *str, const DynString<T, IsUtf8> &other)
{
    return DynString<T, IsUtf8>(str) + other;
}
#endif

} // namespace detail
} // namespace containers


using CharArray = containers::detail::CharArray<char>;

using String = containers::detail::DynString<char, true>;

using ANSIString = containers::detail::DynString<char, false>;
using WideString = containers::detail::DynString<wchar_t, false>;

using UTF32String = containers::detail::DynString<utf::u32char, false>;
using UTF16String = containers::detail::DynString<utf::u16char, false>;

inline String operator+(const char *str, const String &other)
    { return String(str) + other; }

inline ANSIString operator+(const char *str, const ANSIString &other)
    { return ANSIString(str) + other; }

inline UTF16String operator+(const utf::u16char *str, const UTF16String &other)
    { return UTF16String(str) + other; }

inline UTF32String operator+(const utf::u32char *str, const UTF32String &other)
    { return UTF32String(str) + other; }

template <class CharType, Bool IsUtf8, typename = std::enable_if_t<std::is_same_v<CharType, char>, int>>
std::ostream &operator<<(std::ostream &os, const containers::detail::DynString<CharType, IsUtf8> &str)
{
    os << str.Data();

    return os;
}

template <class CharType, Bool IsUtf8, typename = std::enable_if_t<std::is_same_v<CharType, wchar_t>, int>>
std::wostream &operator<<(std::wostream &os, const containers::detail::DynString<CharType, IsUtf8> &str)
{
    os << str.Data();

    return os;
}

} // namespace hyperion

#endif