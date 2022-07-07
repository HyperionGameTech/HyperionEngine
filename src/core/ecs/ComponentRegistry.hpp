#ifndef HYPERION_V2_ECS_COMPONENT_REGISTRY_H
#define HYPERION_V2_ECS_COMPONENT_REGISTRY_H

#include <core/Containers.hpp>
#include <core/lib/TypeMap.hpp>
#include <core/lib/FlatMap.hpp>
#include <Types.hpp>

#include <system/Debug.hpp>

namespace hyperion::v2 {

class ComponentMapBase {

};

template <class Entity, class Component>
class ComponentMap : public ComponentMapBase {
public:
    using Map           = FlatMap<typename Entity::ID, Component>;
    using Iterator      = typename Map::Iterator;
    using ConstIterator = typename Map::ConstIterator;

    Component &Get(typename Entity::ID id)              { return m_components.At(id); }
    const Component &Get(typename Entity::ID id) const  { return m_components.At(id); }

    Iterator Find(typename Entity::ID id)               { return m_components.Find(id); }
    ConstIterator Find(typename Entity::ID id) const    { return m_components.Find(id); }

    bool Has(typename Entity::ID id) const              { return m_components.Contains(id); }

    void Set(typename Entity::ID id, Component &&value) { m_components[id] = std::move(value); }

    bool Remove(typename Entity::ID id)                 { return m_components.Erase(id); }

    HYP_DEF_STL_ITERATOR(m_components)

private:
    Map m_components;
};

template <class Entity>
class ComponentRegistry {
public:
    ComponentRegistry() = default;
    ComponentRegistry(const ComponentRegistry &other) = delete;
    ComponentRegistry &operator=(const ComponentRegistry &other) = delete;
    ~ComponentRegistry() = default;

    template <class Component>
    void Register()
    {
        AssertThrowMsg(!m_component_maps.Contains<Component>(), "Component already registered!");

        m_component_maps.Set<Component>(std::make_unique<ComponentMap<Entity, Component>>());
    }

    template <class Component>
    void AddComponent(typename Entity::ID id, Component &&component)
    {
        m_component_maps.At<Component>().Set(id, std::forward<Component>(component));
    }

    template <class Component>
    bool HasComponent(typename Entity::ID id)
    {
        return m_component_maps.At<Component>().Has(id);
    }

    template <class Component>
    Component *GetComponent(typename Entity::ID id)
    {
        auto it = m_component_maps.At<Component>().Find(id);

        if (it == m_component_maps.At<Component>().End()) {
            return nullptr;
        }

        return &it->second;
    }

    template <class Component>
    void RemoveComponent(typename Entity::ID id)
    {
        m_component_maps.At<Component>().Remove(id);
    }
    
private:
    TypeMap<std::unique_ptr<ComponentMapBase>> m_component_maps;
};

} // namespace hyperion::v2

#endif