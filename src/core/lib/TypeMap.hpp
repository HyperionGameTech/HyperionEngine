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
class TypeMap : public ContainerBase<TypeMap<Value>, TypeID> {
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

        return m_map.Find(id) != m_map.End();
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

    void Clear()
    {
        m_map.Clear();
    }

    HYP_DEF_STL_BEGIN_END(m_map.Begin(), m_map.End())

protected:
    Map m_map;
};

} // namespace hyperion

#endif