/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PAIR_HPP
#define HYPERION_PAIR_HPP

#include <core/Traits.hpp>

#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>

#include <type_traits>

namespace hyperion {

namespace utilities {
namespace detail {

using PairArgTraits = uint32;

enum PairArgTrait : PairArgTraits
{
    NONE                    = 0x0,
    DEFAULT_CONSTRUCTIBLE   = 0x1,
    COPY_CONSTRUCTIBLE      = 0x2,
    COPY_ASSIGNABLE         = 0x4,
    MOVE_CONSTRUCTIBLE      = 0x8,
    MOVE_ASSIGNABLE         = 0x10,
    ALL                     = DEFAULT_CONSTRUCTIBLE | COPY_CONSTRUCTIBLE | COPY_ASSIGNABLE | MOVE_CONSTRUCTIBLE | MOVE_ASSIGNABLE
};

template <class First, class Second>
struct PairHelper
{
    static constexpr bool default_constructible = (std::is_default_constructible_v<First> && (std::is_default_constructible_v<Second>));
    static constexpr bool copy_constructible = (std::is_copy_constructible_v<First> && (std::is_copy_constructible_v<Second>));
    static constexpr bool copy_assignable = (std::is_copy_assignable_v<First> && (std::is_copy_assignable_v<Second>));
    static constexpr bool move_constructible = (std::is_move_constructible_v<First> && (std::is_move_constructible_v<Second>));
    static constexpr bool move_assignable = (std::is_move_assignable_v<First> && (std::is_move_assignable_v<Second>));
};

// template <PairArgTraits FirstTraits, PairArgTraits SecondTraits, class First, class Second>
// struct PairBase :
//     private ConstructAssignmentTraits<
//         PairHelper<First, Second>::default_constructible,
//         (FirstTraits & COPY_CONSTRUCTIBLE) && (SecondTraits & COPY_CONSTRUCTIBLE),
//         (FirstTraits & MOVE_CONSTRUCTIBLE) && (SecondTraits & MOVE_CONSTRUCTIBLE),
//         PairBase<FirstTraits, SecondTraits, First, Second>
//     >
// {
// };

/*! \brief Simple Pair class that is also able to hold a reference to a key or value */
template <class First, class Second, typename T1 = void, typename T2 = void>
struct Pair
{
    static_assert(resolution_failure<Pair>, "Invalid Pair traits");
};

template <class First, class Second>
struct Pair<
    First, Second,
    std::enable_if_t<std::is_copy_constructible_v<std::remove_const_t<First>> && (std::is_move_constructible_v<std::remove_const_t<First>> && !std::is_reference_v<First>)>,
    std::enable_if_t<std::is_copy_constructible_v<std::remove_const_t<Second>> && (std::is_move_constructible_v<std::remove_const_t<Second>> && !std::is_reference_v<Second>)>
>
{
    First   first;
    Second  second;

    constexpr Pair()                            = default;

    constexpr Pair(const First &first, const Second &second)
        : first(first), second(second) {}

    constexpr Pair(First &&first, const Second &second)
        : first(std::move(first)), second(second) {}

    constexpr Pair(const First &first, Second &&second)
        : first(first), second(std::move(second)) {}

    constexpr Pair(First &&first, Second &&second)
        : first(std::move(first)), second(std::move(second)) {}

    constexpr Pair(const Pair &other)       = default;
    Pair &operator=(const Pair &other)      = default;

    constexpr Pair(Pair &&other) noexcept   = default;
    Pair &operator=(Pair &&other) noexcept  = default;

    ~Pair()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const Pair &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const Pair &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const Pair &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const Pair &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const Pair &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const Pair &other) const
        { return !operator==(other); }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(first);
        hc.Add(second);

        return hc;
    }
};

template <class First, class Second>
struct Pair<
    First, Second,
    std::enable_if_t<!std::is_copy_constructible_v<std::remove_const_t<First>> && (std::is_move_constructible_v<std::remove_const_t<First>> && !std::is_reference_v<First>)>,
    std::enable_if_t<std::is_copy_constructible_v<std::remove_const_t<Second>> && (std::is_move_constructible_v<std::remove_const_t<Second>> && !std::is_reference_v<Second>)>
>
{
    First   first;
    Second  second;

    constexpr Pair()                            = default;

    constexpr Pair(First &&first, const Second &second)
        : first(std::move(first)), second(second) {}

    constexpr Pair(First &&first, Second &&second)
        : first(std::move(first)), second(std::move(second)) {}

    constexpr Pair(const Pair &other)       = default;
    Pair &operator=(const Pair &other)      = default;

    constexpr Pair(Pair &&other) noexcept   = default;
    Pair &operator=(Pair &&other) noexcept  = default;

    ~Pair()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const Pair &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const Pair &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const Pair &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const Pair &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const Pair &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const Pair &other) const
        { return !operator==(other); }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(first);
        hc.Add(second);

