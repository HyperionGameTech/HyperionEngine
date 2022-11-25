#ifndef HYPERION_TYPE_MAP_H
#define HYPERION_TYPE_MAP_H

#include "ContainerBase.hpp"
#include "FlatMap.hpp"
#include "FlatSet.hpp"
#include "TypeID.hpp"

#include <Types.hpp>
#include <HashCode.hpp>
#include <util/Defines.hpp>

#include <atomic>

namespace hyperion {

template <class Value>
class TypeMap : public ContainerBase<TypeMap<Value>, TypeID>
{
protected:
    using Map = FlatMap<TypeID, Value>;

public:
    using InsertResult = typename Map::InsertResult;

    using Iterator = typename Map::Iterator;
    using ConstIterator = typename Map::ConstIterator;

    TypeMap() = default;
    TypeMap(const TypeMap &other) = default;
    TypeMap &operator=(const TypeMap &other) = default;

    TypeMap(TypeMap &&other) noexcept
        : m_map(std::move(other.m_map))
    {
    }

    TypeMap &operator=(TypeMap &&other) noexcept
    {
        m_map = std::move(other.m_map);

        return *this;
    }

    ~TypeMap() = default;

    SizeType Size() const { return m_map.Size(); }
    
    template <class T>
    InsertResult Set(const Value &value)
    {
        const auto id = TypeID::ForType<T>();

        return m_map.Set(id, value);
    }
    
    template <class T>
    InsertResult Set(Value &&value)
    {
        const auto id = TypeID::ForType<T>();

        return m_map.Set(id, std::move(value));
    }

    InsertResult Set(const TypeID &type_id, const Value &value)
    {
        return m_map.Set(type_id, value);
    }

    InsertResult Set(const TypeID &type_id, Value &&value)
    {
        return m_map.Set(type_id, std::move(value));
    }

    Value &Get(const TypeID &type_id)
    {
        Iterator it = m_map.Find(type_id);
        AssertThrow(it != m_map.End());

        return it->second;
    }

    template <class T>
    Iterator Find()
    {
        const auto id = TypeID::ForType<T>();

        return m_map.Find(id);
    }

    template <class T>
    ConstIterator Find() const
    {
        const auto id = TypeID::ForType<T>();

        return m_map.Find(id);
    }

    Iterator Find(const TypeID &type_id)
    {
        return m_map.Find(type_id);
    }

    ConstIterator Find(const TypeID &type_id) const
    {
        return m_map.Find(type_id);
    }

    bool Erase(Iterator it)
    {
        return m_map.Erase(it);
    }

    bool Erase(const TypeID &type_id)
    {
        return m_map.Erase(type_id);
    }

    template <class T>
    Value &At()
    {
        const auto it = Find<T>();

        AssertThrow(it != m_map.End());

        return it->second;
    }

    template <class T>
    const Value &At() const
    {
        const auto it = Find<T>();

        AssertThrow(it != m_map.End());

        return it->second;
    }

    template <class T>
    bool Contains() const
    {
        const auto id = TypeID::ForType<T>();

        return m_map.Contains(id);
    }
    
    template <class T>
    bool Remove()
    {
        const auto it = Find<T>();

        if (it == m_map.End()) {
            return false;
        }

        m_map.Erase(it);

        return true;
    }

    bool Remove(const TypeID &type_id)
    {
        const auto it = m_map.Find(type_id);

        if (it == m_map.End()) {
            return false;
        }

        m_map.Erase(it);

        return true;
    }

    void Clear()
    {
        m_map.Clear();
    }

    bool Any() const
        { return m_map.Any(); }

    bool Empty() const
        { return m_map.Empty(); }

    HYP_DEF_STL_BEGIN_END(m_map.Begin(), m_map.End())

protected:
    Map m_map;
};

} // namespace hyperion

#endif