/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/ContainerBase.hpp>
#include <Core/Containers/FlatMap.hpp>
#include <Core/Containers/FlatSet.hpp>
#include <Core/Reflection/TypeId.hpp>
#include <Core/Defines.hpp>

#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

namespace Hyperion {
namespace containers {

template <class Value>
class TypeMap : public ContainerBase<TypeMap<Value>, TypeId>
{
protected:
    using Map = FlatMap<TypeId, Value>;

public:
    static constexpr bool isContiguous = Map::isContiguous;

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

    HYP_FORCE_INLINE size_t Size() const
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

    HYP_FORCE_INLINE InsertResult Set(TypeId typeId, const Value& value)
    {
        return m_map.Set(typeId, value);
    }

    HYP_FORCE_INLINE InsertResult Set(TypeId typeId, Value&& value)
    {
        return m_map.Set(typeId, std::move(value));
    }

    HYP_FORCE_INLINE Value& Get(TypeId typeId)
    {
        Iterator it = m_map.Find(typeId);
        HYP_CORE_ASSERT(it != m_map.End());

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

    HYP_FORCE_INLINE Iterator Find(TypeId typeId)
    {
        return m_map.Find(typeId);
    }

    HYP_FORCE_INLINE ConstIterator Find(TypeId typeId) const
    {
        return m_map.Find(typeId);
    }

    HYP_FORCE_INLINE Iterator Erase(ConstIterator it)
    {
        return m_map.Erase(it);
    }

    HYP_FORCE_INLINE bool Erase(TypeId typeId)
    {
        return m_map.Erase(typeId);
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

        HYP_CORE_ASSERT(it != m_map.End());

        return it->second;
    }

    template <class T>
    HYP_FORCE_INLINE const Value& At() const
    {
        const auto it = Find<T>();

        HYP_CORE_ASSERT(it != m_map.End());

        return it->second;
    }

    HYP_FORCE_INLINE Value& At(TypeId typeId)
    {
        const auto it = Find(typeId);

        HYP_CORE_ASSERT(it != m_map.End());

        return it->second;
    }

    HYP_FORCE_INLINE const Value& At(TypeId typeId) const
    {
        const auto it = Find(typeId);

        HYP_CORE_ASSERT(it != m_map.End());

        return it->second;
    }

    HYP_FORCE_INLINE Value& AtIndex(size_t index)
    {
        return m_map.AtIndex(index).second;
    }

    HYP_FORCE_INLINE const Value& AtIndex(size_t index) const
    {
        return m_map.AtIndex(index).second;
    }

    HYP_FORCE_INLINE Value& operator[](TypeId typeId)
    {
        const auto it = Find(typeId);

        if (it == m_map.End())
        {
            return m_map.Set(typeId, Value()).first->second;
        }

        return it->second;
    }

    HYP_FORCE_INLINE bool Contains(TypeId typeId) const
    {
        return m_map.Contains(typeId);
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

    HYP_FORCE_INLINE bool Remove(TypeId typeId)
    {
        const auto it = m_map.Find(typeId);

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

} // namespace Hyperion
