/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FLAT_MAP_HPP
#define HYPERION_FLAT_MAP_HPP

#include <core/containers/FlatSet.hpp>
#include <core/containers/Array.hpp>
#include <core/utilities/Pair.hpp>
#include <core/containers/ContainerBase.hpp>

#include <HashCode.hpp>

namespace hyperion {
namespace containers {

template <class Key, class Value>
class FlatMap : public ContainerBase<FlatMap<Key, Value>, Key>
{
public:
    using KeyValuePairType = KeyValuePair<Key, Value>;

private:
    Array<KeyValuePairType> m_vector;

public:
    static constexpr bool is_contiguous = true;

    using Base = ContainerBase<FlatMap<Key, Value>, Key>;

    using KeyType = Key;
    using ValueType = Value;

    using Iterator = typename decltype(m_vector)::Iterator;
    using ConstIterator = typename decltype(m_vector)::ConstIterator;
    using InsertResult = Pair<Iterator, bool>; // iterator, was inserted

    FlatMap();

    template <SizeType Sz>
    FlatMap(KeyValuePairType const (&items)[Sz])
    {
        m_vector.Reserve(Sz);

        for (auto it = items; it != items + Sz; ++it) {
            Insert(*it);
        }
    }

    template <SizeType Sz>
    FlatMap(KeyValuePairType (&&items)[Sz])
    {
        m_vector.Reserve(Sz);

        for (auto it = items; it != items + Sz; ++it) {
            Insert(std::move(*it));
        }
    }

    FlatMap(std::initializer_list<Pair<Key, Value>> initializer_list)
    {
        m_vector.Reserve(initializer_list.size());

        for (const auto &it : initializer_list) {
            Insert(it);
        }
    }

    FlatMap(const FlatMap &other);
    FlatMap &operator=(const FlatMap &other);
    FlatMap(FlatMap &&other) noexcept;
    FlatMap &operator=(FlatMap &&other) noexcept;
    ~FlatMap();
    
    Iterator Find(const Key &key);
    ConstIterator Find(const Key &key) const;

    template <class TFindAsType>
    HYP_FORCE_INLINE auto FindAs(const TFindAsType &key) -> Iterator
    {
        const auto it = FlatMap<Key, Value>::Base::LowerBound(key);

        if (it == End()) {
            return it;
        }

        return (it->first == key) ? it : End();
    }

    template <class TFindAsType>
    HYP_FORCE_INLINE auto FindAs(const TFindAsType &key) const -> ConstIterator
    {
        const auto it = FlatMap<Key, Value>::Base::LowerBound(key);

        if (it == End()) {
            return it;
        }

        return (it->first == key) ? it : End();
    }

    template <class TFindAsType>
    HYP_FORCE_INLINE bool Contains(const TFindAsType &key) const
        { return FindAs(key) != End(); }

    InsertResult Insert(const Key &key, const Value &value);
    InsertResult Insert(const Key &key, Value &&value);
    InsertResult Insert(Pair<Key, Value> &&pair);
    InsertResult Insert(const Pair<Key, Value> &pair);

    InsertResult Set(const Key &key, const Value &value);
    InsertResult Set(const Key &key, Value &&value);

    template <class... Args>
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

    HYP_FORCE_INLINE void Reserve(SizeType size)
        { m_vector.Reserve(size); }
    
    HYP_FORCE_INLINE KeyValuePairType &Front()
        { return m_vector.Front(); }

    HYP_FORCE_INLINE const KeyValuePairType &Front() const
        { return m_vector.Front(); }

    HYP_FORCE_INLINE KeyValuePairType &Back()
        { return m_vector.Back(); }

    HYP_FORCE_INLINE const KeyValuePairType &Back() const
        { return m_vector.Back(); }

    HYP_NODISCARD FlatSet<Key> Keys() const;
    HYP_NODISCARD FlatSet<Value> Values() const;

    HYP_NODISCARD HYP_FORCE_INLINE operator Span<KeyValuePairType>()
        { return Span<KeyValuePairType>(Data(), Size()); }

    HYP_NODISCARD HYP_FORCE_INLINE operator Span<const KeyValuePairType>() const
        { return Span<const KeyValuePairType>(Data(), Size()); }

    HYP_NODISCARD HYP_FORCE_INLINE Span<KeyValuePairType> ToSpan()
        { return Span<KeyValuePairType>(Data(), Size()); }

    HYP_NODISCARD HYP_FORCE_INLINE Span<const KeyValuePairType> ToSpan() const
        { return Span<const KeyValuePairType>(Data(), Size()); }

