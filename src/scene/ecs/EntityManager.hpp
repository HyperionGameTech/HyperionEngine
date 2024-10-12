/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENTITY_MANAGER_HPP
#define HYPERION_ECS_ENTITY_MANAGER_HPP

#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/FlatSet.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/TypeMap.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/AnyRef.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/functional/Proc.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/utilities/Tuple.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/object/HypObject.hpp>

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
    NONE                            = 0x0,
    PARALLEL_SYSTEM_EXECUTION       = 0x1,

    DEFAULT                         = PARALLEL_SYSTEM_EXECUTION
};

HYP_MAKE_ENUM_FLAGS(EntityManagerFlags)

enum class EntityManagerCommandQueueFlags : uint32
{
    NONE            = 0x0,
    EXEC_COMMANDS   = 0x1,

    DEFAULT         = EXEC_COMMANDS
};

HYP_MAKE_ENUM_FLAGS(EntityManagerCommandQueueFlags)

class Scene;

/*! \brief A group of Systems that are able to be processed concurrently, as they do not share any dependencies.
 */
class HYP_API SystemExecutionGroup
{
public:
    SystemExecutionGroup();
    SystemExecutionGroup(const SystemExecutionGroup &)                  = delete;
    SystemExecutionGroup &operator=(const SystemExecutionGroup &)       = delete;
    SystemExecutionGroup(SystemExecutionGroup &&) noexcept              = default;
    SystemExecutionGroup &operator=(SystemExecutionGroup &&) noexcept   = default;
    ~SystemExecutionGroup();

    HYP_FORCE_INLINE TypeMap<UniquePtr<SystemBase>> &GetSystems()
        { return m_systems; }

    HYP_FORCE_INLINE const TypeMap<UniquePtr<SystemBase>> &GetSystems() const
        { return m_systems; }

    HYP_FORCE_INLINE TaskBatch *GetTaskBatch() const
        { return m_task_batch.Get(); }

