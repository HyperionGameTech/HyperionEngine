#ifndef HYPERION_V2_LIB_FLAT_MAP_H
#define HYPERION_V2_LIB_FLAT_MAP_H

#include "flat_set.h"

#include <algorithm>
#include <vector>
#include <utility>

namespace hyperion {

template <class Key, class Value>
class FlatMap {
    using Pair = std::pair<Key, Value>;

    FlatSet<Pair> m_set;

public:
    using Iterator      = typename FlatSet<Pair>::Iterator;
    using ConstIterator = typename FlatSet<Pair>::ConstIterator;
    using InsertResult  = std::pair<Iterator, bool>; // iterator, was inserted

    FlatMap();
    FlatMap(std::initializer_list<Pair> initializer_list)
        : m_set(initializer_list)
    {
    }

    FlatMap(const FlatMap &other);
    FlatMap &operator=(const FlatMap &other);
    FlatMap(FlatMap &&other) noexcept;
    FlatMap &operator=(FlatMap &&other) noexcept;
    ~FlatMap();
    
    [[nodiscard]] Iterator Find(const Key &key);
    [[nodiscard]] ConstIterator Find(const Key &key) const;

    [[nodiscard]] bool Contains(const Key &key) const;

    InsertResult Insert(const Key &key, const Value &value);
    InsertResult Insert(const Key &key, Value &&value);
    InsertResult Insert(std::pair<Key, Value> &&pair);

    template <class ...Args>
    InsertResult Emplace(const Key &key, Args &&... args)
        { return Insert(key, Value(std::forward<Args>(args)...)); }

    bool Erase(Iterator it);
    bool Erase(const Key &key);

    [[nodiscard]] size_t Size() const                   { return m_set.Size(); }
    [[nodiscard]] Pair *Data()                          { return m_set.Data(); }
    [[nodiscard]] Pair * const Data() const             { return m_set.Data(); }
    [[nodiscard]] bool Empty() const                    { return m_set.Empty(); }

    void Clear()                                        { m_set.Clear(); }
    
    [[nodiscard]] Pair &Front()                         { return m_set.Front(); }
    [[nodiscard]] const Pair &Front() const             { return m_set.Front(); }
    [[nodiscard]] Pair &Back()                          { return m_set.Back(); }
    [[nodiscard]] const Pair &Back() const              { return m_set.Back(); }

    [[nodiscard]] FlatSet<Key> Keys() const;
    [[nodiscard]] FlatSet<Value> Values() const;

    [[nodiscard]] Value &At(const Key &key)             { return Find(key)->second; }
    [[nodiscard]] const Value &At(const Key &key) const { return Find(key)->second; }

    [[nodiscard]] Value &operator[](const Key &key)
    {
        const auto iter = Insert(key, Value{}).first;

        return iter->second;
    }

    HYP_DEF_STL_ITERATOR(m_set)

private:
    [[nodiscard]] Iterator LowerBound(const Key &key);
    [[nodiscard]] ConstIterator LowerBound(const Key &key) const;
};

template <class Key, class Value>
FlatMap<Key, Value>::FlatMap() {}

template <class Key, class Value>
FlatMap<Key, Value>::FlatMap(const FlatMap &other)
    : m_set(other.m_set)
{
}

template <class Key, class Value>
auto FlatMap<Key, Value>::operator=(const FlatMap &other) -> FlatMap &
{
    m_set = other.m_set;

    return *this;
}

template <class Key, class Value>
FlatMap<Key, Value>::FlatMap(FlatMap &&other) noexcept
    : m_set(std::move(other.m_set))
{
}

template <class Key, class Value>
auto FlatMap<Key, Value>::operator=(FlatMap &&other) noexcept -> FlatMap&
{
    m_set = std::move(other.m_set);

    return *this;
}

template <class Key, class Value>
FlatMap<Key, Value>::~FlatMap() = default;

template <class Key, class Value>
auto FlatMap<Key, Value>::LowerBound(const Key &key) -> Iterator
{
    auto it = std::lower_bound(
        m_set.begin(),
        m_set.end(),
        key,
        [](const Pair &lhs, const Key &rhs) {
            return lhs.first < rhs;
        }
    );

    if (it == m_set.end() || it->first != key) {
        return m_set.end();
    }

    return it;
}

template <class Key, class Value>
auto FlatMap<Key, Value>::LowerBound(const Key &key) const -> ConstIterator
{
    auto it = std::lower_bound(
        m_set.cbegin(),
        m_set.cend(),
        key,
        [](const Pair &lhs, const Key &rhs) {
            return lhs.first < rhs;
        }
    );

    if (it == m_set.cend() || it->first != key) {
        return m_set.cend();
    }

    return it;
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Find(const Key &key) -> Iterator
{
    const auto it = LowerBound(key);

    if (it == End()) {
        return it;
    }

    return (it->first == key) ? it : End();
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Find(const Key &key) const -> ConstIterator
{
    const auto it = LowerBound(key);

    if (it == End()) {
        return it;
    }

    return (it->first == key) ? it : End();
}

template <class Key, class Value>
bool FlatMap<Key, Value>::Contains(const Key &key) const
{
    return Find(key) != End();
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(const Key &key, const Value &value) -> InsertResult
{
    const auto lower_bound = LowerBound(key);

    if (lower_bound == End() || !(lower_bound->first == key)) {
        return m_set.Insert(std::make_pair(key, value));
    }

    return {lower_bound, false};
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(const Key &key, Value &&value) -> InsertResult
{
    const auto lower_bound = LowerBound(key);

    if (lower_bound == End() || !(lower_bound->first == key)) {
        return m_set.Insert(std::make_pair(key, std::forward<Value>(value)));
    }

    return {lower_bound, false};
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(std::pair<Key, Value> &&pair) -> InsertResult
{
    const auto lower_bound = LowerBound(pair.first);

    if (lower_bound == End() || !(lower_bound->first == pair.first)) {
        return m_set.Insert(std::move(pair));
    }

    return {lower_bound, false};
}

template <class Key, class Value>
bool FlatMap<Key, Value>::Erase(Iterator it)
{
    if (it == End()) {
        return false;
    }

    return m_set.Erase(it);
}

template <class Key, class Value>
bool FlatMap<Key, Value>::Erase(const Key &value)
{
    return Erase(Find(value));
}

template <class Key, class Value>
FlatSet<Key> FlatMap<Key, Value>::Keys() const
{
    FlatSet<Key> keys;

    for (const auto &it : *this) {
        keys.Insert(it.first);    
    }

    return keys;
}

template <class Key, class Value>
FlatSet<Value> FlatMap<Key, Value>::Values() const
{
    FlatSet<Value> values;

    for (const auto &it : *this) {
        values.Insert(it.second);    
    }

    return values;
}

} // namespace hyperion

#endif
