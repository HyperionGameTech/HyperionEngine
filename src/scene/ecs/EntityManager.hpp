/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENTITY_MANAGER_HPP
#define HYPERION_ECS_ENTITY_MANAGER_HPP

#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/FlatSet.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>
#include <core/containers/TypeMap.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/AnyRef.hpp>

#include <core/functional/Proc.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/Semaphore.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/utilities/Tuple.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/ForEach.hpp>

#include <core/object/HypObject.hpp>

#include <core/profiling/PerformanceClock.hpp>

#include <core/Handle.hpp>
#include <core/ID.hpp>

#include <scene/Entity.hpp>
#include <scene/ecs/EntitySet.hpp>
#include <scene/ecs/EntityContainer.hpp>
#include <scene/ecs/ComponentContainer.hpp>
#include <scene/ecs/System.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <GameCounter.hpp>

namespace hyperion {

namespace threading {
struct TaskBatch;
} // namespace threading

using threading::TaskBatch;

enum class EntityManagerFlags : uint32
{
    NONE = 0x0,
    PARALLEL_SYSTEM_EXECUTION = 0x1,

    DEFAULT = PARALLEL_SYSTEM_EXECUTION
};

HYP_MAKE_ENUM_FLAGS(EntityManagerFlags)

class World;
class Scene;
struct HypData;
class Node;

static constexpr uint32 g_move_entity_write_flag = 0x1u;
static constexpr uint32 g_move_entity_read_mask = ~0u << 1;

/*! \brief A group of Systems that are able to be processed concurrently, as they do not share any dependencies.
 */
class HYP_API SystemExecutionGroup
{
public:
    SystemExecutionGroup(bool requires_game_thread = false, bool allow_update = true);
    SystemExecutionGroup(const SystemExecutionGroup&) = delete;
    SystemExecutionGroup& operator=(const SystemExecutionGroup&) = delete;
    SystemExecutionGroup(SystemExecutionGroup&&) noexcept = default;
    SystemExecutionGroup& operator=(SystemExecutionGroup&&) noexcept = default;
    ~SystemExecutionGroup();

    HYP_FORCE_INLINE bool RequiresGameThread() const
    {
        return m_requires_game_thread;
    }

    HYP_FORCE_INLINE bool AllowUpdate() const
    {
        return m_allow_update;
    }

    HYP_FORCE_INLINE TypeMap<Handle<SystemBase>>& GetSystems()
    {
        return m_systems;
    }

    HYP_FORCE_INLINE const TypeMap<Handle<SystemBase>>& GetSystems() const
    {
        return m_systems;
    }

    HYP_FORCE_INLINE TaskBatch* GetTaskBatch() const
    {
        return m_task_batch.Get();
    }

#ifdef HYP_DEBUG_MODE
    HYP_FORCE_INLINE const PerformanceClock& GetPerformanceClock() const
    {
        return m_performance_clock;
    }

    HYP_FORCE_INLINE const FlatMap<SystemBase*, PerformanceClock>& GetPerformanceClocks() const
    {
        return m_performance_clocks;
    }
#endif

