#ifndef HYPERION_V2_LIB_PAIR_H
#define HYPERION_V2_LIB_PAIR_H

#include <system/Debug.hpp>
#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>

#include <utility>

namespace hyperion {

template <class First, class Second>
struct Pair {
    First first;
    Second second;

    Pair()
        : first { },
          second { }
    {
    }

    Pair(const First &first, const Second &second)
        : first(first),
          second(second)
    {
    }

    Pair(First &&first, const Second &second)
        : first(std::move(first)),
          second(second)
    {
    }

    Pair(const First &first, Second &&second)
        : first(first),
          second(std::move(second))
    {
    }

    Pair(First &&first, Second &&second)
        : first(std::forward<First>(first)),
          second(std::forward<Second>(second))
    {
    }

    Pair(const Pair &other)
        : first(other.first),
          second(other.second)
    {
    }

    Pair &operator=(const Pair &other)
    {
        first  = other.first;
        second = other.second;

        return *this;
    }

    Pair(Pair &&other) noexcept
        : first(std::move(other.first)),
          second(std::move(other.second))
    {
    }

    Pair &operator=(Pair &&other) noexcept
    {
        first  = std::move(other.first);
        second = std::move(other.second);

        return *this;
    }

    ~Pair() = default;

    bool operator<(const Pair &other) const
    {
        return first < other.first || (!(other.first < first) && second < other.second);
    }

    bool operator<=(const Pair &other) const
    {
        return operator<(other) || operator==(other);
    }

    bool operator>(const Pair &other) const
    {
        return !(operator<(other) || operator==(other));
    }

    bool operator>=(const Pair &other) const
    {
        return operator>(other) || operator==(other);
    }

    bool operator==(const Pair &other) const
    {
        return first == other.first
            && second == other.second;
    }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(first);
        hc.Add(second);

        return hc;
    }
};

template <class Key, class Value>
struct KeyValuePair : Pair<Key, Value> {
    KeyValuePair() : Pair<Key, Value>() {}
    KeyValuePair(const Key &key, const Value &value) : Pair<Key, Value>(key, value) {}
    KeyValuePair(const Key &key, Value &&value) : Pair<Key, Value>(key, std::move(value)) {}
    KeyValuePair(Key &&key, const Value &value) : Pair<Key, Value>(std::move(key), value) {}
    KeyValuePair(Key &&key, Value &&value) : Pair<Key, Value>(std::move(key), std::move(value)) {}
    KeyValuePair(const KeyValuePair &other) : Pair<Key, Value>(other) {}
    KeyValuePair(KeyValuePair &&other) : Pair<Key, Value>(std::move(other)) {}

    KeyValuePair &operator=(const KeyValuePair &other)
    {
        Pair<Key, Value>::first = other.first;
        Pair<Key, Value>::second = other.second;

        return *this;
    }

    KeyValuePair &operator=(KeyValuePair &&other) noexcept
    {
        Pair<Key, Value>::first = std::move(other.first);
        Pair<Key, Value>::second = std::move(other.second);

        return *this;
    }

    ~KeyValuePair() = default;

    bool operator<(const Key &key) const { return Pair<Key, Value>::first < key; }
    bool operator<=(const Key &key) const { return Pair<Key, Value>::first <= key; }
    bool operator>(const Key &key) const { return Pair<Key, Value>::first > key; }
    bool operator>=(const Key &key) const { return Pair<Key, Value>::first >= key; }

    bool operator<(const KeyValuePair &other) const { return Pair<Key, Value>::first < other.first; }
    bool operator<=(const KeyValuePair &other) const { return Pair<Key, Value>::first <= other.first; }
    bool operator>(const KeyValuePair &other) const { return Pair<Key, Value>::first > other.first; }
    bool operator>=(const KeyValuePair &other) const { return Pair<Key, Value>::first >= other.first; }

    bool operator==(const KeyValuePair &other) const
    {
        return Pair<Key, Value>::first == other.first
            && Pair<Key, Value>::second == other.second;
    }

    friend bool operator<(const KeyValuePair &, const Pair<Key, Value> &);
    friend bool operator<=(const KeyValuePair &, const Pair<Key, Value> &);
    friend bool operator>(const KeyValuePair &, const Pair<Key, Value> &);
    friend bool operator>=(const KeyValuePair &, const Pair<Key, Value> &);

    friend bool operator<(const Pair<Key, Value> &, const KeyValuePair &);
    friend bool operator<=(const Pair<Key, Value> &, const KeyValuePair &);
    friend bool operator>(const Pair<Key, Value> &, const KeyValuePair &);
    friend bool operator>=(const Pair<Key, Value> &, const KeyValuePair &);

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

} // namespace hyperion

#endif