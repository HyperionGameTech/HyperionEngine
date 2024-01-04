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
    static constexpr Bool is_contiguous = Map::is_contiguous;

    using KeyValuePairType  = typename Map::KeyValuePairType;

    using InsertResult      = typename Map::InsertResult;

    using Iterator          = typename Map::Iterator;
    using ConstIterator     = typename Map::ConstIterator;

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

    [[nodiscard]]
    SizeType Size() const
        { return m_map.Size(); }

    [[nodiscard]]
    KeyValuePairType *Data()
        { return m_map.Data(); }

    [[nodiscard]]
    KeyValuePairType * const Data() const
        { return m_map.Data(); }
    
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

    InsertResult Set(TypeID type_id, const Value &value)
        { return m_map.Set(type_id, value); }

    InsertResult Set(TypeID type_id, Value &&value)
        { return m_map.Set(type_id, std::move(value)); }

    [[nodiscard]]
    Value &Get(TypeID type_id)
    {
        Iterator it = m_map.Find(type_id);
        AssertThrow(it != m_map.End());

        return it->second;
    }

    template <class T>
    [[nodiscard]]
    Iterator Find()
    {
        const auto id = TypeID::ForType<T>();

        return m_map.Find(id);
    }

    template <class T>
    [[nodiscard]]
    ConstIterator Find() const
    {
        const auto id = TypeID::ForType<T>();

        return m_map.Find(id);
    }

    [[nodiscard]]
    Iterator Find(TypeID type_id)
        { return m_map.Find(type_id); }

    [[nodiscard]]
    ConstIterator Find(TypeID type_id) const
        { return m_map.Find(type_id); }

    Iterator Erase(ConstIterator it)
        { return m_map.Erase(it); }

    Bool Erase(TypeID type_id)
        { return m_map.Erase(type_id); }

    template <class T>
    Bool Erase()
        { return m_map.Erase(TypeID::ForType<T>()); }

    template <class T>
    [[nodiscard]]
    Value &At()
    {
        const auto it = Find<T>();

        AssertThrow(it != m_map.End());

        return it->second;
    }

    template <class T>
    [[nodiscard]]
    const Value &At() const
    {
        const auto it = Find<T>();

        AssertThrow(it != m_map.End());

        return it->second;
    }

    [[nodiscard]]
    Value &At(TypeID type_id)
    {
        const auto it = Find(type_id);

        AssertThrow(it != m_map.End());

        return it->second;
    }

    [[nodiscard]]
    const Value &At(TypeID type_id) const
    {
        const auto it = Find(type_id);

        AssertThrow(it != m_map.End());

        return it->second;
    }

    [[nodiscard]]
    Value &AtIndex(SizeType index)
        { return m_map.AtIndex(index).second; }

    [[nodiscard]]
    const Value &AtIndex(SizeType index) const
        { return m_map.AtIndex(index).second; }

    [[nodiscard]]
    Bool Contains(TypeID type_id) const
        { return m_map.Contains(type_id); }

    template <class T>
    [[nodiscard]]
    Bool Contains() const
    {
        const auto id = TypeID::ForType<T>();

        return m_map.Contains(id);
    }
    
    template <class T>
    Bool Remove()
    {
        const auto it = Find<T>();

        if (it == m_map.End()) {
            return false;
        }

        m_map.Erase(it);

        return true;
    }

    Bool Remove(TypeID type_id)
    {
        const auto it = m_map.Find(type_id);

        if (it == m_map.End()) {
            return false;
        }

        m_map.Erase(it);

        return true;
    }

    void Clear()
        { m_map.Clear(); }

    [[nodiscard]]
    Bool Any() const
        { return m_map.Any(); }

    [[nodiscard]]
    Bool Empty() const
        { return m_map.Empty(); }

    HYP_DEF_STL_BEGIN_END(m_map.Begin(), m_map.End())

protected:
    Map m_map;
};

} // namespace hyperion

#endif