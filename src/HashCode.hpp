/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HASH_CODE_HPP
#define HASH_CODE_HPP

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>
#include <functional>
#include <string>
#include <cstring>

namespace hyperion {

template <class T, class HashCode, class Enabled = void>
struct HasGetHashCode
{
    static constexpr bool value = false;
};

template <class T, class HashCode>
struct HasGetHashCode< T, HashCode, std::enable_if_t< std::is_member_function_pointer_v< decltype(&T::GetHashCode) > > >
{
    static constexpr bool value = std::is_member_function_pointer_v< decltype(&T::GetHashCode) >;
};

struct HashCode
{
    using ValueType = uint64;

    constexpr HashCode()
        : hash(0)
    {
    }

    constexpr HashCode(ValueType value)
        : hash(value)
    {
    }

    constexpr HashCode(const HashCode &other) = default;
    constexpr HashCode &operator=(const HashCode &other) = default;

    constexpr HashCode(HashCode &&other) noexcept = default;
    constexpr HashCode &operator=(HashCode &&other) noexcept = default;

    constexpr bool operator==(const HashCode &other) const { return hash == other.hash; }
    constexpr bool operator!=(const HashCode &other) const { return hash != other.hash; }
    constexpr bool operator<(const HashCode &other) const { return hash < other.hash; }
    constexpr bool operator<=(const HashCode &other) const { return hash <= other.hash; }
    constexpr bool operator>(const HashCode &other) const { return hash > other.hash; }
    constexpr bool operator>=(const HashCode &other) const { return hash >= other.hash; }

    template<class T>
    typename std::enable_if_t<std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>, HashCode &>
    Add(const T &hash_code) { HashCombine(hash_code.Value()); return *this; }

    //template<class T, class DecayedType = std::decay_t<T>>
   // typename std::enable_if_t<!(std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>) && !HasGetHashCode<DecayedType, HashCode>::value && implementation_exists<std::hash<DecayedType>>>
    //Add(const T &value) { HashCombine(std::hash<DecayedType>()(value)); }

    template<class T, class DecayedType = std::decay_t<T>>
    typename std::enable_if_t<!(std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>) && !HasGetHashCode<DecayedType, HashCode>::value &&
        (std::is_fundamental_v<DecayedType> || std::is_pointer_v<DecayedType> || std::is_enum_v<DecayedType>), HashCode &>
    Add(const T &value)
    {
        static_assert(sizeof(T) <= sizeof(uintmax_t));

        uintmax_t value_raw;

        if constexpr (sizeof(uintmax_t) > sizeof(T)) {
            std::memset(&value_raw, 0, sizeof(uintmax_t));
        }

        std::memcpy(&value_raw, &value, sizeof(T));

        HashCombine(value_raw);

        return *this;
    }

    template<class T, class DecayedType = std::decay_t<T>>
    typename std::enable_if_t<!(std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>) && HasGetHashCode<DecayedType, HashCode>::value, HashCode &>
    Add(const T &value) { HashCombine(value.GetHashCode().Value()); return *this; }

    template<class T, class DecayedType = std::decay_t<T>>
    typename std::enable_if_t<!(std::is_same_v<T, HashCode> || std::is_base_of_v<HashCode, T>) && !HasGetHashCode<DecayedType, HashCode>::value && !implementation_exists<std::hash<DecayedType>>, HashCode &>
    Add(const T &value) { static_assert(resolution_failure<T>, "No GetHashCode() method"); return *this; }

    constexpr ValueType Value() const { return hash; }

    template <class T>
    static constexpr inline HashCode GetHashCode(const T &value)
    {
        HashCode hc;
        hc.Add(value);
        return hc;
    }

    template <SizeType Size>
    static constexpr inline HashCode GetHashCode(const char (&str)[Size])
    {
        HashCode hc;

        for (SizeType i = 0; i < Size; ++i) {
            hc.HashCombine(str[i]);
        }

        return hc;
    }

    static constexpr inline HashCode GetHashCode(const char *str)
    {
        HashCode hc;

        while (*str) {
            hc.HashCombine(*str);
            ++str;
        }

        return hc;
    }

    static constexpr inline HashCode GetHashCode(const char *_begin, const char *_end)
    {
        HashCode hc;

        while (*_begin && _begin != _end) {
            hc.HashCombine(*_begin);
            ++_begin;
        }

        return hc;
    }

private:
    ValueType hash;

    constexpr void HashCombine(ValueType other)
    {
        hash ^= other + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
};
} // namespace hyperion

namespace std {

// Specialize std::hash for HashCode
template <>
struct hash<hyperion::HashCode>
{
    size_t operator()(const hyperion::HashCode &hc) const
    {
        return static_cast<size_t>(hc.Value());
    }
};

} // namespace std

#endif
