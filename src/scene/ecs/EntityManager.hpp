#ifndef HYPERION_V2_ECS_ENTITY_MANAGER_HPP
#define HYPERION_V2_ECS_ENTITY_MANAGER_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/FlatSet.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/lib/TypeMap.hpp>
#include <core/lib/Proc.hpp>
#include <core/lib/Mutex.hpp>
#include <core/Handle.hpp>
#include <core/ID.hpp>
#include <scene/Entity.hpp>
#include <scene/ecs/EntitySet.hpp>
#include <scene/ecs/EntityContainer.hpp>
#include <scene/ecs/ComponentContainer.hpp>
#include <scene/ecs/ComponentInterface.hpp>
#include <scene/ecs/System.hpp>

namespace hyperion::v2 {

class Scene;

/*! \brief A group of Systems that are able to be processed concurrently, as they do not share any dependencies.
 */
class SystemExecutionGroup
{
public:
    SystemExecutionGroup()                                              = default;
    SystemExecutionGroup(const SystemExecutionGroup &)                  = delete;
    SystemExecutionGroup &operator=(const SystemExecutionGroup &)       = delete;
    SystemExecutionGroup(SystemExecutionGroup &&) noexcept              = default;
    SystemExecutionGroup &operator=(SystemExecutionGroup &&) noexcept   = default;
    ~SystemExecutionGroup()                                             = default;

