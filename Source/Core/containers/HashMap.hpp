/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/HashSet.hpp>

#include <Core/utilities/Pair.hpp>

#include <Core/functional/FunctionWrapper.hpp>

#include <Core/utilities/Traits.hpp>
#include <Core/HashCode.hpp>

namespace Hyperion {

namespace containers {

/*! \brief HashMap is a hash table based associative container that stores key-value pairs, based on the HashSet implementation.
 *  A custom allocator can be provided to control memory allocation for the nodes.
 *  \tparam Key The type of keys stored in the hash map.
 * \tparam Value The type of values stored in the hash map.
 * \tparam AllocatorType The type of node allocator used for managing memory (See InstrusiveMap)
 *  \tparam Policy Whether to use node pooling or dynamic nodes - if persistent pointers to elements are needed, use HashTablePolicy::NotPooled and the pointers will remain persistent until rehash. */
template <class Key, class Value, class AllocatorType = DynamicAllocator, class Policy = HashTablePolicy::NodePooling>
class HashMap : public IntrusiveMap<KeyValuePair<Key, Value>, &KeyValuePair<Key, Value>::first, AllocatorType, Policy>
{
public:
    using Base = IntrusiveMap<KeyValuePair<Key, Value>, &KeyValuePair<Key, Value>::first, AllocatorType, Policy>;

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

    HashMap(std::initializer_list<KeyValuePair<Key, Value>> initializerList)
        : HashMap()
    {
        Array<KeyValuePair<Key, Value>> temp(initializerList);

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
        HYP_CORE_ASSERT(it != Base::End(), "At(): Element not found");

        return it->second;
    }

    HYP_FORCE_INLINE const Value& At(const Key& key) const
    {
        const auto it = Base::Find(key);
        HYP_CORE_ASSERT(it != Base::End(), "At(): Element not found");

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
                const auto otherIt = other.Find(it->value.first);

                if (otherIt == other.End())
                {
                    return false;
                }

                if (otherIt->second != it->value.second)
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
                const auto otherIt = other.Find(it->value.first);

                if (otherIt == other.End())
                {
                    return true;
                }

                if (otherIt->second != it->value.second)
                {
                    return true;
                }
            }
        }

        return false;
    }

    Iterator FindByHashCode(HashCode hashCode);
    ConstIterator FindByHashCode(HashCode hashCode) const;

    KeyValuePair<Key, Value>* TryGet(const KeyType& key)
    {
        const auto it = Find(key);

        return it != End() ? &(*it) : nullptr;
    }

    const KeyValuePair<Key, Value>* TryGet(const KeyType& key) const
    {
        const auto it = Find(key);

        return it != End() ? &(*it) : nullptr;
    }

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

    /*! \brief Alias for Insert(), but returns the inserted value directly. */
    HYP_FORCE_INLINE const KeyValuePair<Key, Value>& Add(const KeyValuePair<Key, Value>& value)
    {
        return *Insert(value).first;
    }

    /*! \brief Alias for Insert(), but returns the inserted value directly. */
    HYP_FORCE_INLINE const KeyValuePair<Key, Value>& Add(const KeyValuePair<Key, Value>&& value)
    {
        return *Insert(std::move(value)).first;
    }

    template <class OtherContainerType>
    HYP_FORCE_INLINE HashMap& Merge(OtherContainerType&& other)
    {
        Base::Merge(std::forward<OtherContainerType>(other));

        return *this;
    }
};

template <class Key, class Value, class AllocatorType, class Policy>
HashMap<Key, Value, AllocatorType, Policy>::HashMap()
{
}

template <class Key, class Value, class AllocatorType, class Policy>
HashMap<Key, Value, AllocatorType, Policy>::HashMap(const HashMap& other)
    : Base(static_cast<const Base&>(other))
{
}

template <class Key, class Value, class AllocatorType, class Policy>
auto HashMap<Key, Value, AllocatorType, Policy>::operator=(const HashMap& other) -> HashMap&
{
    Base::operator=(static_cast<const Base&>(other));

    return *this;
}

template <class Key, class Value, class AllocatorType, class Policy>
HashMap<Key, Value, AllocatorType, Policy>::HashMap(HashMap&& other) noexcept
    : Base(static_cast<Base&&>(other))
{
}

template <class Key, class Value, class AllocatorType, class Policy>
auto HashMap<Key, Value, AllocatorType, Policy>::operator=(HashMap&& other) noexcept -> HashMap&
{
    Base::operator=(static_cast<Base&&>(other));

    return *this;
}

