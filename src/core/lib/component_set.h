#ifndef HYPERION_V2_LIB_COMPONENT_SET_H
#define HYPERION_V2_LIB_COMPONENT_SET_H

namespace hyperion::v2 {


template <class Component>
class ComponentSet {
    using ComponentId = uint;
    using Map         = FlatMap<ComponentId, std::unique_ptr<Component>>;

    static std::atomic<ComponentId> id_counter;

    template <class T>
    inline static ComponentId GetComponentId()
    {
        static_assert(std::is_base_of_v<Component, T>, "Object must be a derived class of Component");

        static const ComponentId id = ++id_counter;

        return id;
    }

public:
    using Iterator      = typename Map::Iterator;
    using ConstIterator = typename Map::ConstIterator;

    ComponentSet() = default;
    ComponentSet(const ComponentSet &other) = delete;
    ComponentSet &operator=(const ComponentSet &other) = delete;

    ComponentSet(ComponentSet &&other) noexcept
        : m_map(std::move(other.m_map))
    {
    }

    ComponentSet &operator=(ComponentSet &&other) noexcept
    {
        m_map = std::move(other.m_map);

        return *this;
    }

    ~ComponentSet() = default;
    
    template <class T>
    void Set(std::unique_ptr<T> &&component)
    {
        AssertThrow(component != nullptr);

        const auto id = GetComponentId<T>();

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

    template <class T>
    bool Has() const
    {
        const auto id = GetComponentId<T>();

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

    HYP_DEF_STL_BEGIN_END(m_map.begin(), m_map.end());

private:
    Map m_map;
};
} // namespace hyperion::v2

#endif