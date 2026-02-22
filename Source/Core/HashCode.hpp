/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/Types.hpp>
#include <Core/Constants.hpp>
#include <Core/utilities/Traits.hpp>

#include <Core/reflection/ObjectMacros.hpp>

#include <type_traits>
#include <bit>

// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n3876.pdf

namespace Hyperion {

HYP_MAKE_HAS_METHOD(GetHashCode);

struct FNV1
{
    static constexpr uint64 offsetBasis = 14695981039346656037ull;
    static constexpr uint64 fnvPrime = 1099511628211ull;

    template <class CharType, SizeType Size>
    static constexpr uint64 DoHashString(const CharType (&str)[Size])
    {
        uint64 hash = offsetBasis;

        for (SizeType i = 0; i < Size; ++i)
        {
            if (!str[i])
            {
                break;
            }

            hash ^= str[i];
            hash *= fnvPrime;
        }

        return hash;
    }

    template <class CharType>
    static constexpr uint64 DoHashString(const CharType* str)
    {
        uint64 hash = offsetBasis;

        while (*str)
        {
            hash ^= *str;
            hash *= fnvPrime;

            ++str;
        }

        return hash;
    }

    template <class CharType>
    static constexpr uint64 DoHashString(const CharType* _begin, const CharType* _end)
    {
        uint64 hash = offsetBasis;

        while (*_begin && _begin != _end)
        {
            hash ^= *_begin;
            hash *= fnvPrime;

            ++_begin;
        }

        return hash;
    }

    static constexpr uint64 DoHashBytes(const ubyte* _begin, const ubyte* _end)
    {
        uint64 hash = offsetBasis;

        while (_begin != _end)
        {
            hash ^= *_begin;
            hash *= fnvPrime;

            ++_begin;
        }

        return hash;
    }
};

HYP_STRUCT()
struct HashCode
{
    HYP_STRUCT_BODY(HashCode);

    using ValueType = uint64;

    ValueType value;

    constexpr HashCode()
        : value(0)
    {
    }

    constexpr explicit HashCode(ValueType value)
        : value(value)
    {
    }

    constexpr HashCode(const HashCode& other) = default;
    constexpr HashCode& operator=(const HashCode& other) = default;

    constexpr HashCode(HashCode&& other) noexcept = default;
    constexpr HashCode& operator=(HashCode&& other) noexcept = default;

    constexpr bool operator==(const HashCode& other) const
    {
        return value == other.value;
    }

    constexpr bool operator!=(const HashCode& other) const
    {
        return value != other.value;
    }

    constexpr bool operator<(const HashCode& other) const
    {
        return value < other.value;
    }

    constexpr bool operator<=(const HashCode& other) const
    {
        return value <= other.value;
    }

    constexpr bool operator>(const HashCode& other) const
    {
        return value > other.value;
    }

    constexpr bool operator>=(const HashCode& other) const
    {
        return value >= other.value;
    }

    template <class T>
    HashCode& Add(const T& value)
    {
        HashCombine(GetHashCode(value));
        return *this;
    }

    constexpr ValueType Value() const
    {
        return value;
    }

    template <class T, class DecayedType = std::decay_t<T>>
    static constexpr inline
        typename std::enable_if_t<!(std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>) && HYP_HAS_METHOD(DecayedType, GetHashCode), HashCode>
        GetHashCode(const T& value)
    {
        return value.GetHashCode();
    }

    template <class T, class DecayedType = std::decay_t<T>>
    static constexpr inline typename std::enable_if_t<!(std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>) && !HYP_HAS_METHOD(DecayedType, GetHashCode)
            && !std::is_pointer_v<DecayedType> && std::is_arithmetic_v<DecayedType>,
        HashCode>
    GetHashCode(T&& value)
    {
        if constexpr (sizeof(T) == sizeof(uint64))
        {
            return HashCode(ValueType(std::bit_cast<uint64>(value)));
        }
        else if constexpr (sizeof(T) == sizeof(uint32))
        {
            return HashCode(ValueType(std::bit_cast<uint32>(value)));
        }
        else if constexpr (sizeof(T) == sizeof(uint16))
        {
            return HashCode(ValueType(std::bit_cast<uint16>(value)));
        }
        else if constexpr (sizeof(T) == sizeof(uint8))
        {
            return HashCode(ValueType(std::bit_cast<uint8>(value)));
        }
        else
        {
            static_assert(ResolutionFailureV<T>,
                "HashCode::GetHashCode: Unsupported type size for arthmetic type. Supported sizes are 8, 16, 32, and 64 bits.");
        }
    }

