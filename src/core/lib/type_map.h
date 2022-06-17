#ifndef HYPERION_TYPE_MAP_H
#define HYPERION_TYPE_MAP_H

#include "flat_map.h"

#include <types.h>
#include <hash_code.h>
#include <util/defines.h>

#include <atomic>

namespace hyperion {

struct TypeId {
    using Value = uint;

    Value value;

    TypeId() : value{} {}
    TypeId(const Value &id) : value(id) {}
    TypeId(const TypeId &other) = default;
    TypeId &operator=(const TypeId &other) = default;

    TypeId(TypeId &&other) noexcept
        : value(other.value)
    {
        other.value = 0;
    }
    
    TypeId &operator=(TypeId &&other) noexcept
    {
        value = other.value;
        other.value = 0;

        return *this;
    }

    TypeId &operator=(Value id)
    {
        value = id;

        return *this;
    }

    bool operator==(const TypeId &other) const { return value == other.value; }
    bool operator!=(const TypeId &other) const { return value != other.value; }
    bool operator<(const TypeId &other) const  { return value < other.value; }
    bool operator>(const TypeId &other) const  { return value > other.value; }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(value);

        return hc;
    }
};

template <class Value>
class TypeMap {
    using Map = FlatMap<TypeId, Value>;

    static inline std::atomic<TypeId::Value> type_id_counter;

    template <class T>
    static TypeId GetTypeId()
    {
        static const TypeId id = TypeId{++type_id_counter};

        return id;
    }

public:
    using Iterator      = typename Map::Iterator;
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

    size_t Size() const { return m_map.Size(); }
    
    template <class T>
    void Set(const Value &value)
    {
        const auto id = GetTypeId<T>();

        m_map[id] = value;
    }
    
    template <class T>
    void Set(Value &&value)
    {
        const auto id = GetTypeId<T>();

        m_map[id] = std::move(value);
    }

    template <class T>
    Iterator Find()
    {
        const auto id = GetTypeId<T>();

        return m_map.Find(id);
    }

    template <class T>
    ConstIterator Find() const
    {
        const auto id = GetTypeId<T>();

        return m_map.Find(id);
    }

    template <class T>
    Value &At()
    {
        const auto it = Find<T>();

        AssertThrow(it != m_map.End());

        return it->second.get();
    }

    template <class T>
    const Value &At() const
    {
        const auto it = Find<T>();

        AssertThrow(it != m_map.End());

        return it->second.get();
    }

    template <class T>
    bool Contains() const
    {
        const auto id = GetTypeId<T>();

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

private:
    Map m_map;
};

} // namespace hyperion

#endif