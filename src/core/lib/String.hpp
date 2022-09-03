#ifndef HYPERION_V2_LIB_STRING_H
#define HYPERION_V2_LIB_STRING_H

#include <util/UTF8.hpp>

#include "DynArray.hpp"
#include <util/Defines.hpp>
#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>

#include <algorithm>
#include <utility>
#include <type_traits>

namespace hyperion {
namespace containers {
namespace detail {
/*! UTF-8 supporting dynamic string class */
using namespace ::utf;

template <class T, bool IsUtf8>
class DynString : DynArray<T> {
protected:
    using Base = typename DynArray<T>::Base;

public:
    using ValueType = typename Base::ValueType;
    using KeyType = typename Base::KeyType;

    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    static const inline DynString empty = DynString();

    static constexpr bool is_utf8 = IsUtf8;
    static constexpr bool is_ansi = !is_utf8 && (std::is_same_v<T, char> || std::is_same_v<T, unsigned char>);

    DynString();
    DynString(const T *str);
    DynString(const DynString &other);
    DynString(DynString &&other) noexcept;
    ~DynString();

    DynString &operator=(const T *str);
    DynString &operator=(const DynString &other);
    DynString &operator=(DynString &&other) noexcept;

    DynString operator+(const DynString &other) const;
    DynString operator+(DynString &&other) const;
    DynString &operator+=(const DynString &other);
    DynString &operator+=(DynString &&other);

    bool operator==(const DynString &other) const;
    bool operator==(const T *str) const;
    bool operator!=(const DynString &other) const;
    bool operator!=(const T *str) const;

    bool operator<(const DynString &other) const;
    
    [[nodiscard]] const T operator[](SizeType index) const;

    // get raw char (no utf-8 conversion) at an index
    [[nodiscard]] T GetRawChar(SizeType index) const;

    /*! \brief Return the data size in characters. Note, utf-8 strings can have a shorter length than size. */
    [[nodiscard]] typename Base::SizeType Size() const                 { return Base::Size() - 1; /* for NT char */ }
    /*! \brief Return the length of the string in characters. Note, utf-8 strings can have a shorter length than size. */
    [[nodiscard]] typename Base::SizeType Length() const               { return m_length; }

    [[nodiscard]] typename Base::ValueType *Data()                     { return Base::Data(); }
    [[nodiscard]] const typename Base::ValueType *Data() const         { return Base::Data(); }

    [[nodiscard]] typename Base::ValueType &Front()                    { return Base::Front(); }
    [[nodiscard]] const typename Base::ValueType &Front() const        { return Base::Front(); }

    [[nodiscard]] typename Base::ValueType &Back()                     { return Base::m_values[Base::m_size - 2]; /* for NT char */ }
    [[nodiscard]] const typename Base::ValueType &Back() const         { return Base::m_values[Base::m_size - 2]; /* for NT char */ }

    [[nodiscard]] bool Contains(const T &ch) const                     { return ch != T{0} && Base::Contains(ch); }
    [[nodiscard]] Int FindIndex(const DynString &str) const;

    [[nodiscard]] bool Empty() const                                   { return Size() == 0; }
    [[nodiscard]] bool Any() const                                     { return Size() != 0; }

    [[nodiscard]] bool HasMultiByteChars() const                       { return Size() > Length(); }

    /*! \brief Reserve space for the string. {capacity} + 1 is used, to make space for the null character. */
    void Reserve(typename Base::SizeType capacity)                     { Base::Reserve(capacity + 1); }

    // NOTE: no Resize(), because utf8 strings can have different length than size
    // /*! \brief Resizes to {new_size} + 1 to make space for the null character. */
    // void Resize(typename Base::SizeType new_size)                      { Base::Resize(new_size + 1); }

    void Refit()                                                       { Base::Refit(); }
    void Append(const DynString &other);
    void Append(DynString &&other);
    void Append(T &&value);
    void Append(const T &value);
    typename Base::ValueType PopBack();
    typename Base::ValueType PopFront();
    void Clear();

    bool StartsWith(const DynString &other) const;
    bool EndsWith(const DynString &other) const;

    static DynString Base64Encode(const DynArray<UByte> &bytes)
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

    static DynArray<UByte> Base64Decode(const DynString &in)
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

        DynArray<UByte> out;

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
    ToString(Integral &&value)
    {
        SizeType result_size;
        utf::utf_to_str(value, result_size, nullptr);

        AssertThrow(result_size >= 1);

        DynString result;

        if constexpr (is_ansi) { // can write into memory direct without allocating any more
            result.Resize(result_size - 1); // String class automatically adds 1 for null character
            utf::utf_to_str(value, result_size, result.Data());
        } else {
            result.Reserve(result_size - 1);  // String class automatically adds 1 for null character

            char *data = new char[result_size];
            utf::utf_to_str(value, result_size, data);

            for (SizeType i = 0; i < result_size; i++) {
                result.Append(static_cast<T>(data[i]));
            }

            delete[] data;
        }

        return result;
    }

    [[nodiscard]] HashCode GetHashCode() const
        { return Base::GetHashCode(); }

    HYP_DEF_STL_BEGIN_END(
        reinterpret_cast<typename DynArray<T>::ValueType *>(&DynArray<T>::m_buffer[DynArray<T>::m_start_offset]),
        reinterpret_cast<typename DynArray<T>::ValueType *>(&DynArray<T>::m_buffer[DynArray<T>::m_size])
    )

