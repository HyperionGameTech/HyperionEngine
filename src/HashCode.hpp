/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HASH_CODE_HPP
#define HASH_CODE_HPP

#include <Types.hpp>
#include <Constants.hpp>

#include <core/Traits.hpp>

#include <type_traits>

// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n3876.pdf

namespace hyperion {

HYP_MAKE_HAS_METHOD(GetHashCode);

struct FNV1
{
    static constexpr uint64 offset_basis = 14695981039346656037ull;
    static constexpr uint64 fnv_prime = 1099511628211ull;

    template <class CharType, SizeType Size>
    static constexpr uint64 HashString(const CharType (&str)[Size])
    {
        uint64 hash = offset_basis;

        for (SizeType i = 0; i < Size; ++i)
        {
            if (!str[i])
            {
                break;
            }

            hash ^= str[i];
            hash *= fnv_prime;
        }

        return hash;
    }

    template <class CharType>
    static constexpr uint64 HashString(const CharType* str)
    {
        uint64 hash = offset_basis;

        while (*str)
        {
            hash ^= *str;
            hash *= fnv_prime;

            ++str;
        }

        return hash;
    }

    template <class CharType>
    static constexpr uint64 HashString(const CharType* _begin, const CharType* _end)
    {
        uint64 hash = offset_basis;

        while (*_begin && _begin != _end)
        {
            hash ^= *_begin;
            hash *= fnv_prime;

            ++_begin;
        }

        return hash;
    }

    static constexpr uint64 HashBytes(const ubyte* _begin, const ubyte* _end)
    {
        uint64 hash = offset_basis;

        while (_begin != _end)
        {
            hash ^= *_begin;
            hash *= fnv_prime;

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
            && !std::is_pointer_v<DecayedType> && std::is_fundamental_v<DecayedType>,
        HashCode>
    GetHashCode(const T& value)
    {
        return HashCode(FNV1::HashBytes(reinterpret_cast<const ubyte*>(&value), reinterpret_cast<const ubyte*>(&value) + sizeof(T)));
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
    static constexpr inline HashCode GetHashCode(const T* ptr)
    {
        return GetHashCode(uintptr_t(ptr));
    }

    static constexpr inline HashCode GetHashCode(const HashCode& hash_code)
    {
        return hash_code;
    }

    template <SizeType Size>
    static constexpr inline HashCode GetHashCode(const char (&str)[Size])
    {
        return HashCode(FNV1::HashString<char, Size>(str));
    }

    static constexpr inline HashCode GetHashCode(const char* str)
    {
        return HashCode(FNV1::HashString(str));
    }

    static constexpr inline HashCode GetHashCode(const char* _begin, const char* _end)
    {
        return HashCode(FNV1::HashString(_begin, _end));
    }

    static inline HashCode GetHashCode(const ubyte* _begin, const ubyte* _end)
    {
        return HashCode(FNV1::HashBytes(_begin, _end));
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

#endif
