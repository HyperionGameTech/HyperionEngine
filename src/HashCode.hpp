#ifndef HASH_CODE_H
#define HASH_CODE_H

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>
#include <functional>
#include <string>
#include <cstring>

namespace hyperion {

template <class T, class HashCode>
struct HasGetHashCode
{
    template <class U, HashCode(U::*)() const>
    struct Resolve {};

    template <class U>
    static char Test(Resolve<U, &U::GetHashCode> *);

    template <class U>
    static int Test(...);

    static constexpr bool value = sizeof(Test<T>(0)) == sizeof(char);
};

struct HashCode
{
    using ValueType = UInt64;

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
    static inline HashCode GetHashCode(const T &value)
    {
        HashCode hc;
        hc.Add(value);
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

private:
    ValueType hash;

    constexpr void HashCombine(ValueType other)
    {
        hash ^= other + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
};
} // namespace hyperion

#endif