        return hc;
    }
};

template <class First, class Second>
struct Pair<
    First, Second,
    std::enable_if_t<std::is_copy_constructible_v<std::remove_const_t<First>> && (std::is_move_constructible_v<std::remove_const_t<First>> && !std::is_reference_v<First>)>,
    std::enable_if_t<std::is_copy_constructible_v<std::remove_const_t<Second>> && !(std::is_move_constructible_v<std::remove_const_t<Second>> && !std::is_reference_v<Second>)>
>
{
    First   first;
    Second  second;

    constexpr Pair()                            = default;

    constexpr Pair(const First &first, const Second &second)
        : first(first), second(second) {}

    constexpr Pair(First &&first, const Second &second)
        : first(std::move(first)), second(second) {}

    constexpr Pair(const Pair &other)       = default;
    Pair &operator=(const Pair &other)      = default;

    constexpr Pair(Pair &&other) noexcept   = default;
    Pair &operator=(Pair &&other) noexcept  = default;

    ~Pair()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const Pair &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const Pair &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const Pair &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const Pair &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const Pair &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const Pair &other) const
        { return !operator==(other); }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(first);
        hc.Add(second);

        return hc;
    }
};

template <class First, class Second>
struct Pair<
    First, Second,
    std::enable_if_t<std::is_copy_constructible_v<std::remove_const_t<First>> && !(std::is_move_constructible_v<std::remove_const_t<First>> && !std::is_reference_v<First>)>,
    std::enable_if_t<std::is_copy_constructible_v<std::remove_const_t<Second>> && (std::is_move_constructible_v<std::remove_const_t<Second>> && !std::is_reference_v<Second>)>
>
{
    First   first;
    Second  second;

    constexpr Pair()                            = default;

    constexpr Pair(const First &first, const Second &second)
        : first(first), second(second) {}

    constexpr Pair(const First &first, Second &&second)
        : first(first), second(std::move(second)) {}

    constexpr Pair(const Pair &other)       = default;
    Pair &operator=(const Pair &other)      = default;

    constexpr Pair(Pair &&other) noexcept   = default;
    Pair &operator=(Pair &&other) noexcept  = default;

    ~Pair()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const Pair &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const Pair &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const Pair &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const Pair &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const Pair &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const Pair &other) const
        { return !operator==(other); }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(first);
        hc.Add(second);

        return hc;
    }
};

template <class First, class Second>
struct Pair<
    First, Second,
    std::enable_if_t<std::is_copy_constructible_v<std::remove_const_t<First>> && (std::is_move_constructible_v<std::remove_const_t<First>> && !std::is_reference_v<First>)>,
    std::enable_if_t<!std::is_copy_constructible_v<std::remove_const_t<Second>> && (std::is_move_constructible_v<std::remove_const_t<Second>> && !std::is_reference_v<Second>)>
>
{
    First   first;
    Second  second;

    constexpr Pair()                            = default;

    constexpr Pair(const First &first, Second &&second)
        : first(first), second(std::move(second)) {}

    constexpr Pair(First &&first, Second &&second)
        : first(std::move(first)), second(std::move(second)) {}

    constexpr Pair(const Pair &other)       = default;
    Pair &operator=(const Pair &other)      = default;

    constexpr Pair(Pair &&other) noexcept   = default;
    Pair &operator=(Pair &&other) noexcept  = default;

    ~Pair()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const Pair &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const Pair &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const Pair &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const Pair &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const Pair &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const Pair &other) const
        { return !operator==(other); }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(first);
        hc.Add(second);

        return hc;
    }
};

template <class First, class Second>
struct Pair<
    First, Second,
    std::enable_if_t<!std::is_copy_constructible_v<std::remove_const_t<First>> && (std::is_move_constructible_v<std::remove_const_t<First>> && !std::is_reference_v<First>)>,
    std::enable_if_t<std::is_copy_constructible_v<std::remove_const_t<Second>> && !(std::is_move_constructible_v<std::remove_const_t<Second>> && !std::is_reference_v<Second>)>
>
{
    First   first;
    Second  second;

    constexpr Pair()                            = default;

    constexpr Pair(First &&first, const Second &second)
        : first(std::move(first)), second(second) {}

    constexpr Pair(const Pair &other)       = default;
    Pair &operator=(const Pair &other)      = default;

    constexpr Pair(Pair &&other) noexcept   = default;
    Pair &operator=(Pair &&other) noexcept  = default;

    ~Pair()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const Pair &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const Pair &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const Pair &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const Pair &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const Pair &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const Pair &other) const
        { return !operator==(other); }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(first);
        hc.Add(second);

        return hc;
    }
};

