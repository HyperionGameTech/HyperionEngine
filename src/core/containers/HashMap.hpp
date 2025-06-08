/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_HASH_MAP_HPP
#define HYPERION_HASH_MAP_HPP

#include <core/containers/HashSet.hpp>

#include <core/utilities/Pair.hpp>
#include <core/functional/FunctionWrapper.hpp>

#include <HashCode.hpp>

namespace hyperion {

namespace containers {

/*! \brief HashMap is a hash table based associative container that stores key-value pairs, based on the HashSet implementation.
 *  A custom allocator can be provided to control memory allocation for the nodes.
 *  \tparam Key The type of keys stored in the hash map.
 * \tparam Value The type of values stored in the hash map.
 * \tparam NodeAllocatorType The type of node allocator used for managing memory for the hash map elements. The default is `HashTable_DefaultNodeAllocator<KeyValuePair<Key, Value>>`, which can leverage pooled allocation for reduced dynamic memory allocation. If you want to use dynamic allocation (e.g need stable pointers to elements), you can use `HashTable_DynamicNodeAllocator<KeyValuePair<Key, Value>>` instead. */
template <class Key, class Value, class NodeAllocatorType = HashTable_DefaultNodeAllocator<KeyValuePair<Key, Value>>>
class HashMap : public HashSet<KeyValuePair<Key, Value>, &KeyValuePair<Key, Value>::first, NodeAllocatorType>
{
public:
    using Base = HashSet<KeyValuePair<Key, Value>, &KeyValuePair<Key, Value>::first, NodeAllocatorType>;

    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    using InsertResult = typename Base::InsertResult;

    using KeyType = Key;
    using ValueType = Value;

    using Base::Any;
    using Base::Begin;
    using Base::begin;
    using Base::cbegin;
    using Base::cend;
    using Base::Clear;
    using Base::Contains;
    using Base::Empty;
    using Base::End;
    using Base::end;
    using Base::Find;
    using Base::FindAs;
    using Base::Front;
    using Base::Reserve;
    using Base::Size;

    HashMap();

    HashMap(std::initializer_list<KeyValuePair<Key, Value>> initializer_list)
        : HashMap()
    {
        Array<KeyValuePair<Key, Value>> temp(initializer_list);

        for (auto&& item : temp)
        {
            Set(std::move(item.first), std::move(item.second));
        }
    }

    HashMap(const HashMap& other);
    HashMap& operator=(const HashMap& other);
    HashMap(HashMap&& other) noexcept;
    HashMap& operator=(HashMap&& other) noexcept;
    ~HashMap();

    Value& operator[](const Key& key);

    HYP_FORCE_INLINE Value& At(const Key& key)
    {
        const auto it = Base::Find(key);
        AssertDebugMsg(it != Base::End(), "At(): Element not found");

        return it->second;
    }

    HYP_FORCE_INLINE const Value& At(const Key& key) const
    {
        const auto it = Base::Find(key);
        AssertDebugMsg(it != Base::End(), "At(): Element not found");

        return it->second;
    }

    HYP_FORCE_INLINE bool operator==(const HashMap& other) const
    {
        if (Base::m_size != other.Base::m_size)
        {
            return false;
        }

        for (const auto& bucket : Base::m_buckets)
        {
            for (auto it = bucket.head; it != nullptr; it = it->next)
            {
                const auto other_it = other.Find(it->value.first);

                if (other_it == other.End())
                {
                    return false;
                }

                if (other_it->second != it->value.second)
                {
                    return false;
                }
            }
        }

        return true;
    }

    HYP_FORCE_INLINE bool operator!=(const HashMap& other) const
    {
        if (Base::m_size != other.Base::m_size)
        {
            return true;
        }

        for (const auto& bucket : Base::m_buckets)
        {
            for (auto it = bucket.head; it != nullptr; it = it->next)
            {
                const auto other_it = other.Find(it->value.first);

                if (other_it == other.End())
                {
                    return true;
                }

                if (other_it->second != it->value.second)
                {
                    return true;
                }
            }
        }

        return false;
    }

    Iterator FindByHashCode(HashCode hash_code);
    ConstIterator FindByHashCode(HashCode hash_code) const;

    InsertResult Set(const Key& key, const Value& value);
    InsertResult Set(const Key& key, Value&& value);
    InsertResult Set(Key&& key, const Value& value);
    InsertResult Set(Key&& key, Value&& value);

    InsertResult Insert(const Key& key, const Value& value);
    InsertResult Insert(const Key& key, Value&& value);
    InsertResult Insert(Key&& key, const Value& value);
    InsertResult Insert(Key&& key, Value&& value);
    InsertResult Insert(const KeyValuePair<Key, Value>& pair);
    InsertResult Insert(KeyValuePair<Key, Value>&& pair);

    template <class... Args>
    HYP_FORCE_INLINE InsertResult Emplace(const Key& key, Args&&... args)
    {
        return Insert(key, Value(std::forward<Args>(args)...));
    }

    template <class... Args>
    HYP_FORCE_INLINE InsertResult Emplace(Key&& key, Args&&... args)
    {
        return Insert(std::move(key), Value(std::forward<Args>(args)...));
    }

