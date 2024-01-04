#ifndef HYPERION_V2_ECS_ENTITY_MANAGER_HPP
#define HYPERION_V2_ECS_ENTITY_MANAGER_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/lib/TypeMap.hpp>
#include <core/Handle.hpp>
#include <scene/Entity.hpp>
#include <scene/ecs/EntitySet.hpp>
#include <scene/ecs/EntityContainer.hpp>
#include <scene/ecs/ComponentContainer.hpp>
#include <scene/ecs/System.hpp>

namespace hyperion::v2 {

class EntityManager
{
    static EntityManager *s_instance;

public:
    static EntityManager &GetInstance()
    {
        if (!s_instance) {
            s_instance = new EntityManager();
        }

        return *s_instance;
    }

    EntityManager()                                     = default;
    EntityManager(const EntityManager &)                = delete;
    EntityManager &operator=(const EntityManager &)     = delete;
    EntityManager(EntityManager &&) noexcept            = delete;
    EntityManager &operator=(EntityManager &&) noexcept = delete;
    ~EntityManager()                                    = default;

    ID<Entity> CreateEntity();

    void RemoveEntity(ID<Entity> id)
    {
        auto it = m_entities.Find(id);

        if (it != m_entities.End()) {
            m_entities.Erase(it);

            for (auto &pair : m_containers) {
                pair.second->RemoveComponent(id);
            }
        }
    }

    template <class Component>
    Component &GetComponent(ID<Entity> entity)
        { return GetContainer<Component>().GetComponent(entity); }

    template <class Component>
    const Component &GetComponent(ID<Entity> entity) const
        { return GetContainer<Component>().GetComponent(entity); }

    template <class ... Components>
    std::tuple<Components &...> GetComponents(ID<Entity> entity)
        { return std::tuple<Components &...>(GetContainer<Components>().GetComponent(entity)...); }

    template <class ... Components>
    std::tuple<const Components &...> GetComponents(ID<Entity> entity) const
        { return std::tuple<const Components &...>(GetContainer<Components>().GetComponent(entity)...); }

    template <class Component>
    void AddComponent(ID<Entity> entity, Component &&component)
        { GetContainer<Component>().AddComponent(entity, std::move(component)); }

    template <class Component>
    Bool RemoveComponent(ID<Entity> entity)
        { return GetContainer<Component>().RemoveComponent(entity); }

    template <class Component>
    ComponentContainer<Component> &GetContainer()
    {
        auto it = m_containers.Find<Component>();

        if (it == m_containers.End()) {
            it = m_containers.Set<Component>(UniquePtr<ComponentContainer<Component>>::Construct()).first;
        }

        return static_cast<ComponentContainer<Component> &>(*it->second);
    }

    /*! \brief Gets an entity set with the specified components, creating it if it doesn't exist.
     *
     *  \tparam Components The components that the entities in this set have.
     *
     *  \param[in] entities The entity container to use.
     *  \param[in] components The component containers to use.
     *
     *  \return Reference to the entity set.
     */
    template <class ... Components>
    EntitySet<Components...> &GetEntitySet()
    {
        const EntitySetTypeID type_id = EntitySet<Components...>::type_id;

        if (!m_entity_sets.Contains(type_id)) {
            m_entity_sets.Set(type_id, UniquePtr<EntitySet<Components...>>::Construct(
                m_entities,
                GetContainer<Components>()...
            ));
        }

        return static_cast<EntitySet<Components...> &>(*m_entity_sets.At(type_id));
    }

    template <class System, class ...Args>
    System *AddSystem(Args &&... args)
    {
        const auto id = TypeID::ForType<System>();

        if (m_systems.Contains(id)) {
            return nullptr;
        }

        m_systems.Set<System>(UniquePtr<System>::Construct(std::forward<Args>(args)...));

        return static_cast<System *>(m_systems.At<System>().Get());
    }

    void Update(GameCounter::TickUnit delta);

private:
    TypeMap<UniquePtr<ComponentContainerBase>>          m_containers;
    EntityContainer                                     m_entities;
    FlatMap<EntitySetTypeID, UniquePtr<EntitySetBase>>  m_entity_sets;
    TypeMap<UniquePtr<SystemBase>>                      m_systems;
};

} // namespace hyperion::v2

#endif