/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/FlatSet.hpp>
#include <core/containers/SortedArray.hpp>

#include <core/utilities/Pair.hpp>
#include <core/utilities/Traits.hpp>

#include <core/HashCode.hpp>

namespace Hyperion {
namespace containers {

/*! \brief FlatMap is a sorted associative container that stores key-value pairs in a flat contiguous array, based on the Array implementation.
 *  It provides fast lookup and insertion while maintaining order.
 *  \tparam Key The type of keys stored in the flat map.
 *  \tparam Value The type of values stored in the flat map. */
template <class Key, class Value>
class FlatMap : public SortedArray<KeyValuePair<Key, Value>>
{
public:
    using KeyValuePairType = KeyValuePair<Key, Value>;

public:
    static constexpr bool isContiguous = true;

    using Base = SortedArray<KeyValuePairType>;

    using KeyType = Key;
    using ValueType = KeyValuePairType;

    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    using InsertResult = Pair<Iterator, bool>; // iterator, was inserted

    FlatMap();

    template <SizeType Sz>
    FlatMap(KeyValuePairType const (&items)[Sz])
    {
        Base::Reserve(Sz);

        for (auto it = items; it != items + Sz; ++it)
        {
            Insert(*it);
        }
    }

    template <SizeType Sz>
    FlatMap(KeyValuePairType (&&items)[Sz])
    {
        Base::Reserve(Sz);

        for (auto it = items; it != items + Sz; ++it)
        {
            Insert(std::move(*it));
        }
    }

    FlatMap(std::initializer_list<Pair<Key, Value>> initializerList)
    {
        Base::Reserve(initializerList.size());

        for (const auto& it : initializerList)
        {
            Insert(it);
        }
    }

    FlatMap(const FlatMap& other);
    FlatMap& operator=(const FlatMap& other);
    FlatMap(FlatMap&& other) noexcept;
    FlatMap& operator=(FlatMap&& other) noexcept;
    ~FlatMap();

    Iterator Find(const Key& key);
    ConstIterator Find(const Key& key) const;

    template <class TFindAsType>
    HYP_FORCE_INLINE auto FindAs(const TFindAsType& key) -> Iterator
    {
        const auto it = Base::LowerBound(key);

        if (it == End())
        {
            return it;
        }

        return (it->first == key) ? it : End();
    }

    template <class TFindAsType>
    HYP_FORCE_INLINE auto FindAs(const TFindAsType& key) const -> ConstIterator
    {
        const auto it = Base::LowerBound(key);

        if (it == End())
        {
            return it;
        }

        return (it->first == key) ? it : End();
    }

    template <class TFindAsType>
    HYP_FORCE_INLINE bool Contains(const TFindAsType& key) const
    {
        return FindAs(key) != End();
    }

    InsertResult Insert(const Key& key, const Value& value);
    InsertResult Insert(const Key& key, Value&& value);
    InsertResult Insert(Pair<Key, Value>&& pair);
    InsertResult Insert(const Pair<Key, Value>& pair);

    InsertResult Set(const Key& key, const Value& value);
    InsertResult Set(const Key& key, Value&& value);

    template <class... Args>
    InsertResult Emplace(const Key& key, Args&&... args)
    {
        return Insert(key, Value(std::forward<Args>(args)...));
    }

    Iterator Erase(ConstIterator it);
    bool Erase(const Key& key);

    HYP_FORCE_INLINE SizeType Size() const
    {
        return Base::Size();
    }

    HYP_FORCE_INLINE KeyValuePairType* Data()
    {
        return Base::Data();
    }

    HYP_FORCE_INLINE KeyValuePairType* const Data() const
    {
        return Base::Data();
    }

    HYP_FORCE_INLINE bool Any() const
    {
        return Base::Any();
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return Base::Empty();
    }

    HYP_FORCE_INLINE void Clear()
    {
        Base::Clear();
    }

    HYP_FORCE_INLINE void Reserve(SizeType size)
    {
        Base::Reserve(size);
    }

    HYP_FORCE_INLINE KeyValuePairType& Front()
    {
        return Base::Front();
    }

    HYP_FORCE_INLINE const KeyValuePairType& Front() const
    {
        return Base::Front();
    }

    HYP_FORCE_INLINE KeyValuePairType& Back()
    {
        return Base::Back();
    }

    HYP_FORCE_INLINE const KeyValuePairType& Back() const
    {
        return Base::Back();
    }

    HYP_NODISCARD FlatSet<Key> Keys() const;
    HYP_NODISCARD FlatSet<Value> Values() const;

    HYP_NODISCARD HYP_FORCE_INLINE operator Span<KeyValuePairType>()
    {
        return Span<KeyValuePairType>(Data(), Size());
    }

    HYP_NODISCARD HYP_FORCE_INLINE operator Span<const KeyValuePairType>() const
    {
        return Span<const KeyValuePairType>(Data(), Size());
    }

    HYP_NODISCARD HYP_FORCE_INLINE Span<KeyValuePairType> ToSpan()
    {
        return Span<KeyValuePairType>(Data(), Size());
    }

    HYP_NODISCARD HYP_FORCE_INLINE Span<const KeyValuePairType> ToSpan() const
    {
        return Span<const KeyValuePairType>(Data(), Size());
    }

    template <class OtherContainerType>
    FlatMap& Merge(const OtherContainerType& other)
    {
        for (const auto& item : other)
        {
            Set_Internal(KeyValuePair<Key, Value>(item));
        }

        return *this;
    }

