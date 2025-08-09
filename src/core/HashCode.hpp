/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>
#include <core/Constants.hpp>

#include <core/Traits.hpp>

#include <type_traits>
#include <bit>

// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n3876.pdf

namespace hyperion {

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

struct HashCode
{
    using ValueType = uint64;

    constexpr HashCode()
        : m_hash(0)
    {
    }

    constexpr explicit HashCode(ValueType value)
        : m_hash(value)
    {
    }

    constexpr HashCode(const HashCode& other) = default;
    constexpr HashCode& operator=(const HashCode& other) = default;

    constexpr HashCode(HashCode&& other) noexcept = default;
    constexpr HashCode& operator=(HashCode&& other) noexcept = default;

    constexpr bool operator==(const HashCode& other) const
    {
        return m_hash == other.m_hash;
    }

    constexpr bool operator!=(const HashCode& other) const
    {
        return m_hash != other.m_hash;
    }

    constexpr bool operator<(const HashCode& other) const
    {
        return m_hash < other.m_hash;
    }

    constexpr bool operator<=(const HashCode& other) const
    {
        return m_hash <= other.m_hash;
    }

    constexpr bool operator>(const HashCode& other) const
    {
        return m_hash > other.m_hash;
    }

    constexpr bool operator>=(const HashCode& other) const
    {
        return m_hash >= other.m_hash;
    }

    template <class T>
    HashCode& Add(const T& value)
    {
        HashCombine(GetHashCode(value));
        return *this;
    }

    constexpr ValueType Value() const
    {
        return m_hash;
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
            static_assert(resolutionFailure<T>,
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

    template <class T, typename = std::enable_if_t<!std::is_same_v<T, char>>>
    static inline HashCode GetHashCode(T* ptr)
    {
        return GetHashCode(reinterpret_cast<uintptr_t>(ptr));
    }

    template <class T, typename = std::enable_if_t<!std::is_same_v<T, char>>>
    static inline HashCode GetHashCode(const T* ptr)
    {
        return GetHashCode(reinterpret_cast<uintptr_t>(ptr));
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

    static inline HashCode GetHashCode(const ubyte* _begin, const ubyte* _end)
    {
        return HashCode(FNV1::DoHashBytes(_begin, _end));
    }

    static constexpr inline HashCode GetHashCode(const HashCode& hashCode)
    {
        return hashCode;
    }

    constexpr HashCode Combine(const HashCode& other) const
    {
        if (m_hash == 0)
        {
            return other;
        }

        HashCode hc;
        hc.m_hash = m_hash;
        hc.m_hash ^= other.m_hash + 0x9e3779b9 + (hc.m_hash << 6) + (hc.m_hash >> 2);
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
        if (m_hash == 0)
        {
            m_hash = other.m_hash;

            return;
        }

        m_hash ^= other.m_hash + 0x9e3779b9 + (m_hash << 6) + (m_hash >> 2);
    }

    ValueType m_hash;
};
} // namespace hyperion

namespace std {

// Specialize std::hash for HashCode
template <>
struct hash<hyperion::HashCode>
{
    size_t operator()(const hyperion::HashCode& hc) const
    {
        return static_cast<size_t>(hc.Value());
    }
};

} // namespace std