template <class Key, class Value, class AllocatorType, class Policy>
HashMap<Key, Value, AllocatorType, Policy>::~HashMap() = default;

template <class Key, class Value, class AllocatorType, class Policy>
auto HashMap<Key, Value, AllocatorType, Policy>::operator[](const Key& key) -> Value&
{
    const Iterator it = Find(key);

    if (it != End())
    {
        return it->second;
    }

    return Insert(key, Value {}).first->second;
}

template <class Key, class Value, class AllocatorType, class Policy>
auto HashMap<Key, Value, AllocatorType, Policy>::FindByHashCode(HashCode hashCode) -> Iterator
{
    auto* bucket = Base::GetBucketForHash(hashCode.Value());

    auto it = bucket->FindByHashCode(Base::keyByFn, hashCode.Value());

    if (it == bucket->End())
    {
        return End();
    }

    return Iterator(this, it);
}

template <class Key, class Value, class AllocatorType, class Policy>
auto HashMap<Key, Value, AllocatorType, Policy>::FindByHashCode(HashCode hashCode) const -> ConstIterator
{
    auto* bucket = Base::GetBucketForHash(hashCode.Value());

    const auto it = bucket->FindByHashCode(Base::keyByFn, hashCode.Value());

    if (it == bucket->End())
    {
        return End();
    }

    return ConstIterator(this, it);
}

template <class Key, class Value, class AllocatorType, class Policy>
auto HashMap<Key, Value, AllocatorType, Policy>::Set(const Key& key, const Value& value) -> InsertResult
{
    return Base::Set(KeyValuePair<Key, Value> { key, value });
}

template <class Key, class Value, class AllocatorType, class Policy>
auto HashMap<Key, Value, AllocatorType, Policy>::Set(const Key& key, Value&& value) -> InsertResult
{
    return Base::Set(KeyValuePair<Key, Value> { key, std::move(value) });
}

template <class Key, class Value, class AllocatorType, class Policy>
auto HashMap<Key, Value, AllocatorType, Policy>::Set(Key&& key, const Value& value) -> InsertResult
{
    return Base::Set(KeyValuePair<Key, Value> { std::move(key), value });
}

template <class Key, class Value, class AllocatorType, class Policy>
auto HashMap<Key, Value, AllocatorType, Policy>::Set(Key&& key, Value&& value) -> InsertResult
{
    return Base::Set(KeyValuePair<Key, Value> { std::move(key), std::move(value) });
}

template <class Key, class Value, class AllocatorType, class Policy>
auto HashMap<Key, Value, AllocatorType, Policy>::Insert(const Key& key, const Value& value) -> InsertResult
{
    return Base::Insert(KeyValuePair<Key, Value> { key, value });
}

template <class Key, class Value, class AllocatorType, class Policy>
auto HashMap<Key, Value, AllocatorType, Policy>::Insert(const Key& key, Value&& value) -> InsertResult
{
    return Base::Insert(KeyValuePair<Key, Value> { key, std::move(value) });
}

template <class Key, class Value, class AllocatorType, class Policy>
auto HashMap<Key, Value, AllocatorType, Policy>::Insert(Key&& key, const Value& value) -> InsertResult
{
    return Base::Insert(KeyValuePair<Key, Value> { std::move(key), value });
}

template <class Key, class Value, class AllocatorType, class Policy>
auto HashMap<Key, Value, AllocatorType, Policy>::Insert(Key&& key, Value&& value) -> InsertResult
{
    return Base::Insert(KeyValuePair<Key, Value> { std::move(key), std::move(value) });
}

template <class Key, class Value, class AllocatorType, class Policy>
auto HashMap<Key, Value, AllocatorType, Policy>::Insert(const KeyValuePair<Key, Value>& pair) -> InsertResult
{
    return Base::Insert(pair);
}

template <class Key, class Value, class AllocatorType, class Policy>
auto HashMap<Key, Value, AllocatorType, Policy>::Insert(KeyValuePair<Key, Value>&& pair) -> InsertResult
{
    return Base::Insert(std::move(pair));
}

} // namespace containers

using containers::HashMap;

template <class Key, class Value, class AllocatorType, class Policy>
struct IsHashMap<containers::HashMap<Key, Value, AllocatorType, Policy>> : std::true_type
{
};

} // namespace Hyperion
