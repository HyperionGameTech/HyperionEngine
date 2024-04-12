#ifndef HYPERION_V2_LIB_PAIR_H
#define HYPERION_V2_LIB_PAIR_H

#include <system/Debug.hpp>
#include <core/lib/Traits.hpp>
#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>

#include <type_traits>

namespace hyperion {

namespace containers {
namespace detail {

using PairArgTraits = uint32;

enum PairArgTrait : PairArgTraits
{
    NONE                    = 0x0,
    DEFAULT_CONSTRUCTIBLE   = 0x1,
    COPY_CONSTRUCTIBLE      = 0x2,
    COPY_ASSIGNABLE         = 0x4,
    MOVE_CONSTRUCTIBLE      = 0x8,
    MOVE_ASSIGNABLE         = 0x10
};

#define PAIR_ARG_COPY_CONSTRUCTIBLE(x) (x & PairArgTrait::COPY_CONSTRUCTIBLE)
#define PAIR_ARG_COPY_ASSIGNABLE(x) (x & PairArgTrait::COPY_ASSIGNABLE)
#define PAIR_ARG_MOVE_CONSTRUCTIBLE(x) (x & PairArgTrait::MOVE_CONSTRUCTIBLE)
#define PAIR_ARG_MOVE_ASSIGNABLE(x) (x & PairArgTrait::MOVE_ASSIGNABLE)

template <class First, class Second>
struct PairHelper
{
    static constexpr bool default_constructible = (std::is_default_constructible_v<First> && (std::is_default_constructible_v<Second>));
    static constexpr bool copy_constructible = (std::is_copy_constructible_v<First> && (std::is_copy_constructible_v<Second>));
    static constexpr bool copy_assignable = (std::is_copy_assignable_v<First> && (std::is_copy_assignable_v<Second>));
    static constexpr bool move_constructible = (std::is_move_constructible_v<First> && (std::is_move_constructible_v<Second>));
    static constexpr bool move_assignable = (std::is_move_assignable_v<First> && (std::is_move_assignable_v<Second>));
};

template <PairArgTraits FirstTraits, PairArgTraits SecondTraits, class First, class Second>
struct PairBase :
    private ConstructAssignmentTraits<
        PairHelper<First, Second>::default_constructible,
        (FirstTraits & COPY_CONSTRUCTIBLE) && (SecondTraits & COPY_CONSTRUCTIBLE),
        (FirstTraits & MOVE_CONSTRUCTIBLE) && (SecondTraits & MOVE_CONSTRUCTIBLE),
        PairBase<FirstTraits, SecondTraits, First, Second>
    >
{
};

/*! \brief Simple Pair class that is also able to hold a reference to a key or value */
template <PairArgTraits FirstTraits, PairArgTraits SecondTraits, class First, class Second>
struct PairImpl :
    PairBase<
        FirstTraits, SecondTraits,
        First, Second
    >
{
    static_assert(resolution_failure<PairImpl>, "Invalid PairImpl traits");
};

template <class First, class Second>
struct PairImpl<COPY_CONSTRUCTIBLE | MOVE_CONSTRUCTIBLE, COPY_CONSTRUCTIBLE | MOVE_CONSTRUCTIBLE, First, Second> :
    PairBase<
        COPY_CONSTRUCTIBLE | MOVE_CONSTRUCTIBLE, COPY_CONSTRUCTIBLE | MOVE_CONSTRUCTIBLE,
        First, Second
    >
{
    First   first;
    Second  second;

    PairImpl()                                      = default;

    PairImpl(const First &first, const Second &second)
        : first(first), second(second) {}

    PairImpl(First &&first, const Second &second)
        : first(std::move(first)), second(second) {}

    PairImpl(const First &first, Second &&second)
        : first(first), second(std::move(second)) {}

    PairImpl(First &&first, Second &&second)
        : first(std::move(first)), second(std::move(second)) {}

    PairImpl(const PairImpl &other)                 = default;
    PairImpl &operator=(const PairImpl &other)      = default;

    PairImpl(PairImpl &&other) noexcept             = default;
    PairImpl &operator=(PairImpl &&other) noexcept  = default;

    ~PairImpl()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const PairImpl &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const PairImpl &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const PairImpl &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const PairImpl &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const PairImpl &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const PairImpl &other) const
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
struct PairImpl<MOVE_CONSTRUCTIBLE, COPY_CONSTRUCTIBLE | MOVE_CONSTRUCTIBLE, First, Second> :
    PairBase<
        MOVE_CONSTRUCTIBLE, COPY_CONSTRUCTIBLE | MOVE_CONSTRUCTIBLE,
        First, Second
    >
{
    First   first;
    Second  second;

    PairImpl()                                      = default;

    PairImpl(First &&first, const Second &second)
        : first(std::move(first)), second(second) {}

    PairImpl(First &&first, Second &&second)
        : first(std::move(first)), second(std::move(second)) {}

    PairImpl(const PairImpl &other)                 = default;
    PairImpl &operator=(const PairImpl &other)      = default;

    PairImpl(PairImpl &&other) noexcept             = default;
    PairImpl &operator=(PairImpl &&other) noexcept  = default;

    ~PairImpl()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const PairImpl &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const PairImpl &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const PairImpl &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const PairImpl &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const PairImpl &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const PairImpl &other) const
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
struct PairImpl<COPY_CONSTRUCTIBLE | MOVE_CONSTRUCTIBLE, COPY_CONSTRUCTIBLE, First, Second> :
    PairBase<
        COPY_CONSTRUCTIBLE | MOVE_CONSTRUCTIBLE, COPY_CONSTRUCTIBLE,
        First, Second
    >
{
    First   first;
    Second  second;

    PairImpl()                                      = default;

    PairImpl(const First &first, const Second &second)
        : first(first), second(second) {}

    PairImpl(First &&first, const Second &second)
        : first(std::move(first)), second(second) {}

    PairImpl(const PairImpl &other)                 = default;
    PairImpl &operator=(const PairImpl &other)      = default;

    PairImpl(PairImpl &&other) noexcept             = default;
    PairImpl &operator=(PairImpl &&other) noexcept  = default;

    ~PairImpl()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const PairImpl &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const PairImpl &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const PairImpl &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const PairImpl &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const PairImpl &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const PairImpl &other) const
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
struct PairImpl<COPY_CONSTRUCTIBLE, COPY_CONSTRUCTIBLE | MOVE_CONSTRUCTIBLE, First, Second> :
    PairBase<
        COPY_CONSTRUCTIBLE, COPY_CONSTRUCTIBLE | MOVE_CONSTRUCTIBLE,
        First, Second
    >
{
    First   first;
    Second  second;

    PairImpl()                                      = default;

    PairImpl(const First &first, const Second &second)
        : first(first), second(second) {}

    PairImpl(const First &first, Second &&second)
        : first(first), second(std::move(second)) {}

    PairImpl(const PairImpl &other)                 = default;
    PairImpl &operator=(const PairImpl &other)      = default;

    PairImpl(PairImpl &&other) noexcept             = default;
    PairImpl &operator=(PairImpl &&other) noexcept  = default;

    ~PairImpl()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const PairImpl &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const PairImpl &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const PairImpl &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const PairImpl &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const PairImpl &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const PairImpl &other) const
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
struct PairImpl<COPY_CONSTRUCTIBLE | MOVE_CONSTRUCTIBLE, MOVE_CONSTRUCTIBLE, First, Second> :
    PairBase<
        COPY_CONSTRUCTIBLE | MOVE_CONSTRUCTIBLE, MOVE_CONSTRUCTIBLE,
        First, Second
    >
{
    First   first;
    Second  second;

    PairImpl()                                      = default;

    PairImpl(const First &first, Second &&second)
        : first(first), second(std::move(second)) {}

    PairImpl(First &&first, Second &&second)
        : first(std::move(first)), second(std::move(second)) {}

    PairImpl(const PairImpl &other)                 = default;
    PairImpl &operator=(const PairImpl &other)      = default;

    PairImpl(PairImpl &&other) noexcept             = default;
    PairImpl &operator=(PairImpl &&other) noexcept  = default;

    ~PairImpl()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const PairImpl &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const PairImpl &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const PairImpl &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const PairImpl &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const PairImpl &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const PairImpl &other) const
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
struct PairImpl<MOVE_CONSTRUCTIBLE, COPY_CONSTRUCTIBLE, First, Second> :
    PairBase<
        MOVE_CONSTRUCTIBLE, COPY_CONSTRUCTIBLE,
        First, Second
    >
{
    First   first;
    Second  second;

    PairImpl()                                      = default;

    PairImpl(First &&first, const Second &second)
        : first(std::move(first)), second(second) {}

    PairImpl(const PairImpl &other)                 = default;
    PairImpl &operator=(const PairImpl &other)      = default;

    PairImpl(PairImpl &&other) noexcept             = default;
    PairImpl &operator=(PairImpl &&other) noexcept  = default;

    ~PairImpl()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const PairImpl &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const PairImpl &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const PairImpl &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const PairImpl &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const PairImpl &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const PairImpl &other) const
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
struct PairImpl<COPY_CONSTRUCTIBLE, MOVE_CONSTRUCTIBLE, First, Second> :
    PairBase<
        COPY_CONSTRUCTIBLE, MOVE_CONSTRUCTIBLE,
        First, Second
    >
{
    First   first;
    Second  second;

    PairImpl()                                      = default;

    PairImpl(const First &first, Second &&second)
        : first(first), second(std::move(second)) {}

    PairImpl(const PairImpl &other)                 = default;
    PairImpl &operator=(const PairImpl &other)      = default;

    PairImpl(PairImpl &&other) noexcept             = default;
    PairImpl &operator=(PairImpl &&other) noexcept  = default;

    ~PairImpl()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const PairImpl &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const PairImpl &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const PairImpl &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const PairImpl &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const PairImpl &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const PairImpl &other) const
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
struct PairImpl<COPY_CONSTRUCTIBLE, COPY_CONSTRUCTIBLE, First, Second> :
    PairBase<
        COPY_CONSTRUCTIBLE, COPY_CONSTRUCTIBLE,
        First, Second
    >
{
    First   first;
    Second  second;

    PairImpl()                                      = default;

    PairImpl(const First &first, const Second &second)
        : first(first), second(second) {}

    PairImpl(const PairImpl &other)                 = default;
    PairImpl &operator=(const PairImpl &other)      = default;

    PairImpl(PairImpl &&other) noexcept             = default;
    PairImpl &operator=(PairImpl &&other) noexcept  = default;

    ~PairImpl()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const PairImpl &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const PairImpl &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const PairImpl &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const PairImpl &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const PairImpl &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const PairImpl &other) const
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
struct PairImpl<MOVE_CONSTRUCTIBLE, MOVE_CONSTRUCTIBLE, First, Second> :
    PairBase<
        MOVE_CONSTRUCTIBLE, MOVE_CONSTRUCTIBLE,
        First, Second
    >
{
    First   first;
    Second  second;

    PairImpl()                                      = default;

    PairImpl(First &&first, Second &&second)
        : first(std::move(first)), second(std::move(second)) {}

    PairImpl(const PairImpl &other)                 = default;
    PairImpl &operator=(const PairImpl &other)      = default;

    PairImpl(PairImpl &&other) noexcept             = default;
    PairImpl &operator=(PairImpl &&other) noexcept  = default;

    ~PairImpl()                                     = default;

    HYP_FORCE_INLINE
    bool operator<(const PairImpl &other) const
        { return first < other.first || (!(other.first < first) && second < other.second); }

    HYP_FORCE_INLINE
    bool operator<=(const PairImpl &other) const
        { return operator<(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator>(const PairImpl &other) const
        { return !(operator<(other) || operator==(other)); }

    HYP_FORCE_INLINE
    bool operator>=(const PairImpl &other) const
        { return operator>(other) || operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(const PairImpl &other) const
        { return first == other.first && second == other.second; }

    HYP_FORCE_INLINE
    bool operator!=(const PairImpl &other) const
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
using Pair = containers::detail::PairImpl<
    (std::is_copy_constructible_v<std::remove_const_t<First>> ? COPY_CONSTRUCTIBLE : 0) | ((std::is_move_constructible_v<std::remove_const_t<First>> && !std::is_reference_v<First>) ? MOVE_CONSTRUCTIBLE : 0),
    (std::is_copy_constructible_v<std::remove_const_t<Second>> ? COPY_CONSTRUCTIBLE : 0) | ((std::is_move_constructible_v<std::remove_const_t<Second>> && !std::is_reference_v<Second>) ? MOVE_CONSTRUCTIBLE : 0),
    std::remove_const_t<First>, std::remove_const_t<Second>
>;

// // deduction guide
// template <typename First, typename Second>
// PairImpl(First, Second) -> PairImpl<
//     (std::is_copy_constructible_v<NormalizedType<First>> ? containers::detail::COPY_CONSTRUCTIBLE : 0) | (std::is_move_constructible_v<NormalizedType<First>> ? containers::detail::MOVE_CONSTRUCTIBLE : 0),
//     (std::is_copy_constructible_v<NormalizedType<Second>> ? containers::detail::COPY_CONSTRUCTIBLE : 0) | (std::is_move_constructible_v<NormalizedType<Second>> ? containers::detail::MOVE_CONSTRUCTIBLE : 0),
//     NormalizedType<First>, NormalizedType<Second>
// >;

} // namespace detail
} // namespace containers

template <class First, class Second>
using Pair = containers::detail::Pair<First, Second>;


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
    bool operator<(const Key &key) const { return Pair<Key, Value>::first < key; }

    HYP_FORCE_INLINE
    bool operator<=(const Key &key) const { return Pair<Key, Value>::first <= key; }

    HYP_FORCE_INLINE
    bool operator>(const Key &key) const { return Pair<Key, Value>::first > key; }

    HYP_FORCE_INLINE
    bool operator>=(const Key &key) const { return Pair<Key, Value>::first >= key; }

    HYP_FORCE_INLINE
    bool operator<(const KeyValuePair &other) const { return Pair<Key, Value>::first < other.first; }

    HYP_FORCE_INLINE
    bool operator<=(const KeyValuePair &other) const { return Pair<Key, Value>::first <= other.first; }

    HYP_FORCE_INLINE
    bool operator>(const KeyValuePair &other) const { return Pair<Key, Value>::first > other.first; }

    HYP_FORCE_INLINE
    bool operator>=(const KeyValuePair &other) const { return Pair<Key, Value>::first >= other.first; }

    HYP_FORCE_INLINE
    bool operator==(const KeyValuePair &other) const
    {
        return Pair<Key, Value>::first == other.first
            && Pair<Key, Value>::second == other.second;
    }

    HYP_FORCE_INLINE
    HashCode GetHashCode() const { return Pair<Key, Value>::GetHashCode(); }
};

template <class K, class V>
bool operator<(const KeyValuePair<K, V> &lhs, const Pair<K, V> &rhs) { return lhs.first < rhs.first; }

template <class K, class V>
bool operator<=(const KeyValuePair<K, V> &lhs, const Pair<K, V> &rhs) { return lhs.first <= rhs.first; }

template <class K, class V>
bool operator>(const KeyValuePair<K, V> &lhs, const Pair<K, V> &rhs) { return lhs.first > rhs.first; }

template <class K, class V>
bool operator>=(const KeyValuePair<K, V> &lhs, const Pair<K, V> &rhs) { return lhs.first >= rhs.first; }

template <class K, class V>
bool operator<(const Pair<K, V> &lhs, const KeyValuePair<K, V> &rhs) { return lhs.first < rhs.first; }

template <class K, class V>
bool operator<=(const Pair<K, V> &lhs, const KeyValuePair<K, V> &rhs) { return lhs.first <= rhs.first; }

template <class K, class V>
bool operator>(const Pair<K, V> &lhs, const KeyValuePair<K, V> &rhs) { return lhs.first > rhs.first; }

template <class K, class V>
bool operator>=(const Pair<K, V> &lhs, const KeyValuePair<K, V> &rhs) { return lhs.first >= rhs.first; }

template <class K, class V>
bool operator<(const K &lhs, const KeyValuePair<K, V> &rhs) { return lhs < rhs.first; }

template <class K, class V>
bool operator<=(const K &lhs, const KeyValuePair<K, V> &rhs) { return lhs <= rhs.first; }

template <class K, class V>
bool operator>(const K &lhs, const KeyValuePair<K, V> &rhs) { return lhs > rhs.first; }

template <class K, class V>
bool operator>=(const K &lhs, const KeyValuePair<K, V> &rhs) { return lhs >= rhs.first; }

template <class K, class V>
bool operator<(const KeyValuePair<K, V> &lhs, const K &rhs) { return lhs.first < rhs; }

template <class K, class V>
bool operator<=(const KeyValuePair<K, V> &lhs, const K &rhs) { return lhs.first <= rhs; }

template <class K, class V>
bool operator>(const KeyValuePair<K, V> &lhs, const K &rhs) { return lhs.first > rhs; }

template <class K, class V>
bool operator>=(const KeyValuePair<K, V> &lhs, const K &rhs) { return lhs.first >= rhs; }


} // namespace hyperion

#endif