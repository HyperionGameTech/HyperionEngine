/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TYPE_MAP_HPP
#define HYPERION_TYPE_MAP_HPP

#include <core/containers/ContainerBase.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/FlatSet.hpp>
#include <core/utilities/TypeId.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion {
namespace containers {

template <class Value>
class TypeMap : public ContainerBase<TypeMap<Value>, TypeId>
{
protected:
    using Map = FlatMap<TypeId, Value>;

public:
    static constexpr bool is_contiguous = Map::is_contiguous;

    using KeyValuePairType = typename Map::KeyValuePairType;

    using KeyType = TypeId;
    using ValueType = KeyValuePairType;

    using InsertResult = typename Map::InsertResult;

    using Iterator = typename Map::Iterator;
    using ConstIterator = typename Map::ConstIterator;

    TypeMap() = default;
    TypeMap(const TypeMap& other) = default;
    TypeMap& operator=(const TypeMap& other) = default;

    TypeMap(TypeMap&& other) noexcept
        : m_map(std::move(other.m_map))
    {
    }

    TypeMap& operator=(TypeMap&& other) noexcept
    {
        m_map = std::move(other.m_map);

        return *this;
    }

    ~TypeMap() = default;

    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_map.Size();
    }

    HYP_FORCE_INLINE KeyValuePairType* Data()
    {
        return m_map.Data();
    }

    HYP_FORCE_INLINE KeyValuePairType* const Data() const
    {
        return m_map.Data();
    }

    template <class T>
    HYP_FORCE_INLINE InsertResult Set(const Value& value)
    {
        const auto id = TypeId::ForType<T>();

        return m_map.Set(id, value);
    }

    template <class T>
    HYP_FORCE_INLINE InsertResult Set(Value&& value)
    {
        const auto id = TypeId::ForType<T>();

        return m_map.Set(id, std::move(value));
    }

    HYP_FORCE_INLINE InsertResult Set(TypeId type_id, const Value& value)
    {
        return m_map.Set(type_id, value);
    }

    HYP_FORCE_INLINE InsertResult Set(TypeId type_id, Value&& value)
    {
        return m_map.Set(type_id, std::move(value));
    }

    HYP_FORCE_INLINE Value& Get(TypeId type_id)
    {
        Iterator it = m_map.Find(type_id);
        AssertThrow(it != m_map.End());

        return it->second;
    }

    template <class T>
    HYP_FORCE_INLINE Iterator Find()
    {
        const auto id = TypeId::ForType<T>();

        return m_map.Find(id);
    }

    template <class T>
    HYP_FORCE_INLINE ConstIterator Find() const
    {
        const auto id = TypeId::ForType<T>();

        return m_map.Find(id);
    }

    HYP_FORCE_INLINE Iterator Find(TypeId type_id)
    {
        return m_map.Find(type_id);
    }

    HYP_FORCE_INLINE ConstIterator Find(TypeId type_id) const
    {
        return m_map.Find(type_id);
    }

    HYP_FORCE_INLINE Iterator Erase(ConstIterator it)
    {
        return m_map.Erase(it);
    }

    HYP_FORCE_INLINE bool Erase(TypeId type_id)
    {
        return m_map.Erase(type_id);
    }

    template <class T>
    HYP_FORCE_INLINE bool Erase()
    {
        return m_map.Erase(TypeId::ForType<T>());
    }

    template <class T>
    HYP_FORCE_INLINE Value& At()
    {
        const auto it = Find<T>();

        AssertThrow(it != m_map.End());

        return it->second;
    }

    template <class T>
    HYP_FORCE_INLINE const Value& At() const
    {
        const auto it = Find<T>();

        AssertThrow(it != m_map.End());

        return it->second;
    }

    HYP_FORCE_INLINE Value& At(TypeId type_id)
    {
        const auto it = Find(type_id);

        AssertThrow(it != m_map.End());

        return it->second;
    }

    HYP_FORCE_INLINE const Value& At(TypeId type_id) const
    {
        const auto it = Find(type_id);

        AssertThrow(it != m_map.End());

        return it->second;
    }

    HYP_FORCE_INLINE Value& AtIndex(SizeType index)
    {
        return m_map.AtIndex(index).second;
    }

    HYP_FORCE_INLINE const Value& AtIndex(SizeType index) const
    {
        return m_map.AtIndex(index).second;
    }

    HYP_FORCE_INLINE Value& operator[](TypeId type_id)
    {
        const auto it = Find(type_id);

        if (it == m_map.End())
        {
            return m_map.Set(type_id, Value()).first->second;
        }

        return it->second;
    }

    HYP_FORCE_INLINE bool Contains(TypeId type_id) const
    {
        return m_map.Contains(type_id);
    }

    template <class T>
    HYP_FORCE_INLINE bool Contains() const
    {
        const auto id = TypeId::ForType<T>();

        return m_map.Contains(id);
    }

    template <class T>
    HYP_FORCE_INLINE bool Remove()
    {
        const auto it = Find<T>();

        if (it == m_map.End())
        {
            return false;
        }

        m_map.Erase(it);

        return true;
    }

    HYP_FORCE_INLINE bool Remove(TypeId type_id)
    {
        const auto it = m_map.Find(type_id);

        if (it == m_map.End())
        {
            return false;
        }

        m_map.Erase(it);

        return true;
    }

    HYP_FORCE_INLINE void Clear()
    {
        m_map.Clear();
    }

    HYP_FORCE_INLINE bool Any() const
    {
        return m_map.Any();
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return m_map.Empty();
    }

    HYP_NODISCARD HYP_FORCE_INLINE FlatSet<TypeId> Keys() const
    {
        return m_map.Keys();
    }

    HYP_NODISCARD FlatSet<Value> Values() const
    {
        return m_map.Values();
    }

    HYP_DEF_STL_BEGIN_END(m_map.Begin(), m_map.End())

protected:
    Map m_map;
};
} // namespace containers

template <class Value>
using TypeMap = containers::TypeMap<Value>;

} // namespace hyperion

#endif