    template <class OtherContainerType>
    FlatMap &Merge(const OtherContainerType &other)
    {
        for (const auto &item : other) {
            Set_Internal(Pair<Key, Value>(item));
        }

        return *this;
    }

    template <class OtherContainerType>
    FlatMap &Merge(OtherContainerType &&other)
    {
        for (auto &item : other) {
            Set_Internal(std::move(item));
        }

        other.Clear();

        return *this;
    }

    HYP_FORCE_INLINE ValueType &At(const KeyType &key)
    {
        const auto it = Find(key);
        AssertDebugMsg(it != End(), "At(): Element not found");

        return it->second;
    }

    HYP_FORCE_INLINE const ValueType &At(const KeyType &key) const
    {
        const auto it = Find(key);
        AssertDebugMsg(it != End(), "At(): Element not found");

        return it->second;
    }

    HYP_FORCE_INLINE KeyValuePairType &AtIndex(SizeType index)
        { AssertDebugMsg(index < Size(), "Out of bounds"); return *(Data() + index); }

    HYP_FORCE_INLINE const KeyValuePairType &AtIndex(SizeType index) const
        { AssertDebugMsg(index < Size(), "Out of bounds"); return *(Data() + index); }

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

private:
    InsertResult Set_Internal(Pair<KeyType, ValueType> &&pair);
    InsertResult Insert_Internal(Pair<KeyType, ValueType> &&pair);
};

template <class Key, class Value>
FlatMap<Key, Value>::FlatMap() {}

template <class Key, class Value>
FlatMap<Key, Value>::FlatMap(const FlatMap &other)
    : m_vector(other.m_vector)
{
}

template <class Key, class Value>
auto FlatMap<Key, Value>::operator=(const FlatMap &other) -> FlatMap &
{
    m_vector = other.m_vector;

    return *this;
}

template <class Key, class Value>
FlatMap<Key, Value>::FlatMap(FlatMap &&other) noexcept
    : m_vector(std::move(other.m_vector))
{
}

template <class Key, class Value>
auto FlatMap<Key, Value>::operator=(FlatMap &&other) noexcept -> FlatMap&
{
    m_vector = std::move(other.m_vector);

    return *this;
}

template <class Key, class Value>
FlatMap<Key, Value>::~FlatMap() = default;

template <class Key, class Value>
auto FlatMap<Key, Value>::Find(const Key &key) -> Iterator
{
    const auto it = FlatMap<Key, Value>::Base::LowerBound(key);

    if (it == End()) {
        return it;
    }

    return (it->first == key) ? it : End();
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Find(const Key &key) const -> ConstIterator
{
    const auto it = FlatMap<Key, Value>::Base::LowerBound(key);

    if (it == End()) {
        return it;
    }

    return (it->first == key) ? it : End();
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert_Internal(Pair<Key, Value> &&pair) -> InsertResult
{
    const auto lower_bound = m_vector.LowerBound(pair.first);

    if (lower_bound == End() || !(lower_bound->first == pair.first)) {
        auto it = m_vector.Insert(lower_bound, std::move(pair));

        return { it, true };
    }

    return { lower_bound, false };
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(const Key &key, const Value &value) -> InsertResult
{
    return Insert_Internal(Pair<Key, Value> { key, value });
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(const Key &key, Value &&value) -> InsertResult
{
    return Insert_Internal(Pair<Key, Value> { key, std::move(value) });
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(const Pair<Key, Value> &pair) -> InsertResult
{
    return Insert_Internal(Pair<Key, Value>(pair));
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Insert(Pair<Key, Value> &&pair) -> InsertResult
{
    return Insert_Internal(std::move(pair));
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Set_Internal(Pair<Key, Value> &&pair) -> InsertResult
{
    const auto lower_bound = m_vector.LowerBound(pair.first);//FlatMap<Key, Value>::Base::LowerBound(key);

    if (lower_bound == End() || !(lower_bound->first == pair.first)) {
        auto it = m_vector.Insert(lower_bound, std::move(pair));

        return { it, true };
    }

    lower_bound->second = std::move(pair.second);

    return InsertResult { lower_bound, true };
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Set(const Key &key, const Value &value) -> InsertResult
{
    return Set_Internal(Pair<Key, Value> { key, value });
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Set(const Key &key, Value &&value) -> InsertResult
{
    return Set_Internal(Pair<Key, Value> { key, std::move(value) });
}

template <class Key, class Value>
auto FlatMap<Key, Value>::Erase(ConstIterator it) -> Iterator
{
    return m_vector.Erase(it);
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
} // namespace containers

template <class Key, class Value>
using FlatMap = containers::FlatMap<Key, Value>;

} // namespace hyperion

#endif