    friend std::ostream &operator<<(std::ostream &os, const DynString &str)
    {
        os << str.Data();

        return os;
    }
    
#ifdef HYP_UTF8_WIDE
    friend utf::utf8_ostream &operator<<(utf::utf8_ostream &os, const DynString &str)
    {
        os << HYP_UTF8_TOWIDE(str.Data());

        return os;
    }
#endif

protected:
    const T *StrStr(const DynString &other) const;

    typename Base::SizeType m_length;
};

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::DynString()
    : DynArray<T>(),
      m_length(0)
{
    // null-terminated char
    Base::PushBack(T { 0 });
}

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::DynString(const T *str)
    : DynArray<T>()
{
    int count;
    int len = utf::utf_strlen<T, IsUtf8>(str, &count);

    if (len == -1) {
        // invalid utf8 string
        // push back null terminated char
        Base::PushBack(T { 0 });

        return;
    }

    m_length = static_cast<typename Base::SizeType>(len);

    const auto size = static_cast<typename Base::SizeType>(count);
    
    // reserves + 1 for null char
    Reserve(size);

    for (SizeType i = 0; i < size; ++i) {
        Base::PushBack(str[i]);
    }
    
    // null-terminated char
    Base::PushBack(T{0});
}

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::DynString(const DynString &other)
    : DynArray<T>(other),
      m_length(other.m_length)
{
}

template <class T, bool IsUtf8>
DynString<T, IsUtf8>::DynString(DynString &&other) noexcept
    : DynArray<T>(std::move(other)),
      m_length(other.m_length)
{
    other.m_length = 0;
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
    other.m_length = 0;

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

    return utf::utf_strcmp<T, IsUtf8>(&Base::m_values[0], &other.m_values[0]) == 0;
}

template <class T, bool IsUtf8>
bool DynString<T, IsUtf8>::operator==(const T *str) const
{
    const auto len = utf::utf_strlen<T, IsUtf8>(str);

    if (len == -1) {
        return false; // invalid utf string
    }

    if (m_length != static_cast<typename Base::SizeType>(len)) {
        return false;
    }

    if (Empty() && len == 0) {
        return true;
    }

    return utf::utf_strcmp<T, IsUtf8>(&Base::m_values[0], str) == 0;
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
    return utf::utf_strcmp<T, IsUtf8>(&Base::m_values[0], &other.m_values[0]) < 0;
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::operator[](SizeType index) const -> const T
{
    if constexpr (is_utf8) {
        char ch;
        utf::utf8_charat(Data(), &ch, index);
        return static_cast<T>(ch);
    } else {
        return Base::operator[](index);
    }
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::GetRawChar(SizeType index) const -> T
{
    return Base::operator[](index);
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

    for (SizeType i = 0; i < other.Size(); i++) {
        // set item at index
        new (&Base::m_buffer[Base::m_size++].data_buffer) T(other[i]);
    }

    Base::PushBack(T{0});

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


    for (SizeType i = 0; i < other.Size(); i++) {
        // set item at index
        new (&Base::m_buffer[Base::m_size++].data_buffer) T(std::move(other[i]));
    }

    Base::PushBack(T{0});

    m_length += other.m_length;

    other.Clear();
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

    new (&Base::m_buffer[Base::m_size++].data_buffer) T(value);
    Base::PushBack(T{0});

    ++m_length;
}

template <class T, bool IsUtf8>
void DynString<T, IsUtf8>::Append(T &&value)
{
    if (Size() + 2 >= Base::m_capacity) {
        if (Base::m_capacity >= Size() + 2) {
            Base::ResetOffsets();
        } else {
            Base::SetCapacity(Base::GetCapacity(Size() + 2));
        }
    }

    Base::PopBack(); // current NT char

    new (&Base::m_buffer[Base::m_size++].data_buffer) T(std::forward<T>(value));
    Base::PushBack(T{0});

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

    auto res = Base::PopBack();

    Base::PushBack(T { 0 }); // add back NT-char

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
    
    return std::equal(Begin(), Begin() + other.Size(), other.Begin());
}

template <class T, bool IsUtf8>
bool DynString<T, IsUtf8>::EndsWith(const DynString &other) const
{
    if (Size() < other.Size()) {
        return false;
    }
    
    return std::equal(Begin() + Size() - other.Size(), End(), other.Begin());
}

template <class T, bool IsUtf8>
auto DynString<T, IsUtf8>::StrStr(const DynString &other) const -> const T*
{
    if (Size() < other.Size()) {
        return nullptr;
    }

    const T *a, *b;
    b = other.Data();

    for (const T *str = Data(); *str != 0; ++str) {
        if (*str != *b) {
            continue;
        }

        a = str;

        for (;;) {
            if (*b == 0) {
                return str;
            }

            if (*a++ != *b++) {
                break;
            }
        }

        b = other.Data();
    }

    return nullptr;
}

template <class T, bool IsUtf8>
Int DynString<T, IsUtf8>::FindIndex(const DynString &other) const
{
    if (auto *ptr = StrStr(other)) {
        return static_cast<Int>(ptr - Data());
    }

    return -1;
}

} // namespace detail
} // namespace containers

using String = containers::detail::DynString<char, true>;
using ANSIString = containers::detail::DynString<char, false>;
using WideString = containers::detail::DynString<wchar_t, false>;

} // namespace hyperion

#endif