    /*! \brief Checks if the SystemExecutionGroup is valid for the given System.
     *
     *  \param[in] system The System to check.
     *
     *  \return True if the SystemExecutionGroup is valid for the given System, false otherwise.
     */
    Bool IsValidForExecutionGroup(const SystemBase *system) const
    {
        AssertThrow(system != nullptr);

        const Array<TypeID> &component_type_ids = system->GetComponentTypeIDs();

        for (const auto &it : m_systems) {
            for (TypeID component_type_id : component_type_ids) {
                const ComponentRWFlags rw_flags = system->GetComponentRWFlags(component_type_id);

                // This System is read-only for this component, so it can be processed with other Systems
                if (rw_flags == COMPONENT_RW_FLAGS_READ) {
                    if (it.second->HasComponentTypeID(component_type_id, false)) {
                        return false;
                    }
                } else {
                    if (it.second->HasComponentTypeID(component_type_id, true)) {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    /*! \brief Checks if the SystemExecutionGroup has a System of the given type.
     *
     *  \tparam System The type of the System to check for.
     *
     *  \return True if the SystemExecutionGroup has a System of the given type, false otherwise.
     */
    template <class SystemType>
    Bool HasSystem() const
    {
        const TypeID id = TypeID::ForType<SystemType>();

        return m_systems.Find(id) != m_systems.End();
    }

    /*! \brief Adds a System to the SystemExecutionGroup.
     *
     *  \tparam System The type of the System to add.
     *
     *  \param[in] system The System to add.
     */
    template <class SystemType>
    SystemBase *AddSystem(UniquePtr<SystemType> &&system)
    {
        AssertThrow(system != nullptr);
        AssertThrowMsg(IsValidForExecutionGroup(system.Get()), "System is not valid for this SystemExecutionGroup");

        auto it = m_systems.Find<SystemType>();
        AssertThrowMsg(it == m_systems.End(), "System already exists");

        auto insert_result = m_systems.Set<SystemType>(std::move(system));

        return insert_result.first->second.Get();
    }

    /*! \brief Removes a System from the SystemExecutionGroup.
     *
     *  \tparam System The type of the System to remove.
     *
     *  \return True if the System was removed, false otherwise.
     */
    template <class SystemType>
    Bool RemoveSystem()
    {
        const TypeID id = TypeID::ForType<SystemType>();

        return m_systems.Erase(id);
    }

    /*! \brief Processes all Systems in the SystemExecutionGroup.
     *
     *  \param[in] entity_manager The EntityManager to use.
     *  \param[in] delta The delta time to use.
     */
    void Process(EntityManager &entity_manager, GameCounter::TickUnit delta);

private:
    TypeMap<UniquePtr<SystemBase>> m_systems;
};

using EntityListenerID = UInt;

struct EntityListener
{
    static constexpr EntityListenerID invalid_id = 0;

    EntityListener()                                        = default;

    EntityListener(Proc<void, ID<Entity>> &&on_entity_added, Proc<void, ID<Entity>> &&on_entity_removed)
        : on_entity_added(std::move(on_entity_added)),
          on_entity_removed(std::move(on_entity_removed))
    {
    }

    EntityListener(const EntityListener &)                  = delete;
    EntityListener &operator=(const EntityListener &)       = delete;
    EntityListener(EntityListener &&) noexcept              = default;
    EntityListener &operator=(EntityListener &&) noexcept   = default;
    ~EntityListener()                                       = default;

    Proc<void, ID<Entity>> on_entity_added;
    Proc<void, ID<Entity>> on_entity_removed;

    void OnEntityAdded(ID<Entity> id)
    {
        if (on_entity_added) {
            on_entity_added(id);
        }
    }

    void OnEntityRemoved(ID<Entity> id)
    {
        if (on_entity_removed) {
            on_entity_removed(id);
        }
    }
};

class EntityManager
{
    static struct ComponentSetMutexHolder
    {
        struct MutexMap
        {
            // Mutex for accessing the map
            Mutex                       mutex;

            // Maps component type ID to mutex, used for accessing components via dynamic TypeID
            HashMap<TypeID, Mutex *>    map;

            MutexMap()                                  = default;
            MutexMap(const MutexMap &)                  = delete;
            MutexMap &operator=(const MutexMap &)       = delete;
            MutexMap(MutexMap &&) noexcept              = delete;
            MutexMap &operator=(MutexMap &&) noexcept   = delete;
            ~MutexMap()                                 = default;
        };

        struct MutexDeclaration
        {
            Mutex mutex;

            MutexDeclaration(TypeID type_id, MutexMap &mutex_map)
            {
                Mutex::Guard guard(mutex_map.mutex);

                mutex_map.map.Set(type_id, &mutex);
            }

            MutexDeclaration(const MutexDeclaration &)                  = delete;
            MutexDeclaration &operator=(const MutexDeclaration &)       = delete;
            MutexDeclaration(MutexDeclaration &&) noexcept              = delete;
            MutexDeclaration &operator=(MutexDeclaration &&) noexcept   = delete;
            ~MutexDeclaration()                                         = default;
        };

        static MutexMap s_mutex_map;

        template <class Component>
        static Mutex &GetMutex()
        {
            static MutexDeclaration mutex_declaration(TypeID::ForType<Component>(), s_mutex_map);

            return mutex_declaration.mutex;
        }

        static Mutex *TryGetMutex(TypeID component_type_id)
        {
            Mutex::Guard guard(s_mutex_map.mutex);

            auto it = s_mutex_map.map.Find(component_type_id);

            if (it == s_mutex_map.map.End()) {
                return nullptr;
            }

            return it->second;
        }
    } s_component_set_mutex_holder;

public:
    EntityManager(Scene *scene)
        : m_scene(scene)
    {
        AssertThrow(scene != nullptr);
    }

    EntityManager(const EntityManager &)                = delete;
    EntityManager &operator=(const EntityManager &)     = delete;
    EntityManager(EntityManager &&) noexcept            = delete;
    EntityManager &operator=(EntityManager &&) noexcept = delete;
    ~EntityManager()                                    = default;

    Scene *GetScene() const
        { return m_scene; }

    ID<Entity> AddEntity();
    void RemoveEntity(ID<Entity> id);

    /*! \brief Moves an entity from one EntityManager to another.
     *  This is useful for moving entities between scenes.
     *  All components will be moved to the other EntityManager.
     *
     *  \param[in] id The ID of the entity to move.
     *  \param[in] other The EntityManager to move the entity to.
     */
    void MoveEntity(ID<Entity> id, EntityManager &other);
    
    HYP_FORCE_INLINE
    bool HasEntity(ID<Entity> id) const
    {
        Mutex::Guard entities_guard(m_entities_mutex);

        return m_entities.Find(id) != m_entities.End();
    }

    template <class Component>
    bool HasComponent(ID<Entity> entity) const
    {
        Mutex::Guard entities_guard(m_entities_mutex);

        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");

        return it->second.HasComponent<Component>();
    }

    bool HasComponent(TypeID component_type_id, ID<Entity> entity)
    {
        Mutex::Guard entities_guard(m_entities_mutex);

        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");

        return it->second.HasComponent(component_type_id);
    }

    template <class Component>
    Component &GetComponent(ID<Entity> entity)
    {
        Mutex::Guard entities_guard(m_entities_mutex);

        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");
        
        AssertThrowMsg(it->second.HasComponent<Component>(), "Entity does not have component");

        const TypeID component_type_id = TypeID::ForType<Component>();
        const ComponentID component_id = it->second.GetComponentID<Component>();

        Mutex::Guard component_set_guard(s_component_set_mutex_holder.GetMutex<Component>());

        auto component_container_it = m_containers.Find(component_type_id);
        AssertThrowMsg(component_container_it != m_containers.End(), "Component container does not exist");

        return static_cast<ComponentContainer<Component> &>(*component_container_it->second).GetComponent(component_id);
    }

    template <class Component>
    const Component &GetComponent(ID<Entity> entity) const
        { return const_cast<EntityManager *>(this)->GetComponent<Component>(entity); }

    template <class Component>
    Component *TryGetComponent(ID<Entity> entity)
    {
        Mutex::Guard entities_guard(m_entities_mutex);

        auto it = m_entities.Find(entity);

        if (it == m_entities.End()) {
            return nullptr;
        }

        if (!it->second.HasComponent<Component>()) {
            return nullptr;
        }

        const TypeID component_type_id = TypeID::ForType<Component>();
        const ComponentID component_id = it->second.GetComponentID<Component>();

        Mutex::Guard component_set_guard(s_component_set_mutex_holder.GetMutex<Component>());

        auto component_container_it = m_containers.Find(component_type_id);
        if (component_container_it == m_containers.End()) {
            return nullptr;
        }

        if (!component_container_it->second->HasComponent(component_id)) {
            return nullptr;
        }

        return &static_cast<ComponentContainer<Component> &>(*component_container_it->second).GetComponent(component_id);
    }

    template <class Component>
    const Component *TryGetComponent(ID<Entity> entity) const
        { return const_cast<EntityManager *>(this)->TryGetComponent<Component>(entity); }

    /*! \brief Gets a component using the dynamic type ID.
     *
     *  \param[in] component_type_id The type ID of the component to get.
     *  \param[in] entity The entity to get the component from.
     *
     *  \return Pointer to the component as a void pointer, or nullptr if the entity does not have the component.
     */
    void *TryGetComponent(TypeID component_type_id, ID<Entity> entity)
    {
        Mutex::Guard entities_guard(m_entities_mutex);

        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");
        
        if (!it->second.HasComponent(component_type_id)) {
            return nullptr;
        }

        const ComponentID component_id = it->second.GetComponentID(component_type_id);

        Mutex *component_set_mutex = s_component_set_mutex_holder.TryGetMutex(component_type_id);

        if (!component_set_mutex) {
            // No mutex found for this component type, so it doesn't exist

            return nullptr;
        }

        Mutex::Guard component_set_guard(*component_set_mutex);

        auto component_container_it = m_containers.Find(component_type_id);
        AssertThrowMsg(component_container_it != m_containers.End(), "Component container does not exist");

        return component_container_it->second->TryGetComponent(component_id);
    }

    /*! \brief Gets a component using the dynamic type ID.
     *
     *  \param[in] component_type_id The type ID of the component to get.
     *  \param[in] entity The entity to get the component from.
     *
     *  \return Pointer to the component as a void pointer, or nullptr if the entity does not have the component.
     */
    const void *TryGetComponent(TypeID component_type_id, ID<Entity> entity) const
        { return const_cast<EntityManager *>(this)->TryGetComponent(component_type_id, entity); }

    // template <class ... Components>
    // std::tuple<Components &...> GetComponents(ID<Entity> entity)
    //     { return std::tuple<Components &...>(GetComponent<Components>(entity)...); }

    // template <class ... Components>
    // std::tuple<const Components &...> GetComponents(ID<Entity> entity) const
    //     { return std::tuple<const Components &...>(GetComponent<Components>(entity)...); }

    ComponentInterfaceBase *GetComponentInterface(TypeID type_id);

    template <class Component>
    ComponentID AddComponent(ID<Entity> entity, Component &&component)
    {
        Mutex::Guard entities_guard(m_entities_mutex);

        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");

        auto component_it = it->second.components.Find<Component>();
        AssertThrowMsg(component_it == it->second.components.End(), "Entity already has component");

        Mutex::Guard component_set_guard(s_component_set_mutex_holder.GetMutex<Component>());

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

        return component_id;
    }

    template <class Component>
    void RemoveComponent(ID<Entity> entity)
    {
        Mutex::Guard entities_guard(m_entities_mutex);

        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");

        auto component_it = it->second.components.Find<Component>();
        AssertThrowMsg(component_it != it->second.components.End(), "Entity does not have component");

        Mutex::Guard component_set_guard(s_component_set_mutex_holder.GetMutex<Component>());

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
        Mutex::Guard guard(m_entity_sets_mutex);

        const EntitySetTypeID type_id = EntitySet<Components...>::type_id;
        
        auto entity_sets_it = m_entity_sets.Find(type_id);

        if (entity_sets_it == m_entity_sets.End()) {
            auto entity_sets_insert_result = m_entity_sets.Set(type_id, UniquePtr<EntitySet<Components...>>::Construct(
                m_entities,
                GetContainer<Components>()...
            ));

            AssertThrow(entity_sets_insert_result.second); // Make sure the element was inserted (it shouldn't already exist)

            entity_sets_it = entity_sets_insert_result.first;

            // Make sure the element exists in m_component_entity_sets
            for (TypeID component_type_id : { TypeID::ForType<Components>()... }) {
                auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

                if (component_entity_sets_it == m_component_entity_sets.End()) {
                    auto component_entity_sets_insert_result = m_component_entity_sets.Set(component_type_id, { });

                    component_entity_sets_it = component_entity_sets_insert_result.first;
                }

                component_entity_sets_it->second.Insert(type_id);
            }
        }

        return static_cast<EntitySet<Components...> &>(*entity_sets_it->second);
    }

    template <class SystemType, class ...Args>
    SystemType *AddSystem(Args &&... args)
    {
        return static_cast<SystemType *>(AddSystemToExecutionGroup(UniquePtr<SystemType>::Construct(std::forward<Args>(args)...)));
    }

    void Update(GameCounter::TickUnit delta);

private:
    template <class Component>
    ComponentContainer<Component> &GetContainer()
    {
        auto it = m_containers.Find<Component>();

        if (it == m_containers.End()) {
            it = m_containers.Set<Component>(UniquePtr<ComponentContainer<Component>>::Construct()).first;
        }

        return static_cast<ComponentContainer<Component> &>(*it->second);
    }

    ComponentContainerBase *TryGetContainer(TypeID component_type_id)
    {
        auto it = m_containers.Find(component_type_id);
        
        if (it == m_containers.End()) {
            return nullptr;
        }

        return it->second.Get();
    }

    template <class SystemType>
    SystemType *AddSystemToExecutionGroup(UniquePtr<SystemType> &&system)
    {
        AssertThrow(system != nullptr);

        if (m_system_execution_groups.Empty()) {
            m_system_execution_groups.PushBack({ });
        }

        for (auto &system_execution_group : m_system_execution_groups) {
            if (system_execution_group.IsValidForExecutionGroup(system.Get())) {
                return static_cast<SystemType *>(system_execution_group.AddSystem<SystemType>(std::move(system)));
            }
        }

        return static_cast<SystemType *>(m_system_execution_groups.PushBack({ }).AddSystem<SystemType>(std::move(system)));
    }

    Scene                                                                   *m_scene;

    TypeMap<UniquePtr<ComponentContainerBase>>                              m_containers;
    EntityContainer                                                         m_entities;
    mutable Mutex                                                           m_entities_mutex;
    FlatMap<EntitySetTypeID, UniquePtr<EntitySetBase>>                      m_entity_sets;
    mutable Mutex                                                           m_entity_sets_mutex;
    TypeMap<FlatSet<EntitySetTypeID>>                                       m_component_entity_sets;
    FlatMap<EntitySetTypeID, FlatMap<EntityListenerID, EntityListener>>     m_entity_listeners;
    Array<SystemExecutionGroup>                                             m_system_execution_groups;
};

} // namespace hyperion::v2

#endif