template <class First, class Second>
struct Pair<
    First, Second,
    std::enable_if_t<std::is_copy_constructible_v<std::remove_const_t<First>> && !(std::is_move_constructible_v<std::remove_const_t<First>> && !std::is_reference_v<First>)>,
    std::enable_if_t<!std::is_copy_constructible_v<std::remove_const_t<Second>> && (std::is_move_constructible_v<std::remove_const_t<Second>> && !std::is_reference_v<Second>)>
>
{
    First   first;
    Second  second;

    constexpr Pair()                            = default;

    constexpr Pair(const First &first, Second &&second)
        : first(first), second(std::move(second)) {}

    constexpr Pair(const Pair &other)       = default;
    Pair &operator=(const Pair &other)      = default;

    constexpr Pair(Pair &&other) noexcept   = default;
    Pair &operator=(Pair &&other) noexcept  = default;

    ~Pair()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const Pair &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const Pair &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const Pair &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const Pair &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const Pair &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const Pair &other) const
        { return !operator==(other); }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(first);
        hc.Add(second);

        return hc;
    }
};

template <class First, class Second>
struct Pair<
    First, Second,
    std::enable_if_t<std::is_copy_constructible_v<std::remove_const_t<First>> && !(std::is_move_constructible_v<std::remove_const_t<First>> && !std::is_reference_v<First>)>,
    std::enable_if_t<std::is_copy_constructible_v<std::remove_const_t<Second>> && !(std::is_move_constructible_v<std::remove_const_t<Second>> && !std::is_reference_v<Second>)>
>
{
    First   first;
    Second  second;

    constexpr Pair()                        = default;

    constexpr Pair(const First &first, const Second &second)
        : first(first), second(second) {}

    constexpr Pair(const Pair &other)       = default;
    Pair &operator=(const Pair &other)      = default;

    constexpr Pair(Pair &&other) noexcept   = default;
    Pair &operator=(Pair &&other) noexcept  = default;

    ~Pair()                                 = default;

    HYP_FORCE_INLINE
    bool operator<(const Pair &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const Pair &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const Pair &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const Pair &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const Pair &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const Pair &other) const
        { return !operator==(other); }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(first);
        hc.Add(second);

        return hc;
    }
};

template <class First, class Second>
struct Pair<
    First, Second,
    std::enable_if_t<!std::is_copy_constructible_v<std::remove_const_t<First>> && (std::is_move_constructible_v<std::remove_const_t<First>> && !std::is_reference_v<First>)>,
    std::enable_if_t<!std::is_copy_constructible_v<std::remove_const_t<Second>> && (std::is_move_constructible_v<std::remove_const_t<Second>> && !std::is_reference_v<Second>)>
>
{
    First   first;
    Second  second;

    constexpr Pair()                            = default;

    constexpr Pair(First &&first, Second &&second)
        : first(std::move(first)), second(std::move(second)) {}

    constexpr Pair(const Pair &other)       = default;
    Pair &operator=(const Pair &other)      = default;

    constexpr Pair(Pair &&other) noexcept   = default;
    Pair &operator=(Pair &&other) noexcept  = default;

    ~Pair()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const Pair &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const Pair &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const Pair &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const Pair &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const Pair &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const Pair &other) const
        { return !operator==(other); }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(first);
        hc.Add(second);

        return hc;
    }
};

// // deduction guide
// template <typename First, typename Second>
// Pair(First, Second) -> Pair<
//     (std::is_copy_constructible_v<NormalizedType<First>> ? utilities::detail::COPY_CONSTRUCTIBLE : 0) | (std::is_move_constructible_v<NormalizedType<First>> ? utilities::detail::MOVE_CONSTRUCTIBLE : 0),
//     (std::is_copy_constructible_v<NormalizedType<Second>> ? utilities::detail::COPY_CONSTRUCTIBLE : 0) | (std::is_move_constructible_v<NormalizedType<Second>> ? utilities::detail::MOVE_CONSTRUCTIBLE : 0),
//     NormalizedType<First>, NormalizedType<Second>
// >;

} // namespace detail

template <class First, class Second>
using Pair = utilities::detail::Pair<First, Second>;


template <class Key, class Value>
struct KeyValuePair : public Pair<Key, Value>
{
    using Base = Pair<Key, Value>;

    KeyValuePair() : Pair<Key, Value>() {}

    KeyValuePair(const Key &key, const Value &value) : Pair<Key, Value>(key, value) {}
    KeyValuePair(const Key &key, Value &&value) : Pair<Key, Value>(key, std::move(value)) {}
    KeyValuePair(Key &&key, const Value &value) : Pair<Key, Value>(std::move(key), value) {}
    KeyValuePair(Key &&key, Value &&value) : Pair<Key, Value>(std::move(key), std::move(value)) {}