    template <class OtherContainerType>
    HYP_FORCE_INLINE HashMap& Merge(OtherContainerType&& other)
    {
        Base::Merge(std::forward<OtherContainerType>(other));

        return *this;
    }
};

template <class Key, class Value, class NodeAllocatorType>
HashMap<Key, Value, NodeAllocatorType>::HashMap()
{
}

template <class Key, class Value, class NodeAllocatorType>
HashMap<Key, Value, NodeAllocatorType>::HashMap(const HashMap& other)
    : Base(static_cast<const Base&>(other))
{
}

template <class Key, class Value, class NodeAllocatorType>
auto HashMap<Key, Value, NodeAllocatorType>::operator=(const HashMap& other) -> HashMap&
{
    Base::operator=(static_cast<const Base&>(other));

    return *this;
}

template <class Key, class Value, class NodeAllocatorType>
HashMap<Key, Value, NodeAllocatorType>::HashMap(HashMap&& other) noexcept
    : Base(static_cast<Base&&>(other))
{
}

template <class Key, class Value, class NodeAllocatorType>
auto HashMap<Key, Value, NodeAllocatorType>::operator=(HashMap&& other) noexcept -> HashMap&
{
    Base::operator=(static_cast<Base&&>(other));

    return *this;
}

template <class Key, class Value, class NodeAllocatorType>
HashMap<Key, Value, NodeAllocatorType>::~HashMap() = default;

template <class Key, class Value, class NodeAllocatorType>
auto HashMap<Key, Value, NodeAllocatorType>::operator[](const Key& key) -> Value&
{
    const Iterator it = Find(key);

    if (it != End())
    {
        return it->second;
    }

    return Insert(key, Value {}).first->second;
}

template <class Key, class Value, class NodeAllocatorType>
auto HashMap<Key, Value, NodeAllocatorType>::FindByHashCode(HashCode hash_code) -> Iterator
{
    auto* bucket = Base::GetBucketForHash(hash_code.Value());

    auto it = bucket->FindByHashCode(Base::key_by_fn, hash_code.Value());

    if (it == bucket->End())
    {
        return End();
    }

    return Iterator(this, it);
}

template <class Key, class Value, class NodeAllocatorType>
auto HashMap<Key, Value, NodeAllocatorType>::FindByHashCode(HashCode hash_code) const -> ConstIterator
{
    auto* bucket = Base::GetBucketForHash(hash_code.Value());

    const auto it = bucket->FindByHashCode(Base::key_by_fn, hash_code.Value());

    if (it == bucket->End())
    {
        return End();
    }

    return ConstIterator(this, it);
}

template <class Key, class Value, class NodeAllocatorType>
auto HashMap<Key, Value, NodeAllocatorType>::Set(const Key& key, const Value& value) -> InsertResult
{
    return Base::Set(KeyValuePair<Key, Value> { key, value });
}

template <class Key, class Value, class NodeAllocatorType>
auto HashMap<Key, Value, NodeAllocatorType>::Set(const Key& key, Value&& value) -> InsertResult
{
    return Base::Set(KeyValuePair<Key, Value> { key, std::move(value) });
}

template <class Key, class Value, class NodeAllocatorType>
auto HashMap<Key, Value, NodeAllocatorType>::Set(Key&& key, const Value& value) -> InsertResult
{
    return Base::Set(KeyValuePair<Key, Value> { std::move(key), value });
}

template <class Key, class Value, class NodeAllocatorType>
auto HashMap<Key, Value, NodeAllocatorType>::Set(Key&& key, Value&& value) -> InsertResult
{
    return Base::Set(KeyValuePair<Key, Value> { std::move(key), std::move(value) });
}

template <class Key, class Value, class NodeAllocatorType>
auto HashMap<Key, Value, NodeAllocatorType>::Insert(const Key& key, const Value& value) -> InsertResult
{
    return Base::Insert(KeyValuePair<Key, Value> { key, value });
}

template <class Key, class Value, class NodeAllocatorType>
auto HashMap<Key, Value, NodeAllocatorType>::Insert(const Key& key, Value&& value) -> InsertResult
{
    return Base::Insert(KeyValuePair<Key, Value> { key, std::move(value) });
}

template <class Key, class Value, class NodeAllocatorType>
auto HashMap<Key, Value, NodeAllocatorType>::Insert(Key&& key, const Value& value) -> InsertResult
{
    return Base::Insert(KeyValuePair<Key, Value> { std::move(key), value });
}

template <class Key, class Value, class NodeAllocatorType>
auto HashMap<Key, Value, NodeAllocatorType>::Insert(Key&& key, Value&& value) -> InsertResult
{
    return Base::Insert(KeyValuePair<Key, Value> { std::move(key), std::move(value) });
}

template <class Key, class Value, class NodeAllocatorType>
auto HashMap<Key, Value, NodeAllocatorType>::Insert(const KeyValuePair<Key, Value>& pair) -> InsertResult
{
    return Base::Insert(pair);
}

template <class Key, class Value, class NodeAllocatorType>
auto HashMap<Key, Value, NodeAllocatorType>::Insert(KeyValuePair<Key, Value>&& pair) -> InsertResult
{
    return Base::Insert(std::move(pair));
}

} // namespace containers

using containers::HashMap;

} // namespace hyperion

#endif