    template <class OtherContainerType>
    FlatMap& Merge(OtherContainerType&& other)
    {
        for (auto& item : other)
        {
            Set_Internal(std::move(item));
        }

        other.Clear();

        return *this;
    }

    HYP_FORCE_INLINE Value& At(const Key& key)
    {
        const auto it = Find(key);
        HYP_CORE_ASSERT(it != End(), "At(): Element not found");

        return it->second;
    }

    HYP_FORCE_INLINE const Value& At(const Key& key) const
    {
        const auto it = Find(key);
        HYP_CORE_ASSERT(it != End(), "At(): Element not found");

        return it->second;
    }

    HYP_FORCE_INLINE KeyValuePairType& AtIndex(SizeType index)
    {
        HYP_CORE_ASSERT(index < Size(), "Out of bounds");
        return *(Data() + index);
    }

    HYP_FORCE_INLINE const KeyValuePairType& AtIndex(SizeType index) const
    {
        HYP_CORE_ASSERT(index < Size(), "Out of bounds");
        return *(Data() + index);
    }

    HYP_FORCE_INLINE Value& operator[](const Key& key)
    {
        const auto it = Find(key);

        if (it != End())
        {
            return it->second;
        }

        return Insert(key, Value {}).first->second;
    }

    HYP_DEF_STL_BEGIN_END(Base::Begin(), Base::End())

private:
    InsertResult Set_Internal(KeyValuePair<Key, Value>&& pair);
    InsertResult Insert_Internal(KeyValuePair<Key, Value>&& pair);
};

template <class Key, class Value>
FlatMap<Key, Value>::FlatMap()
{
}

template <class Key, class Value>
FlatMap<Key, Value>::FlatMap(const FlatMap& other)
    : Base(static_cast<const Base&>(other))
{
}

template <class Key, class Value>
auto FlatMap<Key, Value>::operator=(const FlatMap& other) -> FlatMap&
{
    Base::operator=(static_cast<const Base&>(other));

    return *this;
}

template <class Key, class Value>
FlatMap<Key, Value>::FlatMap(FlatMap&& other) noexcept
    : Base(std::move(static_cast<Base&&>(other)))
{
}

template <class Key, class Value>
auto FlatMap<Key, Value>::operator=(FlatMap&& other) noexcept -> FlatMap&
{
    Base::operator=(static_cast<Base&&>(other));

    return *this;
}

template <class Key, class Value>
FlatMap<Key, Value>::~FlatMap() = default;

template <class Key, class Value>
auto FlatMap<Key, Value>::Find(const Key& key) -> Iterator
{
    const auto it = Base::LowerBound(key);

    if (it == End())
    {
        return it;
    }

    return (it->first == key) ? it : End();
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Find(const Key& key) const -> ConstIterator
{
    const auto it = Base::LowerBound(key);

    if (it == End())
    {
        return it;
    }

    return (it->first == key) ? it : End();
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert_Internal(KeyValuePair<Key, Value>&& pair) -> InsertResult
{
    const auto lowerBound = Base::LowerBound(pair.first);

    if (lowerBound == End() || !(lowerBound->first == pair.first))
    {
        auto it = static_cast<Array<KeyValuePairType>&>(*this).Insert(lowerBound, std::move(pair));

        return { it, true };
    }

    return { lowerBound, false };
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(const Key& key, const Value& value) -> InsertResult
{
    return Insert_Internal(KeyValuePair<Key, Value> { key, value });
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(const Key& key, Value&& value) -> InsertResult
{
    return Insert_Internal(KeyValuePair<Key, Value> { key, std::move(value) });
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(const Pair<Key, Value>& pair) -> InsertResult
{
    return Insert_Internal(KeyValuePair<Key, Value>(pair));
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(Pair<Key, Value>&& pair) -> InsertResult
{
    return Insert_Internal(std::move(pair));
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Set_Internal(KeyValuePair<Key, Value>&& pair) -> InsertResult
{
    const auto lowerBound = Base::LowerBound(pair.first);

    if (lowerBound == End() || !(lowerBound->first == pair.first))
    {
        auto it = static_cast<Array<KeyValuePairType>&>(*this).Insert(lowerBound, std::move(pair));

        return { it, true };
    }

    lowerBound->second = std::move(pair.second);

    return InsertResult { lowerBound, true };
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Set(const Key& key, const Value& value) -> InsertResult
{
    return Set_Internal(KeyValuePair<Key, Value> { key, value });
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Set(const Key& key, Value&& value) -> InsertResult
{
    return Set_Internal(KeyValuePair<Key, Value> { key, std::move(value) });
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Erase(ConstIterator it) -> Iterator
{
    return Base::Erase(it);
}

template <class Key, class Value>
bool FlatMap<Key, Value>::Erase(const Key& value)
{
    return Erase(Find(value));
}

template <class Key, class Value>
FlatSet<Key> FlatMap<Key, Value>::Keys() const
{
    FlatSet<Key> keys;

    for (const auto& it : *this)
    {
        keys.Insert(it.first);
    }

    return keys;
}

template <class Key, class Value>
FlatSet<Value> FlatMap<Key, Value>::Values() const
{
    FlatSet<Value> values;

    for (const auto& it : *this)
    {
        values.Insert(it.second);
    }

    return values;
}

} // namespace containers

using containers::FlatMap;

template <class Key, class Value>
struct IsFlatMap<FlatMap<Key, Value>> : std::true_type
{
};

} // namespace Hyperion
