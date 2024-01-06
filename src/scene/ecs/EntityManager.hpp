#ifndef HYPERION_V2_ECS_ENTITY_MANAGER_HPP
#define HYPERION_V2_ECS_ENTITY_MANAGER_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/FlatSet.hpp>
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

    ID<Entity> AddEntity();
    void RemoveEntity(ID<Entity> id);
    
    HYP_FORCE_INLINE
    Bool HasEntity(ID<Entity> id) const
        { return m_entities.Find(id) != m_entities.End(); }

    template <class Component>
    Bool HasComponent(ID<Entity> entity) const
    {
        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");

        return it->second.HasComponent<Component>();
    }

    template <class Component>
    Component &GetComponent(ID<Entity> entity)
    {
        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");
        
        AssertThrowMsg(it->second.HasComponent<Component>(), "Entity does not have component");

        const TypeID component_type_id = TypeID::ForType<Component>();
        const ComponentID component_id = it->second.GetComponentID<Component>();

        auto component_container_it = m_containers.Find(component_type_id);
        AssertThrowMsg(component_container_it != m_containers.End(), "Component container does not exist");

        return static_cast<ComponentContainer<Component> &>(*component_container_it->second).GetComponent(component_id);
    }

    template <class Component>
    const Component &GetComponent(ID<Entity> entity) const
        { return const_cast<EntityManager *>(this)->GetComponent<Component>(entity); }

    template <class ... Components>
    std::tuple<Components &...> GetComponents(ID<Entity> entity)
        { return std::tuple<Components &...>(GetComponent<Components>(entity)...); }

    template <class ... Components>
    std::tuple<const Components &...> GetComponents(ID<Entity> entity) const
        { return std::tuple<const Components &...>(GetComponent<Components>(entity)...); }

    template <class Component>
    void AddComponent(ID<Entity> entity, Component &&component)
    {
        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");

        auto component_it = it->second.components.Find<Component>();
        AssertThrowMsg(component_it == it->second.components.End(), "Entity already has component");

        const TypeID component_type_id = TypeID::ForType<Component>();
        const ComponentID component_id = GetContainer<Component>().AddComponent(std::move(component));

        auto components_insert_result = it->second.components.Set<Component>(component_id);
        component_it = components_insert_result.first;

        auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

        if (component_entity_sets_it != m_component_entity_sets.End()) {
            for (EntitySetTypeID entity_set_id : component_entity_sets_it->second) {
                auto entity_set_it = m_entity_sets.Find(entity_set_id);
                AssertThrowMsg(entity_set_it != m_entity_sets.End(), "Entity set does not exist");

                entity_set_it->second->OnEntityUpdated(entity);
            }
        }
    }

    template <class Component>
    void RemoveComponent(ID<Entity> entity)
    {
        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");

        auto component_it = it->second.components.Find<Component>();
        AssertThrowMsg(component_it != it->second.components.End(), "Entity does not have component");

        const TypeID component_type_id = component_it->first;
        const ComponentID component_id = component_it->second;

        GetContainer<Component>().RemoveComponent(component_id);

        it->second.components.Erase(component_it);

        auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

        if (component_entity_sets_it != m_component_entity_sets.End()) {
            for (EntitySetTypeID entity_set_id : component_entity_sets_it->second) {
                auto entity_set_it = m_entity_sets.Find(entity_set_id);
                AssertThrowMsg(entity_set_it != m_entity_sets.End(), "Entity set does not exist");

                entity_set_it->second->OnEntityUpdated(entity);
            }
        }
    }

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
        
        auto entity_sets_it = m_entity_sets.Find(type_id);

        if (entity_sets_it == m_entity_sets.End()) {
            auto entity_sets_insert_result = m_entity_sets.Set(type_id, UniquePtr<EntitySet<Components...>>::Construct(
                m_entities,
                GetContainer<Components>()...
            ));

            // Make sure the element exists in m_component_entity_sets
            for (TypeID component_type_id : { TypeID::ForType<Components>()... }) {
                auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

                if (component_entity_sets_it == m_component_entity_sets.End()) {
                    auto component_entity_sets_insert_result = m_component_entity_sets.Set(component_type_id, { });

                    component_entity_sets_it = component_entity_sets_insert_result.first;
                }

                component_entity_sets_it->second.Insert(type_id);
            }

            entity_sets_it = entity_sets_insert_result.first;
        }

        return static_cast<EntitySet<Components...> &>(*entity_sets_it->second);
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
    TypeMap<FlatSet<EntitySetTypeID>>                   m_component_entity_sets;
    TypeMap<UniquePtr<SystemBase>>                      m_systems;
};

} // namespace hyperion::v2

#endif