    KeyValuePair(const KeyValuePair &other) : Pair<Key, Value>(other) {}
    KeyValuePair(KeyValuePair &&other) noexcept : Pair<Key, Value>(std::move(other)) {}

    HYP_FORCE_INLINE
    KeyValuePair &operator=(const KeyValuePair &other)
    {
        Pair<Key, Value>::first = other.first;
        Pair<Key, Value>::second = other.second;

        return *this;
    }

    HYP_FORCE_INLINE
    KeyValuePair &operator=(KeyValuePair &&other) noexcept
    {
        Pair<Key, Value>::first = std::move(other.first);
        Pair<Key, Value>::second = std::move(other.second);

        return *this;
    }

    KeyValuePair(const Pair<Key, Value> &pair) : Pair<Key, Value>(pair) {}
    KeyValuePair(Pair<Key, Value> &&pair) noexcept : Pair<Key, Value>(std::move(pair)) {}

    HYP_FORCE_INLINE
    KeyValuePair &operator=(const Pair<Key, Value> &other)
    {
        Pair<Key, Value>::first = other.first;
        Pair<Key, Value>::second = other.second;

        return *this;
    }

    HYP_FORCE_INLINE
    KeyValuePair &operator=(Pair<Key, Value> &&other) noexcept
    {
        Pair<Key, Value>::first = std::move(other.first);
        Pair<Key, Value>::second = std::move(other.second);

        return *this;
    }

    ~KeyValuePair() = default;

    HYP_FORCE_INLINE
    bool operator==(const KeyValuePair &other) const
    {
        return Pair<Key, Value>::first == other.first
            && Pair<Key, Value>::second == other.second;
    }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const
        { return Pair<Key, Value>::GetHashCode(); }
};

template <class K0, class V0, class K1, class V1>
bool operator<(const KeyValuePair<K0, V0> &lhs, const Pair<K1, V1> &rhs) { return lhs.first < rhs.first; }

template <class K0, class V0, class K1, class V1>
bool operator<=(const KeyValuePair<K0, V0> &lhs, const Pair<K1, V1> &rhs) { return lhs.first <= rhs.first; }

template <class K0, class V0, class K1, class V1>
bool operator>(const KeyValuePair<K0, V0> &lhs, const Pair<K1, V1> &rhs) { return lhs.first > rhs.first; }

template <class K0, class V0, class K1, class V1>
bool operator>=(const KeyValuePair<K0, V0> &lhs, const Pair<K1, V1> &rhs) { return lhs.first >= rhs.first; }

template <class K0, class V0, class K1, class V1>
bool operator<(const Pair<K0, V0> &lhs, const KeyValuePair<K1, V1> &rhs) { return lhs.first < rhs.first; }

template <class K0, class V0, class K1, class V1>
bool operator<=(const Pair<K0, V0> &lhs, const KeyValuePair<K1, V1> &rhs) { return lhs.first <= rhs.first; }

template <class K0, class V0, class K1, class V1>
bool operator>(const Pair<K0, V0> &lhs, const KeyValuePair<K1, V1> &rhs) { return lhs.first > rhs.first; }

template <class K0, class V0, class K1, class V1>
bool operator>=(const Pair<K0, V0> &lhs, const KeyValuePair<K1, V1> &rhs) { return lhs.first >= rhs.first; }

template <class K, class V>
bool operator<(const K &lhs, const KeyValuePair<K, V> &rhs) { return lhs < rhs.first; }

template <class K, class V>
bool operator<=(const K &lhs, const KeyValuePair<K, V> &rhs) { return lhs <= rhs.first; }

template <class K, class V>
bool operator>(const K &lhs, const KeyValuePair<K, V> &rhs) { return lhs > rhs.first; }

template <class K, class V>
bool operator>=(const K &lhs, const KeyValuePair<K, V> &rhs) { return lhs >= rhs.first; }

template <class K, class V, class T>
bool operator<(const KeyValuePair<K, V> &lhs, const T &rhs) { return lhs.first < rhs; }

template <class K, class V, class T>
bool operator<=(const KeyValuePair<K, V> &lhs, const T &rhs) { return lhs.first <= rhs; }

template <class K, class V, class T>
bool operator>(const KeyValuePair<K, V> &lhs, const T &rhs) { return lhs.first > rhs; }

template <class K, class V, class T>
bool operator>=(const KeyValuePair<K, V> &lhs, const T &rhs) { return lhs.first >= rhs; }

} // namespace utilities

template <class K, class V>
using KeyValuePair = utilities::KeyValuePair<K, V>;

template <class K, class V>
using Pair = utilities::Pair<K, V>;

} // namespace hyperion

#endif