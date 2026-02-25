/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/Array.hpp>
#include <Core/utilities/Pair.hpp>
#include <Core/containers/ContainerBase.hpp>

#include <Core/HashCode.hpp>

#include <algorithm>
#include <utility>

namespace Hyperion {
namespace containers {

/*! \brief Super basic map type - linear array of key-value pairs, not sorted or hashed in any way.
 *  Insertion order is preserved. Not suitable for large datasets, but very useful for small maps
 *  and when insertion order matters. */
template <class Key, class Value>
class ArrayMap : public Array<KeyValuePair<Key, Value>, DynamicAllocator>
{
public:
    using KeyValuePairType = KeyValuePair<Key, Value>;

public:
    using Base = Array<KeyValuePair<Key, Value>, DynamicAllocator>;

    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;
    using InsertResult = typename Base::InsertResult;

    using KeyType = Key;
    using ValueType = Value;

    ArrayMap();

    ArrayMap(std::initializer_list<KeyValuePairType> initializerList)
        : Base(initializerList)
    {
    }

    ArrayMap(const ArrayMap& other);
    ArrayMap& operator=(const ArrayMap& other);

    ArrayMap(ArrayMap&& other) noexcept;
    ArrayMap& operator=(ArrayMap&& other) noexcept;

    ~ArrayMap();

    Iterator Find(const Key& key);
    ConstIterator Find(const Key& key) const;

    template <class TFindAsType>
    auto FindAs(const TFindAsType& key) -> Iterator
    {
        for (auto it = Base::Begin(); it != Base::End(); ++it)
        {
            if (it->first == key)
            {
                return it;
            }
        }

        return Base::End();
    }

    template <class TFindAsType>
    auto FindAs(const TFindAsType& key) const -> ConstIterator
    {
        for (auto it = Base::Begin(); it != Base::End(); ++it)
        {
            if (it->first == key)
            {
                return it;
            }
        }

        return Base::End();
    }

    bool Contains(const Key& key) const;

    InsertResult Insert(const Key& key, const Value& value);
    InsertResult Insert(const Key& key, Value&& value);
    InsertResult Insert(Pair<Key, Value>&& pair);

    InsertResult Set(const Key& key, const Value& value);
    InsertResult Set(const Key& key, Value&& value);
    InsertResult Set(Iterator iter, const Value& value);
    InsertResult Set(Iterator iter, Value&& value);

    template <class... Args>
    InsertResult Emplace(const Key& key, Args&&... args)
    {
        return Insert(key, Value(std::forward<Args>(args)...));
    }

    Iterator Erase(ConstIterator it);
    bool Erase(const Key& key);

    HYP_FORCE_INLINE Value& operator[](const Key& key)
    {
        const auto it = Find(key);

        if (it != Base::End())
        {
            return it->second;
        }

        return Insert(key, Value {}).first->second;
    }

    template <class OtherContainerType>
    ArrayMap& Merge(const OtherContainerType& other)
    {
        if (Base::Empty())
        {
            *this = other;
        }
        else
        {
            for (const auto& item : other)
            {
                Set(item.first, item.second);
            }
        }

        return *this;
    }

    template <class OtherContainerType>
    ArrayMap& Merge(OtherContainerType&& other)
    {
        if (Base::Empty())
        {
            *this = std::forward<OtherContainerType>(other);
        }
        else
        {
            for (auto& item : other)
            {
                Set(item.first, std::move(item.second));
            }
        }

        other.Clear();

        return *this;
    }
};

template <class Key, class Value>
ArrayMap<Key, Value>::ArrayMap()
    : Base()
{
}

template <class Key, class Value>
ArrayMap<Key, Value>::ArrayMap(const ArrayMap& other)
    : Base(static_cast<const Base&>(other))
{
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::operator=(const ArrayMap& other) -> ArrayMap&
{
    Base::operator=(static_cast<const Base&>(other));

    return *this;
}

template <class Key, class Value>
ArrayMap<Key, Value>::ArrayMap(ArrayMap&& other) noexcept
    : Base(static_cast<Base&&>(other))
{
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::operator=(ArrayMap&& other) noexcept -> ArrayMap&
{
    Base::operator=(static_cast<Base&&>(other));

    return *this;
}

template <class Key, class Value>
ArrayMap<Key, Value>::~ArrayMap() = default;

template <class Key, class Value>
auto ArrayMap<Key, Value>::Find(const Key& key) -> Iterator
{
    for (auto it = Base::Begin(); it != Base::End(); ++it)
    {
        if (it->first == key)
        {
            return it;
        }
    }

    return Base::End();
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::Find(const Key& key) const -> ConstIterator
{
    for (auto it = Base::Begin(); it != Base::End(); ++it)
    {
        if (it->first == key)
        {
            return it;
        }
    }

    return Base::End();
}

template <class Key, class Value>
bool ArrayMap<Key, Value>::Contains(const Key& key) const
{
    return Find(key) != Base::End();
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::Insert(const Key& key, const Value& value) -> InsertResult
{
    auto it = Find(key);

    if (it == Base::End())
    {
        Base::PushBack({ key, value });

        return { Base::Begin() + (Base::Size() - 1), true };
    }

    return { it, false };
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::Insert(const Key& key, Value&& value) -> InsertResult
{
    auto it = Find(key);

    if (it == Base::End())
    {
        Base::PushBack({ key, std::move(value) });

        return { Base::Begin() + (Base::Size() - 1), true };
    }

    return { it, false };
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::Insert(Pair<Key, Value>&& pair) -> InsertResult
{
    auto it = Find(pair.first);

    if (it == Base::End())
    {
        Base::PushBack(std::move(pair));

        return { Base::Begin() + (Base::Size() - 1), true };
    }

    return { it, false };
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::Set(const Key& key, const Value& value) -> InsertResult
{
    auto it = Find(key);

    if (it == Base::End())
    {
        Base::PushBack({ key, value });

        return { Base::Begin() + (Base::Size() - 1), true };
    }

    it->second = value;

    return { it, true };
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::Set(const Key& key, Value&& value) -> InsertResult
{
    auto it = Find(key);

    if (it == Base::End())
    {
        Base::PushBack({ key, std::move(value) });

        return { Base::Begin() + (Base::Size() - 1), true };
    }

    it->second = std::move(value);

    return { it, true };
}

template <class Key, class Value>
auto ArrayMap<Key, Value>::Erase(ConstIterator it) -> Iterator
{
    return Base::Erase(it);
}

template <class Key, class Value>
bool ArrayMap<Key, Value>::Erase(const Key& value)
{
    return Erase(Find(value)) != Base::End();
}
} // namespace containers

template <class Key, class Value>
using ArrayMap = containers::ArrayMap<Key, Value>;

template <class Key, class Value>
struct IsArrayMap<containers::ArrayMap<Key, Value>> : std::true_type
{
};

} // namespace Hyperion
