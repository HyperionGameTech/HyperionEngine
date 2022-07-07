#ifndef HYPERION_V2_LIB_COMPONENT_SET_H
#define HYPERION_V2_LIB_COMPONENT_SET_H

#include <Types.hpp>

namespace hyperion::v2 {

template <class Component>
class ComponentSetUnique {
public:
    using ComponentId = UInt;
    using Map         = FlatMap<ComponentId, std::unique_ptr<Component>>;

private:
    static inline std::atomic<ComponentId> id_counter;

public:
    template <class T>
    static ComponentId GetComponentId()
    {
        static_assert(std::is_base_of_v<Component, T>, "Object must be a derived class of the component class");

        static const ComponentId id = ++id_counter;

        return id;
    }

    using Iterator      = typename Map::Iterator;
    using ConstIterator = typename Map::ConstIterator;

    ComponentSetUnique() = default;
    ComponentSetUnique(const ComponentSetUnique &other) = delete;
    ComponentSetUnique &operator=(const ComponentSetUnique &other) = delete;

    ComponentSetUnique(ComponentSetUnique &&other) noexcept
        : m_map(std::move(other.m_map))
    {
    }

    ComponentSetUnique &operator=(ComponentSetUnique &&other) noexcept
    {
        m_map = std::move(other.m_map);

        return *this;
    }

    ~ComponentSetUnique() = default;
    
    template <class T>
    void Set(std::unique_ptr<T> &&component)
    {
        AssertThrow(component != nullptr);

        const auto id = GetComponentId<T>();

        m_map[id] = std::move(component);
    }

    /*! Only use if you know what you're doing (That the object has the exact type id) */
    void Set(ComponentId id, std::unique_ptr<Component> &&component)
    {
        AssertThrow(component != nullptr);

        m_map[id] = std::move(component);
    }

    template <class T>
    T *Get() const
    {
        const auto id = GetComponentId<T>();

        const auto it = m_map.Find(id);

        if (it == m_map.End()) {
            return nullptr;
        }

        return static_cast<T *>(it->second.get());
    }

    Component *Get(ComponentId id) const
    {
        const auto it = m_map.Find(id);

        if (it == m_map.End()) {
            return nullptr;
        }

        return it->second.get();
    }

    template <class T>
    bool Has() const
    {
        const auto id = GetComponentId<T>();

        return m_map.Find(id) != m_map.End();
    }
    
    bool Has(ComponentId id) const
    {
        return m_map.Find(id) != m_map.End();
    }
    
    template <class T>
    bool Remove()
    {
        const auto id = GetComponentId<T>();

        const auto it = m_map.Find(id);

        if (it == m_map.End()) {
            return false;
        }

        m_map.Erase(it);

        return true;
    }
    
    bool Remove(ComponentId id)
    {
        const auto it = m_map.Find(id);

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

    HYP_DEF_STL_BEGIN_END(m_map.begin(), m_map.end());

private:
    Map m_map;
};

} // namespace hyperion::v2

#endif