    /*! \brief Checks if the SystemExecutionGroup is valid for the given System.
     *
     *  \param[in] system_ptr The System to check.
     *
     *  \return True if the SystemExecutionGroup is valid for the given System, false otherwise.
     */
    bool IsValidForSystem(const SystemBase* system_ptr) const
    {
        AssertThrow(system_ptr != nullptr);

        // If the system does not allow update calls, and we don't as well, return true as there will be no overlap.
        if (!AllowUpdate())
        {
            return !system_ptr->AllowUpdate();
        }

        // If the system requires to execute on game thread and the SystemExecutionGroup does not, it is not valid
        // and if the system does not require to execute on game thread and the SystemExecutionGroup does, it is not valid (it could be better parallelized)
        if (system_ptr->RequiresGameThread() != RequiresGameThread())
        {
            return false;
        }

        const Array<TypeID>& component_type_ids = system_ptr->GetComponentTypeIDs();

        for (const auto& it : m_systems)
        {
            const SystemBase* other_system = it.second.Get();

            for (TypeID component_type_id : component_type_ids)
            {
                const ComponentInfo& component_info = system_ptr->GetComponentInfo(component_type_id);

                if (component_info.rw_flags & COMPONENT_RW_FLAGS_WRITE)
                {
                    if (other_system->HasComponentTypeID(component_type_id, true))
                    {
                        return false;
                    }
                }
                else
                {
                    // This System is read-only for this component, so it can be processed with other Systems
                    if (other_system->HasComponentTypeID(component_type_id, false))
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    /*! \brief Checks if the SystemExecutionGroup has a System of the given type.
     *
     *  \tparam SystemType The type of the System to check for.
     *
     *  \return True if the SystemExecutionGroup has a System of the given type, false otherwise.
     */
    template <class SystemType>
    HYP_FORCE_INLINE bool HasSystem() const
    {
        static const TypeID type_id = TypeID::ForType<SystemType>();

        return m_systems.Find(type_id) != m_systems.End();
    }

    /*! \brief Adds a System to the SystemExecutionGroup.
     *
     *  \param[in] system The System to add.
     */
    Handle<SystemBase> AddSystem(const Handle<SystemBase>& system)
    {
        AssertThrow(system.IsValid());
        AssertThrowMsg(IsValidForSystem(system.Get()), "System is not valid for this SystemExecutionGroup");

        auto it = m_systems.Find(system->GetTypeID());
        AssertThrowMsg(it == m_systems.End(), "System already exists");

        auto insert_result = m_systems.Set(system->GetTypeID(), system);

        return insert_result.first->second;
    }

    template <class SystemType>
    Handle<SystemType> GetSystem() const
    {
        static const TypeID type_id = TypeID::ForType<SystemType>();

        const auto it = m_systems.Find(type_id);

        if (it == m_systems.End() || !it->second.IsValid())
        {
            return Handle<SystemType>::empty;
        }

        if (!IsInstanceOfHypClass<SystemType>(*it->second))
        {
            HYP_BREAKPOINT;
            return Handle<SystemType>::empty;
        }

        return Handle<SystemType>(it->second);
    }

    /*! \brief Removes a System from the SystemExecutionGroup.
     *
     *  \tparam SystemType The type of the System to remove.
     *
     *  \return True if the System was removed, false otherwise.
     */
    template <class SystemType>
    bool RemoveSystem()
    {
        static const TypeID type_id = TypeID::ForType<SystemType>();

        return m_systems.Erase(type_id);
    }

    /*! \brief Start processing all Systems in the SystemExecutionGroup.
     *
     *  \param[in] delta The delta time value
     */
    void StartProcessing(float delta);

    /*! \brief Waits on all processing tasks to complete */
    void FinishProcessing(bool execute_blocking = false);

private:
    bool m_requires_game_thread;
    bool m_allow_update;

    TypeMap<Handle<SystemBase>> m_systems;
    UniquePtr<TaskBatch> m_task_batch;

#ifdef HYP_DEBUG_MODE
    PerformanceClock m_performance_clock;
    FlatMap<SystemBase*, PerformanceClock> m_performance_clocks;
#endif
};

class EntityManager;
/*! \brief The EntityManager is responsible for managing Entities, their components, and Systems within a Scene. */
HYP_CLASS()
class HYP_API EntityManager final : public HypObject<EntityManager>
{
    HYP_OBJECT_BODY(EntityManager);

    friend class EntityToEntityManagerMap;

    // Allow Entity destructor to call RemoveEntity().
    friend class Entity;

public:
    static constexpr ComponentID invalid_component_id = 0;

    EntityManager(const ThreadID& owner_thread_id, Scene* scene, EnumFlags<EntityManagerFlags> flags = EntityManagerFlags::DEFAULT);
    EntityManager(const EntityManager&) = delete;
    EntityManager& operator=(const EntityManager&) = delete;
    EntityManager(EntityManager&&) noexcept = delete;
    EntityManager& operator=(EntityManager&&) noexcept = delete;
    ~EntityManager();

    template <class Component>
    static bool IsValidComponentType()
    {
        return IsValidComponentType(TypeID::ForType<Component>());
    }

    static bool IsValidComponentType(TypeID component_type_id);

    template <class Component>
    static bool IsEntityTagComponent()
    {
        return IsEntityTagComponent(TypeID::ForType<Component>());
    }

    static bool IsEntityTagComponent(TypeID component_type_id);
    static bool IsEntityTagComponent(TypeID component_type_id, EntityTag& out_tag);

    template <class Component>
    static ANSIStringView GetComponentTypeName()
    {
        return GetComponentTypeName(TypeID::ForType<Component>());
    }

    static ANSIStringView GetComponentTypeName(TypeID component_type_id);

    /*! \brief Gets the thread mask of the thread that owns this EntityManager.
     *
     *  \return The thread mask.
     */
    HYP_FORCE_INLINE const ThreadID& GetOwnerThreadID() const
    {
        return m_owner_thread_id;
    }

    /*! \brief Sets the thread mask of the thread that owns this EntityManager.
     *  \internal This is used by the Scene to set the thread mask of the Scene's thread. It should not be called from user code. */
    HYP_FORCE_INLINE void SetOwnerThreadID(const ThreadID& owner_thread_id)
    {
        m_owner_thread_id = owner_thread_id;
    }

    /*! \brief Gets the World that this EntityManager is associated with.
     *
     *  \return Pointer to the World.
     */
    HYP_METHOD()
    HYP_FORCE_INLINE World* GetWorld() const
    {
        return m_world;
    }

    void SetWorld(World* world);

    /*! \brief Gets the Scene that this EntityManager is associated with.
     *
     *  \return Pointer to the Scene.
     */
    HYP_METHOD()
    HYP_FORCE_INLINE Scene* GetScene() const
    {
        return m_scene;
    }

    HYP_FORCE_INLINE EntityContainer& GetEntities()
    {
        return m_entities;
    }

    HYP_FORCE_INLINE const EntityContainer& GetEntities() const
    {
        return m_entities;
    }

    /*! \brief Adds a new entity to the EntityManager.
     *  \note Must be called from the owner thread.
     *
     *  \return The Entity that was added. */
    HYP_FORCE_INLINE Handle<Entity> AddEntity()
    {
        return AddBasicEntity();
    }

    /*! \brief Adds a new entity to the EntityManager.
     *  \note Must be called from the owner thread.
     *
     *  \tparam T The type of the Entity to add. Must be a subclass of Entity.
     * 
     *  \param [in] args The arguments to pass to the Entity constructor.
     *
     *  \return The Entity that was added. */
    template <class T, class... Args>
    HYP_NODISCARD HYP_FORCE_INLINE Handle<T> AddEntity(Args&&... args)
    {
        static_assert(std::is_base_of_v<Entity, T>, "T must be a subclass of Entity");

        Handle<T> entity = CreateObject<T>(std::forward<Args>(args)...);
        AssertThrowMsg(entity.IsValid(), "Failed to create instance of Entity subclass %s", TypeNameWithoutNamespace<T>().Data());

        AddExistingEntity(entity);

        return entity;
    }

    /*! \brief Adds an existing entity to the EntityManager. */
    HYP_METHOD()
    HYP_FORCE_INLINE void AddExistingEntity(const Handle<Entity>& entity)
    {
        AddExistingEntity_Internal(entity);
    }

    HYP_METHOD()
    Handle<Entity> AddTypedEntity(const HypClass* hyp_class);

    /*! \brief Moves an entity from one EntityManager to another.
     *  This is useful for moving entities between scenes.
     *  All components will be moved to the other EntityManager.
     *
     *  \param[in] entity The Entity to move.
     *  \param[in] other The EntityManager to move the entity to.
     */
    void MoveEntity(const Handle<Entity>& entity, const Handle<EntityManager>& other);

    HYP_FORCE_INLINE bool HasEntity(const Entity* entity) const
    {
        Threads::AssertOnThread(m_owner_thread_id);

        if (!entity)
        {
            return false;
        }

        return m_entities.Find(entity) != m_entities.End();
    }

    void AddTag(Entity* entity, EntityTag tag);
    bool RemoveTag(Entity* entity, EntityTag tag);
    bool HasTag(const Entity* entity, EntityTag tag) const;

    template <EntityTag Tag>
    HYP_FORCE_INLINE bool HasTag(const Entity* entity) const
    {
        return HasComponent<EntityTagComponent<Tag>>(entity);
    }

    template <EntityTag Tag>
    HYP_FORCE_INLINE void AddTag(Entity* entity)
    {
        if (HasTag<Tag>(entity))
        {
            return;
        }

        AddTag(entity, Tag);
    }

    template <EntityTag... Tag>
    HYP_FORCE_INLINE void AddTags(Entity* entity)
    {
        (AddTag<Tag>(entity), ...);
    }

    template <EntityTag Tag>
    HYP_FORCE_INLINE bool RemoveTag(Entity* entity)
    {
        if (!HasTag<Tag>(entity))
        {
            return false;
        }

        return RemoveComponent<EntityTagComponent<Tag>>(entity);
    }

    HYP_FORCE_INLINE Array<EntityTag> GetSavableTags(const Entity* entity) const
    {
        Array<EntityTag> tags;
        GetTagsHelper(entity, std::make_integer_sequence<uint32, uint32(EntityTag::DESCRIPTOR_MAX) - 2>(), tags);

        return tags;
    }

    HYP_FORCE_INLINE uint32 GetSavableTagsMask(const Entity* entity) const
    {
        uint32 mask = 0;
        GetTagsHelper(entity, std::make_integer_sequence<uint32, uint32(EntityTag::DESCRIPTOR_MAX) - 2>(), mask);

        return mask;
    }

    template <class Component>
    bool HasComponent(const Entity* entity) const
    {
        EnsureValidComponentType<Component>();

        if (!entity)
        {
            return false;
        }

        // Threads::AssertOnThread(m_owner_thread_id);

        HYP_MT_CHECK_READ(m_entities_data_race_detector);

        return m_entities.GetEntityData(entity).HasComponent<Component>();
    }

    bool HasComponent(TypeID component_type_id, const Entity* entity) const
    {
        EnsureValidComponentType(component_type_id);

        if (!entity)
        {
            return false;
        }

        // Threads::AssertOnThread(m_owner_thread_id);

        HYP_MT_CHECK_READ(m_entities_data_race_detector);

        return m_entities.GetEntityData(entity).HasComponent(component_type_id);
    }

    template <class Component>
    HYP_FORCE_INLINE Component& GetComponent(const Entity* entity)
    {
        EnsureValidComponentType<Component>();

        AssertThrowMsg(entity, "Invalid entity");

        // Threads::AssertOnThread(m_owner_thread_id);

        HYP_MT_CHECK_READ(m_entities_data_race_detector);
        HYP_MT_CHECK_READ(m_containers_data_race_detector);

        EntityData* entity_data = m_entities.TryGetEntityData(entity);
        AssertThrowMsg(entity_data != nullptr, "Entity does not exist");

        const Optional<ComponentID> component_id_opt = entity_data->TryGetComponentID<Component>();
        AssertThrowMsg(component_id_opt.HasValue(), "Entity does not have component of type %s", TypeNameWithoutNamespace<Component>().Data());

        static const TypeID component_type_id = TypeID::ForType<Component>();

        auto component_container_it = m_containers.Find(component_type_id);
        AssertThrowMsg(component_container_it != m_containers.End(), "Component container does not exist");

        HYP_MT_CHECK_READ(component_container_it->second->GetDataRaceDetector());

        return static_cast<ComponentContainer<Component>&>(*component_container_it->second).GetComponent(*component_id_opt);
    }

    template <class Component>
    HYP_FORCE_INLINE const Component& GetComponent(const Entity* entity) const
    {
        return const_cast<EntityManager*>(this)->GetComponent<Component>(entity);
    }

    template <class Component>
    Component* TryGetComponent(const Entity* entity)
    {
        EnsureValidComponentType<Component>();

        if (!entity)
        {
            return nullptr;
        }

        // Threads::AssertOnThread(m_owner_thread_id);

        HYP_MT_CHECK_READ(m_entities_data_race_detector);
        HYP_MT_CHECK_READ(m_containers_data_race_detector);

        EntityData* entity_data = m_entities.TryGetEntityData(entity);

        if (!entity_data)
        {
            return nullptr;
        }

        if (!entity_data->HasComponent<Component>())
        {
            return nullptr;
        }

        static const TypeID component_type_id = TypeID::ForType<Component>();

        const Optional<ComponentID> component_id_opt = entity_data->TryGetComponentID<Component>();

        if (!component_id_opt)
        {
            return nullptr;
        }

        auto component_container_it = m_containers.Find(component_type_id);
        if (component_container_it == m_containers.End())
        {
            return nullptr;
        }

        HYP_MT_CHECK_READ(component_container_it->second->GetDataRaceDetector());

        return &static_cast<ComponentContainer<Component>&>(*component_container_it->second).GetComponent(*component_id_opt);
    }

    template <class Component>
    HYP_FORCE_INLINE const Component* TryGetComponent(const Entity* entity) const
    {
        return const_cast<EntityManager*>(this)->TryGetComponent<Component>(entity);
    }

    /*! \brief Gets a component using the dynamic type ID.
     *
     *  \param[in] component_type_id The type ID of the component to get.
     *  \param[in] entity The Entity to get the component from.
     *
     *  \return Pointer to the component as a void pointer, or nullptr if the entity does not have the component.
     */
    AnyRef TryGetComponent(TypeID component_type_id, const Entity* entity)
    {
        EnsureValidComponentType(component_type_id);

        if (!entity)
        {
            return AnyRef::Empty();
        }

        // Threads::AssertOnThread(m_owner_thread_id);

        HYP_MT_CHECK_READ(m_entities_data_race_detector);
        HYP_MT_CHECK_READ(m_containers_data_race_detector);

        EntityData* entity_data = m_entities.TryGetEntityData(entity);

        if (!entity)
        {
            return AnyRef::Empty();
        }

        const Optional<ComponentID> component_id_opt = entity_data->TryGetComponentID(component_type_id);

        if (!component_id_opt)
        {
            return AnyRef::Empty();
        }

        auto component_container_it = m_containers.Find(component_type_id);
        AssertThrowMsg(component_container_it != m_containers.End(), "Component container does not exist");

        return component_container_it->second->TryGetComponent(*component_id_opt);
    }

    /*! \brief Gets a component using the dynamic type ID.
     *
     *  \param[in] component_type_id The type ID of the component to get.
     *  \param[in] entity The entity to get the component from.
     *
     *  \return Pointer to the component as a void pointer, or nullptr if the entity does not have the component.
     */
    HYP_FORCE_INLINE ConstAnyRef TryGetComponent(TypeID component_type_id, const Entity* entity) const
    {
        return const_cast<EntityManager*>(this)->TryGetComponent(component_type_id, entity);
    }

    template <class... Components>
    HYP_FORCE_INLINE Tuple<Components&...> GetComponents(const Entity* entity)
    {
        return Tie(GetComponent<Components>(entity)...);
    }

    template <class... Components>
    HYP_FORCE_INLINE Tuple<const Components&...> GetComponents(const Entity* entity) const
    {
        return Tie(GetComponent<Components>(entity)...);
    }

    /*! \brief Get a map of all component types to respective component IDs for a given Entity.
     *  \param entity The Entity to get the components from
     *  \returns An Optional object holding a reference to the typemap if it exists, otherwise an empty optional. */
    HYP_FORCE_INLINE Optional<const TypeMap<ComponentID>&> GetAllComponents(const Entity* entity) const
    {
        if (!entity)
        {
            return {};
        }

        Threads::AssertOnThread(m_owner_thread_id);

        auto it = m_entities.Find(entity);
        if (it == m_entities.End())
        {
            return {};
        }

        return it->second.components;
    }

    void AddComponent(Entity* entity, const HypData& component_data);
    void AddComponent(Entity* entity, HypData&& component_data);

    bool RemoveComponent(TypeID component_type_id, Entity* entity);

    template <class Component, class U = Component>
    Component& AddComponent(Entity* entity, U&& component)
    {
        EnsureValidComponentType<Component>();

        AssertThrowMsg(entity, "Invalid entity");

        Threads::AssertOnThread(m_owner_thread_id);

        Handle<Entity> entity_handle = entity->HandleFromThis();
        AssertThrow(entity_handle.IsValid());

        EntityData* entity_data = m_entities.TryGetEntityData(entity);
        AssertThrow(entity_data != nullptr);

        Component* component_ptr = nullptr;
        TypeMap<ComponentID> component_ids;

        auto component_it = entity_data->FindComponent<Component>();
        // @TODO: Replace the component if it already exists
        AssertThrowMsg(component_it == entity_data->components.End(), "Entity already has component of type %s", TypeNameWithoutNamespace<Component>().Data());

        static const TypeID component_type_id = TypeID::ForType<Component>();

        const Pair<ComponentID, Component&> component_insert_result = GetContainer<Component>().AddComponent(std::move(component));

        entity_data->components.Set<Component>(component_insert_result.first);

        { // Lock the entity sets mutex
            Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

            auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

            if (component_entity_sets_it != m_component_entity_sets.End())
            {
                for (TypeID entity_set_type_id : component_entity_sets_it->second)
                {
                    EntitySetBase& entity_set = *m_entity_sets.At(entity_set_type_id);

                    entity_set.OnEntityUpdated(entity);
                }
            }
        }

        component_ptr = &component_insert_result.second;
        component_ids = entity_data->components;

        // Note: Call OnComponentAdded on the entity before notifying systems, as systems may remove the component
        EntityTag tag;
        if (IsEntityTagComponent(component_type_id, tag))
        {
            // If the component is an EntityTagComponent, add the tag to the entity
            entity->OnTagAdded(tag);
        }
        else
        {
            // Notify the entity that a component was added
            entity->OnComponentAdded(component_ptr);
        }

        // Notify systems that entity is being added to them
        NotifySystemsOfEntityAdded(entity_handle, component_ids);

        return *component_ptr;
    }

    template <class Component>
    bool RemoveComponent(Entity* entity)
    {
        EnsureValidComponentType<Component>();

        if (!entity)
        {
            return false;
        }

        Handle<Entity> entity_handle = entity->HandleFromThis();
        AssertThrow(entity_handle.IsValid());

        Threads::AssertOnThread(m_owner_thread_id);

        TypeMap<ComponentID> removed_component_ids;

        EntityData* entity_data = m_entities.TryGetEntityData(entity);

        if (!entity_data)
        {
            return false;
        }

        auto component_it = entity_data->FindComponent<Component>();
        if (component_it == entity_data->components.End())
        {
            return false;
        }

        const TypeID component_type_id = component_it->first;
        const ComponentID component_id = component_it->second;

        // Notify systems that entity is being removed from them
        removed_component_ids.Set(component_type_id, component_id);

        HypData component_hyp_data;

        if (!GetContainer<Component>().RemoveComponent(component_id, component_hyp_data))
        {
            return false;
        }

        entity_data->components.Erase(component_it);

        {
            // Lock the entity sets mutex
            Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

            auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

            if (component_entity_sets_it != m_component_entity_sets.End())
            {
                for (TypeID entity_set_type_id : component_entity_sets_it->second)
                {
                    EntitySetBase& entity_set = *m_entity_sets.At(entity_set_type_id);

                    entity_set.OnEntityUpdated(entity_handle);
                }
            }
        }

        NotifySystemsOfEntityRemoved(entity, removed_component_ids);

        EntityTag tag;
        if (IsEntityTagComponent(component_type_id, tag))
        {
            // If the component is an EntityTagComponent, remove the tag from the entity
            entity->OnTagRemoved(tag);
        }
        else
        {
            // Notify the entity that a component was removed
            entity->OnComponentRemoved(component_hyp_data.ToRef());
        }

        component_hyp_data.Reset();

        return true;
    }

    /*! \brief Gets an entity set with the specified components, creating it if it doesn't exist.
     *  This method is thread-safe, and can be used within Systems running in task threads.
     *
     *  \tparam Components The components that the entities in this set have.
     *
     *  \param[in] entities The entity container to use.
     *  \param[in] components The component containers to use.
     *
     *  \return Reference to the entity set.
     */
    template <class... Components>
    EntitySet<Components...>& GetEntitySet()
    {
        Mutex::Guard guard(m_entity_sets_mutex);

        static const TypeID entity_set_type_id = TypeID::ForType<EntitySet<Components...>>();

        auto entity_sets_it = m_entity_sets.Find(entity_set_type_id);

        if (entity_sets_it == m_entity_sets.End())
        {
            auto entity_sets_insert_result = m_entity_sets.Set(
                entity_set_type_id,
                MakeUnique<EntitySet<Components...>>(m_entities, GetContainer<Components>()...));

            AssertThrow(entity_sets_insert_result.second); // Make sure the element was inserted (it shouldn't already exist)

            entity_sets_it = entity_sets_insert_result.first;

            if constexpr (sizeof...(Components) > 0)
            {
                // Make sure the element exists in m_component_entity_sets
                for (TypeID component_type_id : { TypeID::ForType<Components>()... })
                {
                    auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

                    if (component_entity_sets_it == m_component_entity_sets.End())
                    {
                        auto component_entity_sets_insert_result = m_component_entity_sets.Set(component_type_id, {});

                        component_entity_sets_it = component_entity_sets_insert_result.first;
                    }

                    component_entity_sets_it->second.Insert(entity_set_type_id);
                }
            }
        }

        return static_cast<EntitySet<Components...>&>(*entity_sets_it->second);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE Handle<SystemBase> AddSystem(const Handle<SystemBase>& system)
    {
        if (!system.IsValid())
        {
            return Handle<SystemBase>::empty;
        }

        return AddSystemToExecutionGroup(system);
    }

    template <class SystemType>
    Handle<SystemType> GetSystem() const
    {
        for (const SystemExecutionGroup& system_execution_group : m_system_execution_groups)
        {
            if (Handle<SystemType> system = system_execution_group.GetSystem<SystemType>())
            {
                return system;
            }
        }

        return Handle<SystemType>::empty;
    }

    HYP_METHOD()
    Handle<SystemBase> GetSystemByTypeID(TypeID system_type_id) const
    {
        for (const SystemExecutionGroup& system_execution_group : m_system_execution_groups)
        {
            for (const auto& it : system_execution_group.GetSystems())
            {
                if (it.first == system_type_id)
                {
                    return it.second;
                }
            }
        }

        return Handle<SystemBase>::empty;
    }

    template <class Callback>
    HYP_FORCE_INLINE void ForEachEntity(Callback&& callback) const
    {
        Threads::AssertOnThread(m_owner_thread_id);

        ForEach(m_entities, [callback = std::forward<Callback>(callback)](const auto& it)
            {
                const WeakHandle<Entity>& entity_weak = it.first;
                const EntityData& entity_data = it.second;

                Handle<Entity> entity = entity_weak.Lock();

                if (entity.IsValid())
                {
                    return callback(entity, entity_data);
                }

                return IterationResult::CONTINUE;
            });
    }

    void Shutdown();

    void BeginAsyncUpdate(float delta);
    void EndAsyncUpdate();

    template <class Component>
    HYP_FORCE_INLINE ComponentContainer<Component>& GetContainer()
    {
        EnsureValidComponentType<Component>();

        HYP_MT_CHECK_READ(m_containers_data_race_detector);

        auto it = m_containers.Find<Component>();

        if (it == m_containers.End())
        {
            it = m_containers.Set<Component>(MakeUnique<ComponentContainer<Component>>()).first;
        }

        return static_cast<ComponentContainer<Component>&>(*it->second);
    }

    HYP_FORCE_INLINE ComponentContainerBase* TryGetContainer(TypeID component_type_id)
    {
        EnsureValidComponentType(component_type_id);

        HYP_MT_CHECK_READ(m_containers_data_race_detector);

        auto it = m_containers.Find(component_type_id);

        if (it == m_containers.End())
        {
            return nullptr;
        }

        return it->second.Get();
    }

private:
    void Init() override;

    HYP_METHOD()
    Handle<Entity> AddBasicEntity();

    void AddExistingEntity_Internal(const Handle<Entity>& entity);

    template <class Component>
    static void EnsureValidComponentType()
    {
#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(IsValidComponentType<Component>(), "Invalid component type: %s", TypeName<Component>().Data());
#endif
    }

    static void EnsureValidComponentType(TypeID component_type_id)
    {
#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(IsValidComponentType(component_type_id), "Invalid component type: TypeID(%u)", component_type_id.Value());
#endif
    }

    template <uint32... Indices>
    HYP_FORCE_INLINE void GetTagsHelper(const Entity* entity, std::integer_sequence<uint32, Indices...>, Array<EntityTag>& out_tags) const
    {
        ((HasTag<EntityTag(Indices + 1)>(entity) ? (void)(out_tags.PushBack(EntityTag(Indices + 1))) : void()), ...);
    }

    template <uint32... Indices>
    HYP_FORCE_INLINE void GetTagsHelper(const Entity* entity, std::integer_sequence<uint32, Indices...>, uint32& out_mask) const
    {
        ((HasTag<EntityTag(Indices + 1)>(entity) ? (void)(out_mask |= (1u << uint32(Indices))) : void()), ...);
    }

    Handle<SystemBase> AddSystemToExecutionGroup(const Handle<SystemBase>& system)
    {
        AssertThrow(system.IsValid());
        AssertThrow(system->m_entity_manager == nullptr || system->m_entity_manager == this);

        bool was_added = false;

        if ((m_flags & EntityManagerFlags::PARALLEL_SYSTEM_EXECUTION) && system->AllowParallelExecution())
        {
            for (SystemExecutionGroup& system_execution_group : m_system_execution_groups)
            {
                if (system_execution_group.IsValidForSystem(system.Get()))
                {
                    if (system_execution_group.AddSystem(system))
                    {
                        was_added = true;

                        break;
                    }
                }
            }
        }

        if (!was_added)
        {
            SystemExecutionGroup& system_execution_group = m_system_execution_groups.EmplaceBack(system->RequiresGameThread(), system->AllowUpdate());

            if (system_execution_group.AddSystem(system))
            {
                was_added = true;
            }
        }

        system->m_entity_manager = this;

        // If the EntityManager is initialized, call Initialize() on the System.
        if (IsInitCalled() && was_added)
        {
            system->InitComponentInfos_Internal();

            if (m_world != nullptr)
            {
                InitializeSystem(system);
            }
        }

        return system;
    }

    void InitializeSystem(const Handle<SystemBase>& system);
    void ShutdownSystem(const Handle<SystemBase>& system);

    void NotifySystemsOfEntityAdded(const Handle<Entity>& entity, const TypeMap<ComponentID>& component_ids);
    void NotifySystemsOfEntityRemoved(Entity* entity, const TypeMap<ComponentID>& component_ids);

    /*! \brief Removes an entity from the EntityManager.
     *
     *  \param[in] entity The Entity to remove.
     *
     *  \return True if the entity was removed, false otherwise.
     */
    bool RemoveEntity(Entity* entity);

    bool IsEntityInitializedForSystem(SystemBase* system, const Entity* entity) const;

    void GetSystemClasses(Array<const HypClass*>& out_classes) const;

    ThreadID m_owner_thread_id;
    World* m_world;
    Scene* m_scene;
    EnumFlags<EntityManagerFlags> m_flags;

    TypeMap<UniquePtr<ComponentContainerBase>> m_containers;
    DataRaceDetector m_containers_data_race_detector;
    EntityContainer m_entities;
    DataRaceDetector m_entities_data_race_detector;
    HashMap<TypeID, UniquePtr<EntitySetBase>> m_entity_sets;
    mutable Mutex m_entity_sets_mutex;
    TypeMap<HashSet<TypeID>> m_component_entity_sets;

    Array<SystemExecutionGroup> m_system_execution_groups;

    SystemExecutionGroup* m_root_synchronous_execution_group;

    HashMap<SystemBase*, HashSet<Entity*>> m_system_entity_map;
    mutable Mutex m_system_entity_map_mutex;
};

} // namespace hyperion

#endif