    template <class T, class DecayedType = std::decay_t<T>>
    static constexpr inline typename std::enable_if_t<!(std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>) && !HYP_HAS_METHOD(DecayedType, GetHashCode)
            && !std::is_pointer_v<DecayedType> && std::is_fundamental_v<DecayedType> && !std::is_integral_v<DecayedType>,
        HashCode>
    GetHashCode(const T& value)
    {
        return HashCode(FNV1::DoHashBytes(reinterpret_cast<const ubyte*>(&value), reinterpret_cast<const ubyte*>(&value) + sizeof(T)));
    }

    template <class T, class DecayedType = std::decay_t<T>>
    static constexpr inline typename std::enable_if_t<!(std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>) && !HYP_HAS_METHOD(DecayedType, GetHashCode)
            && !std::is_pointer_v<DecayedType> && std::is_enum_v<DecayedType>,
        HashCode>
    GetHashCode(const T& value)
    {
        return GetHashCode(static_cast<std::underlying_type_t<DecayedType>>(value));
    }

    static inline HashCode GetHashCode(const void* ptr)
    {
        return GetHashCode(reinterpret_cast<UIntPtr>(ptr));
    }

    template <SizeType Size>
    static constexpr inline HashCode GetHashCode(const char (&str)[Size])
    {
        return HashCode(FNV1::DoHashString<char, Size>(str));
    }

    static constexpr inline HashCode GetHashCode(const char* str)
    {
        return HashCode(FNV1::DoHashString(str));
    }

    static constexpr inline HashCode GetHashCode(const char* _begin, const char* _end)
    {
        return HashCode(FNV1::DoHashString(_begin, _end));
    }

    template <SizeType Size>
    static constexpr inline HashCode GetHashCode(const char16_t (&str)[Size])
    {
        return HashCode(FNV1::DoHashString<char16_t, Size>(str));
    }

    static constexpr inline HashCode GetHashCode(const char16_t* str)
    {
        return HashCode(FNV1::DoHashString(str));
    }

    static constexpr inline HashCode GetHashCode(const char16_t* _begin, const char16_t* _end)
    {
        return HashCode(FNV1::DoHashString(_begin, _end));
    }

    template <SizeType Size>
    static constexpr inline HashCode GetHashCode(const char32_t (&str)[Size])
    {
        return HashCode(FNV1::DoHashString<char32_t, Size>(str));
    }

    static constexpr inline HashCode GetHashCode(const char32_t* str)
    {
        return HashCode(FNV1::DoHashString(str));
    }

    static constexpr inline HashCode GetHashCode(const char32_t* _begin, const char32_t* _end)
    {
        return HashCode(FNV1::DoHashString(_begin, _end));
    }

    template <SizeType Size>
    static constexpr inline HashCode GetHashCode(const wchar_t (&bytes)[Size])
    {
        return HashCode(FNV1::DoHashString<wchar_t, Size>(bytes));
    }

    static constexpr inline HashCode GetHashCode(const wchar_t* str)
    {
        return HashCode(FNV1::DoHashString(str));
    }

    static constexpr inline HashCode GetHashCode(const wchar_t* _begin, const wchar_t* _end)
    {
        return HashCode(FNV1::DoHashString(_begin, _end));
    }

    static inline HashCode GetHashCode(const ubyte* _begin, const ubyte* _end)
    {
        return HashCode(FNV1::DoHashBytes(_begin, _end));
    }

    template <class T, SizeType Size>
    static constexpr inline HashCode GetHashCode(const T (&elems)[Size])
    {
        HashCode hc;
        for (SizeType i = 0; i < Size; i++)
        {
            hc = hc.Combine(HashCode::GetHashCode(elems[i]));
        }

        return hc;
    }

    template <class T>
    static constexpr inline HashCode GetHashCode(const T* _begin, const T* _end)
    {
        HashCode hc;
        while (_begin && _begin != _end)
        {
            hc = hc.Combine(HashCode::GetHashCode(*_begin));
            ++_begin;
        }

        return hc;
    }

    static constexpr inline HashCode GetHashCode(const HashCode& hashCode)
    {
        return hashCode;
    }

    constexpr HashCode Combine(const HashCode& other) const
    {
        if (value == 0)
        {
            return other;
        }

        HashCode hc;
        hc.value = value;
        hc.value ^= other.value + 0x9e3779b9 + (hc.value << 6) + (hc.value >> 2);
        return hc;
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<T, HashCode>>>
    constexpr HashCode Combine(const T& value) const
    {
        return Combine(GetHashCode(value));
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return *this;
    }

private:
    constexpr void HashCombine(const HashCode& other)
    {
        if (value == 0)
        {
            value = other.value;

            return;
        }

        value ^= other.value + 0x9e3779b9 + (value << 6) + (value >> 2);
    }
};
} // namespace Hyperion

namespace std {

// Specialize std::hash for HashCode
template <>
struct hash<Hyperion::HashCode>
{
    size_t operator()(const Hyperion::HashCode& hc) const
    {
        return static_cast<size_t>(hc.Value());
    }
};

} // namespace std