    /*! \brief Checks if the SystemExecutionGroup is valid for the given System.
     *
     *  \param[in] system_ptr The System to check.
     *
     *  \return True if the SystemExecutionGroup is valid for the given System, false otherwise.
     */
    bool IsValidForExecutionGroup(const SystemBase *system_ptr) const
    {
        AssertThrow(system_ptr != nullptr);

        const Array<TypeID> &component_type_ids = system_ptr->GetComponentTypeIDs();

        for (const auto &it : m_systems) {
            const SystemBase *other_system = it.second.Get();

            for (TypeID component_type_id : component_type_ids) {
                const ComponentInfo &component_info = system_ptr->GetComponentInfo(component_type_id);

                if (component_info.rw_flags & COMPONENT_RW_FLAGS_WRITE) {
                    if (other_system->HasComponentTypeID(component_type_id, true)) {
                        return false;
                    }
                } else {
                    // This System is read-only for this component, so it can be processed with other Systems
                    if (other_system->HasComponentTypeID(component_type_id, false)) {
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
        const TypeID id = TypeID::ForType<SystemType>();

        return m_systems.Find(id) != m_systems.End();
    }

    /*! \brief Adds a System to the SystemExecutionGroup.
     *
     *  \tparam SystemType The type of the System to add.
     *
     *  \param[in] system_ptr The System to add.
     */
    template <class SystemType>
    SystemBase *AddSystem(UniquePtr<SystemType> &&system_ptr)
    {
        AssertThrow(system_ptr != nullptr);
        AssertThrowMsg(IsValidForExecutionGroup(system_ptr.Get()), "System is not valid for this SystemExecutionGroup");

        auto it = m_systems.Find<SystemType>();
        AssertThrowMsg(it == m_systems.End(), "System already exists");

        auto insert_result = m_systems.Set<SystemType>(std::move(system_ptr));

        return insert_result.first->second.Get();
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
        const TypeID id = TypeID::ForType<SystemType>();

        return m_systems.Erase(id);
    }

    /*! \brief Start processing all Systems in the SystemExecutionGroup.
     *
     *  \param[in] delta The delta time value
     */
    void StartProcessing(GameCounter::TickUnit delta);

    /*! \brief Waits on all processing tasks to complete */
    void FinishProcessing();

private:
    TypeMap<UniquePtr<SystemBase>>  m_systems;
    UniquePtr<TaskBatch>            m_task_batch;
};

using EntityListenerID = uint;

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

class EntityManager;

using EntityManagerCommandProc = Proc<void, EntityManager &/* mgr*/, GameCounter::TickUnit /* delta */>;

class HYP_API EntityManagerCommandQueue
{
public:
    EntityManagerCommandQueue(EnumFlags<EntityManagerCommandQueueFlags> flags = EntityManagerCommandQueueFlags::DEFAULT);
    EntityManagerCommandQueue(const EntityManagerCommandQueue &)                = delete;
    EntityManagerCommandQueue &operator=(const EntityManagerCommandQueue &)     = delete;
    EntityManagerCommandQueue(EntityManagerCommandQueue &&) noexcept            = delete;
    EntityManagerCommandQueue &operator=(EntityManagerCommandQueue &&) noexcept = delete;
    ~EntityManagerCommandQueue()                                                = default;

    HYP_FORCE_INLINE EnumFlags<EntityManagerCommandQueueFlags> GetFlags() const
        { return m_flags; }

    HYP_FORCE_INLINE void SetFlags(EnumFlags<EntityManagerCommandQueueFlags> flags)
        { m_flags = flags; }

    HYP_FORCE_INLINE bool HasUpdates() const
        { return m_count.Get(MemoryOrder::ACQUIRE) != 0; }

    /*! \brief Waits for the command queue to be empty.
     *  Useful for ensuring that all commands have been processed before continuing. */
    void AwaitEmpty();

    void Push(EntityManagerCommandProc &&command);
    void Execute(EntityManager &mgr, GameCounter::TickUnit delta);

private:
    struct EntityManagerCommandBuffer
    {
        Queue<EntityManagerCommandProc> commands;
        std::mutex                      mutex;
    };

    EnumFlags<EntityManagerCommandQueueFlags>   m_flags;
    FixedArray<EntityManagerCommandBuffer, 2>   m_command_buffers;
    AtomicVar<uint>                             m_buffer_index { 0 };
    AtomicVar<uint>                             m_count { 0 };
    std::condition_variable                     m_condition_variable;
};

class EntityToEntityManagerMap
{
public:
    HYP_FORCE_INLINE void Add(ID<Entity> entity, EntityManager *entity_manager)
    {
        Mutex::Guard guard(m_mutex);

#ifdef HYP_DEBUG_MODE
        const auto it = m_map.Find(entity);
        AssertThrowMsg(it == m_map.End(), "Entity already added to Entity -> EntityManager mapping!");
#endif

        m_map.Set(entity, entity_manager);
    }

    HYP_FORCE_INLINE void Remove(ID<Entity> entity)
    {
        Mutex::Guard guard(m_mutex);

        const auto it = m_map.Find(entity);
        AssertThrowMsg(it != m_map.End(), "Entity -> EntityManager mapping does not exist!");

        m_map.Erase(it);
    }

    HYP_FORCE_INLINE EntityManager *GetEntityManager(ID<Entity> entity) const
    {
        Mutex::Guard guard(m_mutex);

        const auto it = m_map.Find(entity);

        if (it == m_map.End()) {
            return nullptr;
        }

        return it->second;
    }

    HYP_FORCE_INLINE void RemoveEntityManager(EntityManager *entity_manager)
    {
        Mutex::Guard guard(m_mutex);

        Array<ID<Entity>> keys_to_remove;

        for (auto &it : m_map) {
            if (it.second == entity_manager) {
                keys_to_remove.PushBack(it.first);
            }
        }

        for (ID<Entity> entity : keys_to_remove) {
            m_map.Erase(entity);
        }
    }

    HYP_FORCE_INLINE void Remap(ID<Entity> entity, EntityManager *new_entity_manager)
    {
        Mutex::Guard guard(m_mutex);

        const auto it = m_map.Find(entity);
        AssertThrowMsg(it != m_map.End(), "Entity -> EntityManager mapping does not exist!");

        it->second = new_entity_manager;
    }

private:
    HashMap<ID<Entity>, EntityManager *>    m_map;
    mutable Mutex                           m_mutex;
};

HYP_CLASS()
class HYP_API EntityManager : public EnableRefCountedPtrFromThis<EntityManager>
{
    static EntityToEntityManagerMap s_entity_to_entity_manager_map;

    HYP_OBJECT_BODY(EntityManager);

public:
    static constexpr ComponentID invalid_component_id = 0;

    EntityManager(ThreadMask owner_thread_mask, Scene *scene, EnumFlags<EntityManagerFlags> flags = EntityManagerFlags::DEFAULT);
    EntityManager(const EntityManager &)                = delete;
    EntityManager &operator=(const EntityManager &)     = delete;
    EntityManager(EntityManager &&) noexcept            = delete;
    EntityManager &operator=(EntityManager &&) noexcept = delete;
    ~EntityManager();

    static EntityToEntityManagerMap &GetEntityToEntityManagerMap();
    
    template <class Component>
    static bool IsValidComponentType()
    {
        return IsValidComponentType(TypeID::ForType<Component>());
    }

    static bool IsValidComponentType(TypeID component_type_id);


    /*! \brief Gets the thread mask of the thread that owns this EntityManager.
     *
     *  \return The thread mask.
     */
    HYP_FORCE_INLINE ThreadMask GetOwnerThreadMask() const
        { return m_owner_thread_mask; }

    /*! \brief Sets the thread mask of the thread that owns this EntityManager.
     *  \internal This is used by the Scene to set the thread mask of the Scene's thread. It should not be called from user code. */
    HYP_FORCE_INLINE void SetOwnerThreadMask(ThreadMask owner_thread_mask)
        { m_owner_thread_mask = owner_thread_mask; }

    /*! \brief Gets the Scene that this EntityManager is associated with.
     *
     *  \return Pointer to the Scene.
     */
    HYP_METHOD()
    HYP_FORCE_INLINE Scene *GetScene() const
        { return m_scene; }

    /*! \brief Gets the command queue for this EntityManager.
     *
     *  \return Reference to the command queue.
     */
    HYP_FORCE_INLINE EntityManagerCommandQueue &GetCommandQueue()
        { return m_command_queue; }

    /*! \brief Gets the command queue for this EntityManager.
     *
     *  \return Reference to the command queue.
     */
    HYP_FORCE_INLINE const EntityManagerCommandQueue &GetCommandQueue() const
        { return m_command_queue; }

    /*! \brief Adds a new entity to the EntityManager.
     *  \note Must be called from the owner thread.
     *
     *  \return The Entity that was added.
     */
    HYP_NODISCARD ID<Entity> AddEntity();

    /*! \brief Removes an entity from the EntityManager.
     *
     *  \param[in] id The Entity to remove.
     *
     *  \return True if the entity was removed, false otherwise.
     */
    bool RemoveEntity(ID<Entity> id);

    /*! \brief Moves an entity from one EntityManager to another.
     *  This is useful for moving entities between scenes.
     *  All components will be moved to the other EntityManager.
     *
     *  \param[in] entity The Entity to move.
     *  \param[in] other The EntityManager to move the entity to.
     */
    void MoveEntity(ID<Entity> entity, EntityManager &other);
    
    HYP_FORCE_INLINE bool HasEntity(ID<Entity> entity) const
    {
        Threads::AssertOnThread(m_owner_thread_mask);
        HYP_MT_CHECK_READ(m_entities_data_race_detector);

        if (!entity.IsValid()) {
            return false;
        }

        return m_entities.Find(entity) != m_entities.End();
    }

    template <EntityTag Tag>
    HYP_FORCE_INLINE bool HasTag(ID<Entity> entity) const
        { return HasComponent<EntityTagComponent<Tag>>(entity); }

    template <EntityTag Tag>
    HYP_FORCE_INLINE void AddTag(ID<Entity> entity)
    {
        if (HasTag<Tag>(entity)) {
            return;
        }

        AddComponent<EntityTagComponent<Tag>>(entity, EntityTagComponent<Tag>());
    }

    template <EntityTag Tag>
    HYP_FORCE_INLINE void RemoveTag(ID<Entity> entity)
    {
        if (!HasTag<Tag>(entity)) {
            return;
        }

        RemoveComponent<EntityTagComponent<Tag>>(entity);
    }

    HYP_FORCE_INLINE Array<EntityTag> GetTags(ID<Entity> entity) const
    {
        Array<EntityTag> tags;
        GetTagsHelper(entity, std::make_integer_sequence<uint32, uint32(EntityTag::MAX) - 2>(), tags);

        return tags;
    }

    HYP_FORCE_INLINE uint32 GetTagsMask(ID<Entity> entity) const
    {
        uint32 mask = 0;
        GetTagsHelper(entity, std::make_integer_sequence<uint32, uint32(EntityTag::MAX) - 2>(), mask);

        return mask;
    }

    template <class Component>
    HYP_FORCE_INLINE bool HasComponent(ID<Entity> entity) const
    {
        EnsureValidComponentType<Component>();

        Threads::AssertOnThread(m_owner_thread_mask);
        HYP_MT_CHECK_READ(m_entities_data_race_detector);

        AssertThrowMsg(entity.IsValid(), "Invalid entity ID");

        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");

        return it->second.HasComponent<Component>();
    }

    HYP_FORCE_INLINE bool HasComponent(TypeID component_type_id, ID<Entity> entity)
    {
        EnsureValidComponentType(component_type_id);

        Threads::AssertOnThread(m_owner_thread_mask);
        HYP_MT_CHECK_READ(m_entities_data_race_detector);

        AssertThrowMsg(entity.IsValid(), "Invalid entity ID");

        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");

        return it->second.HasComponent(component_type_id);
    }

    template <class Component>
    HYP_FORCE_INLINE Component &GetComponent(ID<Entity> entity)
    {
        // // Temporarily remove this check because OnEntityAdded() and OnEntityRemoved() are called from the game thread
        // Threads::AssertOnThread(m_owner_thread_mask);

        EnsureValidComponentType<Component>();

        AssertThrowMsg(entity.IsValid(), "Invalid entity ID");
        
        HYP_MT_CHECK_READ(m_entities_data_race_detector);
        HYP_MT_CHECK_READ(m_containers_data_race_detector);

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
    HYP_FORCE_INLINE const Component &GetComponent(ID<Entity> entity) const
        { return const_cast<EntityManager *>(this)->GetComponent<Component>(entity); }

    template <class Component>
    Component *TryGetComponent(ID<Entity> entity)
    {
        EnsureValidComponentType<Component>();

        if (!entity.IsValid()) {
            return nullptr;
        }

        HYP_MT_CHECK_READ(m_entities_data_race_detector);
        HYP_MT_CHECK_READ(m_containers_data_race_detector);

        auto it = m_entities.Find(entity);

        if (it == m_entities.End()) {
            return nullptr;
        }

        if (!it->second.HasComponent<Component>()) {
            return nullptr;
        }

        const TypeID component_type_id = TypeID::ForType<Component>();
        const ComponentID component_id = it->second.GetComponentID<Component>();

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
    HYP_FORCE_INLINE const Component *TryGetComponent(ID<Entity> entity) const
        { return const_cast<EntityManager *>(this)->TryGetComponent<Component>(entity); }

    /*! \brief Gets a component using the dynamic type ID.
     *
     *  \param[in] component_type_id The type ID of the component to get.
     *  \param[in] entity The entity to get the component from.
     *
     *  \return Pointer to the component as a void pointer, or nullptr if the entity does not have the component.
     */
    AnyRef TryGetComponent(TypeID component_type_id, ID<Entity> entity)
    {
        // // Temporarily remove this check because OnEntityAdded() and OnEntityRemoved() are called from the game thread
        // Threads::AssertOnThread(m_owner_thread_mask);
        
        EnsureValidComponentType(component_type_id);

        if (!entity.IsValid()) {
            return AnyRef::Empty();
        }

        HYP_MT_CHECK_READ(m_entities_data_race_detector);
        HYP_MT_CHECK_READ(m_containers_data_race_detector);

        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");
        
        if (!it->second.HasComponent(component_type_id)) {
            return AnyRef::Empty();
        }

        const ComponentID component_id = it->second.GetComponentID(component_type_id);

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
    HYP_FORCE_INLINE ConstAnyRef TryGetComponent(TypeID component_type_id, ID<Entity> entity) const
        { return const_cast<EntityManager *>(this)->TryGetComponent(component_type_id, entity); }

    template <class ... Components>
    HYP_FORCE_INLINE Tuple< Components &... > GetComponents(ID<Entity> entity)
        { return Tie(GetComponent<Components>(entity)...); }

    template <class ... Components>
    HYP_FORCE_INLINE Tuple< const Components &... > GetComponents(ID<Entity> entity) const
        { return Tie(GetComponent<Components>(entity)...); }

    /*! \brief Get a map of all component types to respective component IDs for a given Entity.
     *  \param entity The entity to lookup components for
     *  \returns An Optional object holding a reference to the typemap if it exists, otherwise an empty optional. */
    HYP_FORCE_INLINE Optional<const TypeMap<ComponentID> &> GetAllComponents(ID<Entity> entity) const
    {
        Threads::AssertOnThread(m_owner_thread_mask);
        HYP_MT_CHECK_READ(m_entities_data_race_detector);

        if (!entity.IsValid()) {
            return { };
        }

        auto it = m_entities.Find(entity);
        if (it == m_entities.End()) {
            return { };
        }

        return it->second.components;
    }

    template <class Component, class U = Component>
    ComponentID AddComponent(ID<Entity> entity, U &&component)
    {
        EnsureValidComponentType<Component>();

        Threads::AssertOnThread(m_owner_thread_mask);
        HYP_MT_CHECK_READ(m_entities_data_race_detector);

        AssertThrowMsg(entity.IsValid(), "Invalid entity ID");

        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");

        auto component_it = it->second.components.Find<Component>();
        // @TODO: Replace the component if it already exists
        AssertThrowMsg(component_it == it->second.components.End(), "Entity already has component of type %s", TypeNameWithoutNamespace<Component>().Data());

        const TypeID component_type_id = TypeID::ForType<Component>();
        const ComponentID component_id = GetContainer<Component>().AddComponent(std::move(component));

        auto components_insert_result = it->second.components.Set<Component>(component_id);
        component_it = components_insert_result.first;

        { // Lock the entity sets mutex
            Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

            auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

            if (component_entity_sets_it != m_component_entity_sets.End()) {
                for (EntitySetTypeID entity_set_id : component_entity_sets_it->second) {
                    auto entity_set_it = m_entity_sets.Find(entity_set_id);
                    AssertThrowMsg(entity_set_it != m_entity_sets.End(), "Entity set does not exist");

                    entity_set_it->second->OnEntityUpdated(entity);
                }
            }
        }

        // Notify systems that entity is being added to them
        NotifySystemsOfEntityAdded(entity, it->second.components);

        return component_id;
    }

    ComponentID AddComponent(ID<Entity> entity, AnyRef component)
    {
        const TypeID component_type_id = component.GetTypeID();

        EnsureValidComponentType(component_type_id);

        Threads::AssertOnThread(m_owner_thread_mask);
        HYP_MT_CHECK_READ(m_entities_data_race_detector);

        AssertThrowMsg(entity.IsValid(), "Invalid entity ID");

        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");

        auto component_it = it->second.components.Find(component_type_id);
        // @TODO: Replace the component if it already exists
        AssertThrowMsg(component_it == it->second.components.End(), "Entity already has component of type %u", component_type_id.Value());

        ComponentContainerBase *container = TryGetContainer(component_type_id);
        AssertThrowMsg(container != nullptr, "Component container does not exist for component type %u", component_type_id.Value());

        const ComponentID component_id = container->AddComponent(component);

        auto components_insert_result = it->second.components.Set(component.GetTypeID(), component_id);
        component_it = components_insert_result.first;

        { // Lock the entity sets mutex
            Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

            auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

            if (component_entity_sets_it != m_component_entity_sets.End()) {
                for (EntitySetTypeID entity_set_id : component_entity_sets_it->second) {
                    auto entity_set_it = m_entity_sets.Find(entity_set_id);
                    AssertThrowMsg(entity_set_it != m_entity_sets.End(), "Entity set does not exist");

                    entity_set_it->second->OnEntityUpdated(entity);
                }
            }
        }

        // Notify systems that entity is being added to them
        NotifySystemsOfEntityAdded(entity, it->second.components);

        return component_id;
    }

    template <class Component>
    void RemoveComponent(ID<Entity> entity)
    {
        EnsureValidComponentType<Component>();

        Threads::AssertOnThread(m_owner_thread_mask);
        HYP_MT_CHECK_READ(m_entities_data_race_detector);

        AssertThrowMsg(entity.IsValid(), "Invalid entity ID");

        auto it = m_entities.Find(entity);
        AssertThrowMsg(it != m_entities.End(), "Entity does not exist");

        auto component_it = it->second.components.Find<Component>();
        AssertThrowMsg(component_it != it->second.components.End(), "Entity does not have component");

        const TypeID component_type_id = component_it->first;
        const ComponentID component_id = component_it->second;

        // Notify systems that entity is being removed from them
        TypeMap<ComponentID> removed_component_ids;
        removed_component_ids.Set(component_type_id, component_id);
        NotifySystemsOfEntityRemoved(entity, removed_component_ids);

        AssertThrow(GetContainer<Component>().RemoveComponent(component_id));

        it->second.components.Erase(component_it);

        // Lock the entity sets mutex
        Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

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

    template <class SystemType, class... Args>
    HYP_FORCE_INLINE SystemType *AddSystem(Args &&... args)
    {
        return AddSystemToExecutionGroup(MakeUnique<SystemType>(*this, std::forward<Args>(args)...));
    }

    void Initialize();

    void BeginUpdate(GameCounter::TickUnit delta);
    void EndUpdate();

    void PushCommand(EntityManagerCommandProc &&command)
        { m_command_queue.Push(std::move(command)); }

    template <class Component>
    HYP_FORCE_INLINE ComponentContainer<Component> &GetContainer()
    {
        EnsureValidComponentType<Component>();

        HYP_MT_CHECK_READ(m_containers_data_race_detector);

        auto it = m_containers.Find<Component>();

        if (it == m_containers.End()) {
            it = m_containers.Set<Component>(MakeUnique<ComponentContainer<Component>>()).first;
        }

        return static_cast<ComponentContainer<Component> &>(*it->second);
    }

    HYP_FORCE_INLINE ComponentContainerBase *TryGetContainer(TypeID component_type_id)
    {
        EnsureValidComponentType(component_type_id);

        HYP_MT_CHECK_READ(m_containers_data_race_detector);

        auto it = m_containers.Find(component_type_id);
        
        if (it == m_containers.End()) {
            return nullptr;
        }

        return it->second.Get();
    }

private:
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
        AssertThrowMsg(IsValidComponentType(component_type_id), "Invalid component type");
#endif
    }

    template <uint32... indices>
    HYP_FORCE_INLINE void GetTagsHelper(ID<Entity> id, std::integer_sequence<uint32, indices...>, Array<EntityTag> &out_tags) const
    {
        ((HasTag<EntityTag(indices + 1)>(id) ? (void)(out_tags.PushBack(EntityTag(indices + 1))) : void()), ...);
    }

    template <uint32... indices>
    HYP_FORCE_INLINE void GetTagsHelper(ID<Entity> id, std::integer_sequence<uint32, indices...>, uint32 &out_mask) const
    {
        ((HasTag<EntityTag(indices + 1)>(id) ? (void)(out_mask |= (1u << uint32(indices))) : void()), ...);
    }

    template <class SystemType>
    SystemType *AddSystemToExecutionGroup(UniquePtr<SystemType> &&system_ptr)
    {
        AssertThrow(system_ptr != nullptr);

        SystemType *ptr = nullptr;

        if ((m_flags & EntityManagerFlags::PARALLEL_SYSTEM_EXECUTION) && system_ptr->AllowParallelExecution()) {
            for (auto &system_execution_group : m_system_execution_groups) {
                if (system_execution_group.IsValidForExecutionGroup(system_ptr.Get())) {
                    ptr = static_cast<SystemType *>(system_execution_group.AddSystem<SystemType>(std::move(system_ptr)));

                    break;
                }
            }
        }

        if (!ptr) {
            ptr = static_cast<SystemType *>(m_system_execution_groups.EmplaceBack().AddSystem<SystemType>(std::move(system_ptr)));
        }

        // @TODO Notify the system of all entities that have the components it needs
        // Maybe not necessary, since systems are added at the start

        return ptr;
    }

    void NotifySystemsOfEntityAdded(ID<Entity> id, const TypeMap<ComponentID> &component_ids);
    void NotifySystemsOfEntityRemoved(ID<Entity> id, const TypeMap<ComponentID> &component_ids);

    ThreadMask                                                              m_owner_thread_mask;
    Scene                                                                   *m_scene;
    EnumFlags<EntityManagerFlags>                                           m_flags;

    TypeMap<UniquePtr<ComponentContainerBase>>                              m_containers;
    DataRaceDetector                                                        m_containers_data_race_detector;
    EntityContainer                                                         m_entities;
    DataRaceDetector                                                        m_entities_data_race_detector;
    FlatMap<EntitySetTypeID, UniquePtr<EntitySetBase>>                      m_entity_sets;
    mutable Mutex                                                           m_entity_sets_mutex;
    TypeMap<FlatSet<EntitySetTypeID>>                                       m_component_entity_sets;
    FlatMap<EntitySetTypeID, FlatMap<EntityListenerID, EntityListener>>     m_entity_listeners;
    EntityManagerCommandQueue                                               m_command_queue;
    Array<SystemExecutionGroup>                                             m_system_execution_groups;

    bool                                                                    m_is_initialized;
};

} // namespace hyperion

#endif