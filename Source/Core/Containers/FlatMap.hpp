/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/FlatSet.hpp>
#include <Core/Containers/SortedArray.hpp>

#include <Core/Utilities/Pair.hpp>
#include <Core/Utilities/Traits.hpp>

#include <Core/HashCode.hpp>

namespace Hyperion {
namespace containers {

/*! \brief TFlatMap is a sorted associative container that stores key-value pairs in a flat contiguous array, based on the Array implementation.
 *  It provides fast lookup and insertion while maintaining order.
 *  \tparam Key The type of keys stored in the flat map.
 *  \tparam Value The type of values stored in the flat map. */
template <class Key, class Value, class AllocatorType = DynamicAllocator>
class TFlatMap : public TSortedArray<KeyValuePair<Key, Value>, AllocatorType>
{
public:
    using KeyValuePairType = KeyValuePair<Key, Value>;

public:
    static constexpr bool isContiguous = true;

    using Base = TSortedArray<KeyValuePair<Key, Value>, AllocatorType>;

    using KeyType = Key;
    using ValueType = KeyValuePairType;

    using Iterator = typename Base::Iterator;
    using ConstIterator = typename Base::ConstIterator;

    using InsertResult = Pair<Iterator, bool>; // iterator, was inserted

    TFlatMap();

    template <size_t Sz>
    TFlatMap(KeyValuePairType const (&items)[Sz])
    {
        Base::Reserve(Sz);

        for (auto it = items; it != items + Sz; ++it)
        {
            Insert(*it);
        }
    }

    template <size_t Sz>
    TFlatMap(KeyValuePairType (&&items)[Sz])
    {
        Base::Reserve(Sz);

        for (auto it = items; it != items + Sz; ++it)
        {
            Insert(std::move(*it));
        }
    }

    TFlatMap(std::initializer_list<Pair<Key, Value>> initializerList)
    {
        Base::Reserve(initializerList.size());

        for (const auto& it : initializerList)
        {
            Insert(it);
        }
    }

    TFlatMap(const TFlatMap& other);
    TFlatMap& operator=(const TFlatMap& other);
    
    TFlatMap(TFlatMap&& other) noexcept;
    TFlatMap& operator=(TFlatMap&& other) noexcept;

    ~TFlatMap();

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

    HYP_FORCE_INLINE KeyValuePair<Key, Value>* TryGet(const KeyType& key)
    {
        Iterator it = Find(key);

        return it != End() ? &(*it) : nullptr;
    }

