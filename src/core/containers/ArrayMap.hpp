/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ARRAY_MAP_HPP
#define HYPERION_ARRAY_MAP_HPP

#include <core/containers/Array.hpp>
#include <core/utilities/Pair.hpp>
#include <core/containers/ContainerBase.hpp>

#include <HashCode.hpp>

#include <algorithm>
#include <utility>

namespace hyperion {
namespace containers {

// A flat, unordered map. Stored internally as an array, uses linear search to find items
// Useful for when there are only a small number of items to loop through,
// as the CPU will have an easier time caching this.

template <class Key, class Value>
class ArrayMap : public ContainerBase<ArrayMap<Key, Value>, Key>
{
public:
    using KeyValuePairType = KeyValuePair<Key, Value>;

private:
    Array<KeyValuePairType> m_vector;

public:
    static constexpr bool is_contiguous = true;

    using Base = ContainerBase<ArrayMap<Key, Value>, Key>;

    using KeyType = Key;
    using ValueType = Value;

    using Iterator = typename decltype(m_vector)::Iterator;
    using ConstIterator = typename decltype(m_vector)::ConstIterator;
    using InsertResult = std::pair<Iterator, bool>; // iterator, was inserted

    ArrayMap();
    ArrayMap(std::initializer_list<KeyValuePairType> initializer_list)
        : m_vector(initializer_list)
    {
    }

    ArrayMap(const ArrayMap &other);
    ArrayMap &operator=(const ArrayMap &other);
    ArrayMap(ArrayMap &&other) noexcept;
    ArrayMap &operator=(ArrayMap &&other) noexcept;
    ~ArrayMap();
    
    [[nodiscard]] Iterator Find(const Key &key);
    [[nodiscard]] ConstIterator Find(const Key &key) const;

    [[nodiscard]] bool Contains(const Key &key) const;

    InsertResult Insert(const Key &key, const Value &value);
    InsertResult Insert(const Key &key, Value &&value);
    InsertResult Insert(Pair<Key, Value> &&pair);

    InsertResult Set(const Key &key, const Value &value);
    InsertResult Set(const Key &key, Value &&value);
    InsertResult Set(Iterator iter, const Value &value);
    InsertResult Set(Iterator iter, Value &&value);

    template <class ...Args>
    InsertResult Emplace(const Key &key, Args &&... args)
        { return Insert(key, Value(std::forward<Args>(args)...)); }

    Iterator Erase(ConstIterator it);
    bool Erase(const Key &key);

    HYP_FORCE_INLINE SizeType Size() const
        { return m_vector.Size(); }

    HYP_FORCE_INLINE KeyValuePairType *Data()
        { return m_vector.Data(); }

    HYP_FORCE_INLINE KeyValuePairType * const Data() const
        { return m_vector.Data(); }

    HYP_FORCE_INLINE bool Any() const
        { return m_vector.Any(); }

    HYP_FORCE_INLINE bool Empty() const
        { return m_vector.Empty(); }

    HYP_FORCE_INLINE void Clear()
        { m_vector.Clear(); }
    
    HYP_FORCE_INLINE KeyValuePairType &Front()
        { return m_vector.Front(); }

    HYP_FORCE_INLINE const KeyValuePairType &Front() const
        { return m_vector.Front(); }

    HYP_FORCE_INLINE KeyValuePairType &Back()
        { return m_vector.Back(); }

    HYP_FORCE_INLINE const KeyValuePairType &Back() const
        { return m_vector.Back(); }

    template <class Lambda>
    HYP_FORCE_INLINE bool Any(Lambda &&lambda) const
        { return Base::Any(std::forward<Lambda>(lambda)); }

    template <class Lambda>
    HYP_FORCE_INLINE bool Every(Lambda &&lambda) const
        { return Base::Every(std::forward<Lambda>(lambda)); }

    HYP_FORCE_INLINE Value &operator[](const Key &key)
    {
        const auto it = Find(key);

        if (it != End()) {
            return it->second;
        }

        return Insert(key, Value { }).first->second;
    }

    HYP_DEF_STL_BEGIN_END(
        m_vector.Begin(),
        m_vector.End()
    )
};

template <class Key, class Value>
ArrayMap<Key, Value>::ArrayMap() {}

template <class Key, class Value>
ArrayMap<Key, Value>::ArrayMap(const ArrayMap &other)
    : m_vector(other.m_vector)
{
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::operator=(const ArrayMap &other) -> ArrayMap &
{
    m_vector = other.m_vector;

    return *this;
}

template <class Key, class Value>
ArrayMap<Key, Value>::ArrayMap(ArrayMap &&other) noexcept
    : m_vector(std::move(other.m_vector))
{
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::operator=(ArrayMap &&other) noexcept -> ArrayMap &
{
    m_vector = std::move(other.m_vector);

    return *this;
}

template <class Key, class Value>
ArrayMap<Key, Value>::~ArrayMap() = default;

template <class Key, class Value>
auto ArrayMap<Key, Value>::Find(const Key &key) -> Iterator
{
    for (auto it = Begin(); it != End(); ++it) {
        if (it->first == key) {
            return it;
        }
    }

    return End();
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::Find(const Key &key) const -> ConstIterator
{
    for (auto it = Begin(); it != End(); ++it) {
        if (it->first == key) {
            return it;
        }
    }

    return End();
}

template <class Key, class Value>
bool ArrayMap<Key, Value>::Contains(const Key &key) const
{
    return Find(key) != End();
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::Insert(const Key &key, const Value &value) -> InsertResult
{
    auto it = Find(key);

    if (it == End()) {
        m_vector.PushBack({ key, value });

        return { m_vector.Begin() + (m_vector.Size() - 1), true };
    }

    return { it, false };
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::Insert(const Key &key, Value &&value) -> InsertResult
{
    auto it = Find(key);

    if (it == End()) {
        m_vector.PushBack({ key, std::move(value) });

        return { m_vector.Begin() + (m_vector.Size() - 1), true };
    }

    return { it, false };
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::Insert(Pair<Key, Value> &&pair) -> InsertResult
{
    auto it = Find(pair.first);

    if (it == End()) {
        m_vector.PushBack(std::move(pair));

        return { m_vector.Begin() + (m_vector.Size() - 1), true };
    }

    return { it, false };
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::Set(const Key &key, const Value &value) -> InsertResult
{
    auto it = Find(key);

    if (it == End()) {
        m_vector.PushBack({ key, value });

        return { m_vector.Begin() + (m_vector.Size() - 1), true };
    }

    it->second = value;

    return { it, true };
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::Set(const Key &key, Value &&value) -> InsertResult
{
    auto it = Find(key);

    if (it == End()) {
        m_vector.PushBack({ key, std::move(value) });

        return { m_vector.Begin() + (m_vector.Size() - 1), true };
    }

    it->second = std::move(value);

    return { it, true };
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::Erase(ConstIterator it) -> Iterator
{
    if (it == End()) {
        return End();
    }

    return m_vector.Erase(it);
}

template <class Key, class Value>
bool ArrayMap<Key, Value>::Erase(const Key &value)
{
    return Erase(Find(value)) != End();
}
} // namespace containers

template <class Key, class Value>
using ArrayMap = containers::ArrayMap<Key, Value>;

} // namespace hyperion

#endif