    HYP_FORCE_INLINE const KeyValuePair<Key, Value>* TryGet(const KeyType& key) const
    {
        ConstIterator it = Find(key);

        return it != End() ? &(*it) : nullptr;
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

    HYP_FORCE_INLINE size_t Size() const
    {
        return Base::Size();
    }

    HYP_FORCE_INLINE KeyValuePairType* Data()
    {
        return Base::Data();
    }

    HYP_FORCE_INLINE const KeyValuePairType* Data() const
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

    HYP_FORCE_INLINE void Reserve(size_t size)
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

    HYP_NODISCARD TFlatSet<Key, AllocatorType> Keys() const;
    HYP_NODISCARD TFlatSet<Value, AllocatorType> Values() const;

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
    TFlatMap& Merge(const OtherContainerType& other)
    {
        for (const auto& item : other)
        {
            Set_Internal(KeyValuePair<Key, Value>(item));
        }

        return *this;
    }

    template <class OtherContainerType>
    TFlatMap& Merge(OtherContainerType&& other)
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

    HYP_FORCE_INLINE KeyValuePairType& AtIndex(size_t index)
    {
        HYP_CORE_ASSERT(index < Size(), "Out of bounds");
        return *(Data() + index);
    }

    HYP_FORCE_INLINE const KeyValuePairType& AtIndex(size_t index) const
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

template <class Key, class Value, class AllocatorType>
TFlatMap<Key, Value, AllocatorType>::TFlatMap()
{
}

template <class Key, class Value, class AllocatorType>
TFlatMap<Key, Value, AllocatorType>::TFlatMap(const TFlatMap& other)
    : Base(static_cast<const Base&>(other))
{
}

template <class Key, class Value, class AllocatorType>
auto TFlatMap<Key, Value, AllocatorType>::operator=(const TFlatMap& other) -> TFlatMap&
{
    Base::operator=(static_cast<const Base&>(other));

    return *this;
}

template <class Key, class Value, class AllocatorType>
TFlatMap<Key, Value, AllocatorType>::TFlatMap(TFlatMap&& other) noexcept
    : Base(std::move(static_cast<Base&&>(other)))
{
}

template <class Key, class Value, class AllocatorType>
auto TFlatMap<Key, Value, AllocatorType>::operator=(TFlatMap&& other) noexcept -> TFlatMap&
{
    Base::operator=(static_cast<Base&&>(other));

    return *this;
}

template <class Key, class Value, class AllocatorType>
TFlatMap<Key, Value, AllocatorType>::~TFlatMap() = default;

template <class Key, class Value, class AllocatorType>
auto TFlatMap<Key, Value, AllocatorType>::Find(const Key& key) -> Iterator
{
    const auto it = Base::LowerBound(key);

    if (it == End())
    {
        return it;
    }

    return (it->first == key) ? it : End();
}

template <class Key, class Value, class AllocatorType>
auto TFlatMap<Key, Value, AllocatorType>::Find(const Key& key) const -> ConstIterator
{
    const auto it = Base::LowerBound(key);

    if (it == End())
    {
        return it;
    }

    return (it->first == key) ? it : End();
}

template <class Key, class Value, class AllocatorType>
auto TFlatMap<Key, Value, AllocatorType>::Insert_Internal(KeyValuePair<Key, Value>&& pair) -> InsertResult
{
    const auto lowerBound = Base::LowerBound(pair.first);

    if (lowerBound == End() || !(lowerBound->first == pair.first))
    {
        auto it = static_cast<Array<KeyValuePairType, AllocatorType>&>(*this).Insert(lowerBound, std::move(pair));

        return { it, true };
    }

    return { lowerBound, false };
}

template <class Key, class Value, class AllocatorType>
auto TFlatMap<Key, Value, AllocatorType>::Insert(const Key& key, const Value& value) -> InsertResult
{
    return Insert_Internal(KeyValuePair<Key, Value> { key, value });
}

template <class Key, class Value, class AllocatorType>
auto TFlatMap<Key, Value, AllocatorType>::Insert(const Key& key, Value&& value) -> InsertResult
{
    return Insert_Internal(KeyValuePair<Key, Value> { key, std::move(value) });
}

template <class Key, class Value, class AllocatorType>
auto TFlatMap<Key, Value, AllocatorType>::Insert(const Pair<Key, Value>& pair) -> InsertResult
{
    return Insert_Internal(KeyValuePair<Key, Value>(pair));
}

template <class Key, class Value, class AllocatorType>
auto TFlatMap<Key, Value, AllocatorType>::Insert(Pair<Key, Value>&& pair) -> InsertResult
{
    return Insert_Internal(std::move(pair));
}

template <class Key, class Value, class AllocatorType>
auto TFlatMap<Key, Value, AllocatorType>::Set_Internal(KeyValuePair<Key, Value>&& pair) -> InsertResult
{
    const auto lowerBound = Base::LowerBound(pair.first);

    if (lowerBound == End() || !(lowerBound->first == pair.first))
    {
        auto it = static_cast<Array<KeyValuePairType, AllocatorType>&>(*this).Insert(lowerBound, std::move(pair));

        return { it, true };
    }

    lowerBound->second = std::move(pair.second);

    return InsertResult { lowerBound, true };
}

template <class Key, class Value, class AllocatorType>
auto TFlatMap<Key, Value, AllocatorType>::Set(const Key& key, const Value& value) -> InsertResult
{
    return Set_Internal(KeyValuePair<Key, Value> { key, value });
}

template <class Key, class Value, class AllocatorType>
auto TFlatMap<Key, Value, AllocatorType>::Set(const Key& key, Value&& value) -> InsertResult
{
    return Set_Internal(KeyValuePair<Key, Value> { key, std::move(value) });
}

template <class Key, class Value, class AllocatorType>
auto TFlatMap<Key, Value, AllocatorType>::Erase(ConstIterator it) -> Iterator
{
    return Base::Erase(it);
}

template <class Key, class Value, class AllocatorType>
bool TFlatMap<Key, Value, AllocatorType>::Erase(const Key& value)
{
    return Erase(Find(value));
}

template <class Key, class Value, class AllocatorType>
TFlatSet<Key, AllocatorType> TFlatMap<Key, Value, AllocatorType>::Keys() const
{
    TFlatSet<Key, AllocatorType> keys;
    keys.Reserve(Size());

    for (const auto& it : *this)
    {
        keys.Insert(it.first);
    }

    return keys;
}

template <class Key, class Value, class AllocatorType>
TFlatSet<Value, AllocatorType> TFlatMap<Key, Value, AllocatorType>::Values() const
{
    TFlatSet<Value, AllocatorType> values;
    values.Reserve(Size());

    for (const auto& it : *this)
    {
        values.Insert(it.second);
    }

    return values;
}

} // namespace containers

using containers::TFlatMap;

template <class Key, class Value, class AllocatorType>
struct IsFlatMap<TFlatMap<Key, Value, AllocatorType>> : std::true_type
{
};

} // namespace Hyperion
