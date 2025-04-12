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
#include <core/memory/RefCountedPtr.hpp>

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
class TaskBatch;
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

class World;
class Scene;

static constexpr uint32 g_move_entity_write_flag = 0x1u;
static constexpr uint32 g_move_entity_read_mask = ~0u << 1;

/*! \brief A group of Systems that are able to be processed concurrently, as they do not share any dependencies.
 */
class HYP_API SystemExecutionGroup
{
public:
    SystemExecutionGroup(bool requires_game_thread = false, bool allow_update = true);
    SystemExecutionGroup(const SystemExecutionGroup &)                  = delete;
    SystemExecutionGroup &operator=(const SystemExecutionGroup &)       = delete;
    SystemExecutionGroup(SystemExecutionGroup &&) noexcept              = default;
    SystemExecutionGroup &operator=(SystemExecutionGroup &&) noexcept   = default;
    ~SystemExecutionGroup();

    HYP_FORCE_INLINE bool RequiresGameThread() const
        { return m_requires_game_thread; }

    HYP_FORCE_INLINE bool AllowUpdate() const
        { return m_allow_update; }

    HYP_FORCE_INLINE TypeMap<UniquePtr<SystemBase>> &GetSystems()
        { return m_systems; }

    HYP_FORCE_INLINE const TypeMap<UniquePtr<SystemBase>> &GetSystems() const
        { return m_systems; }

    HYP_FORCE_INLINE TaskBatch *GetTaskBatch() const
        { return m_task_batch.Get(); }

#ifdef HYP_DEBUG_MODE
    HYP_FORCE_INLINE const PerformanceClock &GetPerformanceClock() const
        { return m_performance_clock; }

    HYP_FORCE_INLINE const FlatMap<SystemBase *, PerformanceClock> &GetPerformanceClocks() const
        { return m_performance_clocks; }
#endif

    /*! \brief Checks if the SystemExecutionGroup is valid for the given System.
     *
     *  \param[in] system_ptr The System to check.
     *
     *  \return True if the SystemExecutionGroup is valid for the given System, false otherwise.
     */
    bool IsValidForSystem(const SystemBase *system_ptr) const
    {
        AssertThrow(system_ptr != nullptr);

        // If the system does not allow update calls, and we don't as well, return true as there will be no overlap.
        if (!AllowUpdate()) {
            return !system_ptr->AllowUpdate();
        }

        // If the system requires to execute on game thread and the SystemExecutionGroup does not, it is not valid
        // and if the system does not require to execute on game thread and the SystemExecutionGroup does, it is not valid (it could be better parallelized)
        if (system_ptr->RequiresGameThread() != RequiresGameThread()) {
            return false;
        }

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
        static const TypeID type_id = TypeID::ForType<SystemType>();

        return m_systems.Find(type_id) != m_systems.End();
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
        AssertThrowMsg(IsValidForSystem(system_ptr.Get()), "System is not valid for this SystemExecutionGroup");

        auto it = m_systems.Find<SystemType>();
        AssertThrowMsg(it == m_systems.End(), "System already exists");

        auto insert_result = m_systems.Set<SystemType>(std::move(system_ptr));

        return insert_result.first->second.Get();
    }

    template <class SystemType>
    SystemType *GetSystem() const
    {
        static const TypeID type_id = TypeID::ForType<SystemType>();

        const auto it = m_systems.Find(type_id);

        if (it == m_systems.End()) {
            return nullptr;
        }

        return static_cast<SystemType *>(it->second.Get());
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
    void StartProcessing(GameCounter::TickUnit delta);

    /*! \brief Waits on all processing tasks to complete */
    void FinishProcessing(bool execute_blocking = false);

private:
    bool                                    m_requires_game_thread;
    bool                                    m_allow_update;

    TypeMap<UniquePtr<SystemBase>>          m_systems;
    UniquePtr<TaskBatch>                    m_task_batch;

#ifdef HYP_DEBUG_MODE
    PerformanceClock                        m_performance_clock;
    FlatMap<SystemBase *, PerformanceClock> m_performance_clocks;
#endif
};

class EntityManager;

using EntityManagerCommandProc = Proc<void(EntityManager &mgr, GameCounter::TickUnit delta)>;

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
    AtomicVar<uint32>                           m_buffer_index { 0 };
    AtomicVar<uint32>                           m_count { 0 };
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

    HYP_FORCE_INLINE void Remove(ID<Entity> id)
    {
        Mutex::Guard guard(m_mutex);

        const auto it = m_map.FindAs(id);
        AssertThrowMsg(it != m_map.End(), "Entity -> EntityManager mapping does not exist!");

        m_map.Erase(it);
    }

    HYP_FORCE_INLINE EntityManager *GetEntityManager(ID<Entity> id) const
    {
        Mutex::Guard guard(m_mutex);

        const auto it = m_map.FindAs(id);

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

        for (const ID<Entity> &entity : keys_to_remove) {
            m_map.Erase(entity);
        }
    }

    HYP_FORCE_INLINE void Remap(ID<Entity> id, EntityManager *new_entity_manager)
    {
        Mutex::Guard guard(m_mutex);

        const auto it = m_map.FindAs(id);
        AssertThrowMsg(it != m_map.End(), "Entity -> EntityManager mapping does not exist!");

        it->second = new_entity_manager;
    }

    void ForEachEntityManager(ProcRef<void(EntityManager *entity_manager)> proc) const
    {
        HashSet<RC<EntityManager>> entity_managers;

        {
            Mutex::Guard guard(m_mutex);

            entity_managers.Reserve(m_map.Size());

            for (auto &it : m_map) {
                entity_managers.Insert(ToRefCountedPtr(it.second));
            }
        }

        for (const RC<EntityManager> &entity_manager : entity_managers) {
            proc(entity_manager.Get());
        }
    }

    HYP_API Task<bool> PerformActionWithEntity(ID<Entity> id, Proc<void(EntityManager *entity_manager, ID<Entity> id)> &&callback);
    HYP_API void PerformActionWithEntity_FireAndForget(ID<Entity> id, Proc<void(EntityManager *entity_manager, ID<Entity> id)> &&callback);

private:
    HashMap<ID<Entity>, EntityManager *>    m_map;
    mutable Mutex                           m_mutex;
};

HYP_CLASS()
class HYP_API EntityManager : public EnableRefCountedPtrFromThis<EntityManager>
{
    static EntityToEntityManagerMap s_entity_to_entity_manager_map;

    HYP_OBJECT_BODY(EntityManager);

    friend class EntityToEntityManagerMap;

    // Allow Entity destructor to call RemoveEntity().
    friend class Entity;

    // Prevents an Entity from being moved while it is being processed on another thread.
    // When an Entity is being moved from one EntityManager to another, the g_move_entity_write_flag is set.
    // This prevents any other thread from reading the EntityManager while it is being moved.
    struct MoveEntityGuard
    {
        const EntityManager &m_entity_manager;

        MoveEntityGuard(const EntityManager &entity_manager)
            : m_entity_manager(entity_manager)
        {
            uint64 state;
            while (((state = m_entity_manager.m_move_entity_rw_mask.Increment(2, MemoryOrder::ACQUIRE)) & g_move_entity_write_flag)) {
                m_entity_manager.m_move_entity_rw_mask.Decrement(2, MemoryOrder::RELAXED);
                // wait for write flag to be released
                Threads::Sleep(0);
            }
        }

        ~MoveEntityGuard()
        {
            m_entity_manager.m_move_entity_rw_mask.Decrement(2, MemoryOrder::RELEASE);
        }
    };

public:
    static constexpr ComponentID invalid_component_id = 0;

    EntityManager(const ThreadID &owner_thread_id, Scene *scene, EnumFlags<EntityManagerFlags> flags = EntityManagerFlags::DEFAULT);
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

    template <class Component>
    static bool IsEntityTagComponent()
    {
        return IsEntityTagComponent(TypeID::ForType<Component>());
    }

    static bool IsEntityTagComponent(TypeID component_type_id);

    /*! \brief Gets the thread mask of the thread that owns this EntityManager.
     *
     *  \return The thread mask.
     */
    HYP_FORCE_INLINE const ThreadID &GetOwnerThreadID() const
        { return m_owner_thread_id; }

    /*! \brief Sets the thread mask of the thread that owns this EntityManager.
     *  \internal This is used by the Scene to set the thread mask of the Scene's thread. It should not be called from user code. */
    HYP_FORCE_INLINE void SetOwnerThreadID(const ThreadID &owner_thread_id)
        { m_owner_thread_id = owner_thread_id; }

    /*! \brief Gets the World that this EntityManager is associated with.
     *
     *  \return Pointer to the World.
     */
    HYP_METHOD()
    HYP_FORCE_INLINE World *GetWorld() const
        { return m_world; }

    void SetWorld(World *world);

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
     *  \return The Entity that was added. */
    HYP_METHOD()
    HYP_NODISCARD Handle<Entity> AddEntity();

    /*! \brief Moves an entity from one EntityManager to another.
     *  This is useful for moving entities between scenes.
     *  All components will be moved to the other EntityManager.
     *
     *  \param[in] entity The Entity to move.
     *  \param[in] other The EntityManager to move the entity to.
     */
    void MoveEntity(const Handle<Entity> &entity, const RC<EntityManager> &other);
    
    HYP_FORCE_INLINE bool HasEntity(ID<Entity> entity) const
    {
        Threads::AssertOnThread(m_owner_thread_id);

        MoveEntityGuard move_entity_guard(*this);
        HYP_MT_CHECK_READ(m_entities_data_race_detector);

        if (!entity.IsValid()) {
            return false;
        }

        return m_entities.Find(entity) != m_entities.End();
    }

    void AddTag(ID<Entity> entity_id, EntityTag tag);
    bool RemoveTag(ID<Entity> entity_id, EntityTag tag);
    bool HasTag(ID<Entity> entity_id, EntityTag tag) const;

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

    template <EntityTag... Tag>
    HYP_FORCE_INLINE void AddTags(ID<Entity> entity)
    {
        (AddTag<Tag>(entity), ...);
    }

    template <EntityTag Tag>
    HYP_FORCE_INLINE bool RemoveTag(ID<Entity> entity)
    {
        if (!HasTag<Tag>(entity)) {
            return false;
        }

        return RemoveComponent<EntityTagComponent<Tag>>(entity);
    }

    HYP_FORCE_INLINE Array<EntityTag> GetTags(ID<Entity> entity) const
    {
        Array<EntityTag> tags;
        GetTagsHelper(entity, std::make_integer_sequence<uint32, uint32(EntityTag::DESCRIPTOR_MAX) - 2>(), tags);

        return tags;
    }

    HYP_FORCE_INLINE uint32 GetTagsMask(ID<Entity> entity) const
    {
        uint32 mask = 0;
        GetTagsHelper(entity, std::make_integer_sequence<uint32, uint32(EntityTag::DESCRIPTOR_MAX) - 2>(), mask);

        return mask;
    }

    template <class Component>
    bool HasComponent(ID<Entity> entity) const
    {
        EnsureValidComponentType<Component>();

        if (!entity.IsValid()) {
            return false;
        }

        MoveEntityGuard move_entity_guard(*this);
        HYP_MT_CHECK_READ(m_entities_data_race_detector);

        return m_entities.GetEntityData(entity).HasComponent<Component>();
    }

    bool HasComponent(TypeID component_type_id, ID<Entity> entity_id) const
    {
        EnsureValidComponentType(component_type_id);

        if (!entity_id.IsValid()) {
            return false;
        }

        MoveEntityGuard move_entity_guard(*this);
        HYP_MT_CHECK_READ(m_entities_data_race_detector);
        
        return m_entities.GetEntityData(entity_id).HasComponent(component_type_id);
    }

    template <class Component>
    HYP_FORCE_INLINE Component &GetComponent(ID<Entity> entity)
    {
        EnsureValidComponentType<Component>();

        AssertThrowMsg(entity.IsValid(), "Invalid entity ID");
        
        MoveEntityGuard move_entity_guard(*this);
        HYP_MT_CHECK_READ(m_entities_data_race_detector);
        HYP_MT_CHECK_READ(m_containers_data_race_detector);

        EntityData &entity_data = m_entities.GetEntityData(entity);

        const Optional<ComponentID> component_id_opt = entity_data.TryGetComponentID<Component>();
        AssertThrowMsg(component_id_opt.HasValue(), "Entity does not have component");

        static const TypeID component_type_id = TypeID::ForType<Component>();

        auto component_container_it = m_containers.Find(component_type_id);
        AssertThrowMsg(component_container_it != m_containers.End(), "Component container does not exist");

        HYP_MT_CHECK_READ(component_container_it->second->GetDataRaceDetector());

        return static_cast<ComponentContainer<Component> &>(*component_container_it->second).GetComponent(*component_id_opt);
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

        MoveEntityGuard move_entity_guard(*this);
        HYP_MT_CHECK_READ(m_entities_data_race_detector);
        HYP_MT_CHECK_READ(m_containers_data_race_detector);

        EntityData *entity_data = m_entities.TryGetEntityData(entity);

        if (!entity_data) {
            return nullptr;
        }

        if (!entity_data->HasComponent<Component>()) {
            return nullptr;
        }

        static const TypeID component_type_id = TypeID::ForType<Component>();

        const Optional<ComponentID> component_id_opt = entity_data->TryGetComponentID<Component>();

        if (!component_id_opt) {
            return nullptr;
        }

        auto component_container_it = m_containers.Find(component_type_id);
        if (component_container_it == m_containers.End()) {
            return nullptr;
        }

        HYP_MT_CHECK_READ(component_container_it->second->GetDataRaceDetector());

        return &static_cast<ComponentContainer<Component> &>(*component_container_it->second).GetComponent(*component_id_opt);
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
        // Threads::AssertOnThread(m_owner_thread_id);
        
        EnsureValidComponentType(component_type_id);

        if (!entity.IsValid()) {
            return AnyRef::Empty();
        }

        MoveEntityGuard move_entity_guard(*this);
        HYP_MT_CHECK_READ(m_entities_data_race_detector);
        HYP_MT_CHECK_READ(m_containers_data_race_detector);

        EntityData &entity_data = m_entities.GetEntityData(entity);

        const Optional<ComponentID> component_id_opt = entity_data.TryGetComponentID(component_type_id);
        
        if (!component_id_opt) {
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
    HYP_FORCE_INLINE ConstAnyRef TryGetComponent(TypeID component_type_id, ID<Entity> entity) const
        { return const_cast<EntityManager *>(this)->TryGetComponent(component_type_id, entity); }

    template <class... Components>
    HYP_FORCE_INLINE Tuple<Components &...> GetComponents(ID<Entity> entity)
        { return Tie(GetComponent<Components>(entity)...); }

    template <class... Components>
    HYP_FORCE_INLINE Tuple<const Components &...> GetComponents(ID<Entity> entity) const
        { return Tie(GetComponent<Components>(entity)...); }

    /*! \brief Get a map of all component types to respective component IDs for a given Entity.
     *  \param entity The entity to lookup components for
     *  \returns An Optional object holding a reference to the typemap if it exists, otherwise an empty optional. */
    HYP_FORCE_INLINE Optional<const TypeMap<ComponentID> &> GetAllComponents(ID<Entity> entity) const
    {
        if (!entity.IsValid()) {
            return { };
        }

        MoveEntityGuard move_entity_guard(*this);
        HYP_MT_CHECK_RW(m_entities_data_race_detector);

        auto it = m_entities.Find(entity);
        if (it == m_entities.End()) {
            return { };
        }

        return it->second.components;
    }

    void AddComponent(ID<Entity> entity_id, AnyRef component);
    bool RemoveComponent(TypeID component_type_id, ID<Entity> entity_id);

    template <class Component, class U = Component>
    Component &AddComponent(ID<Entity> entity_id, U &&component)
    {
        EnsureValidComponentType<Component>();
        
        AssertThrowMsg(entity_id.IsValid(), "Invalid entity ID");

        Threads::AssertOnThread(m_owner_thread_id);

        // To keep ID valid for NotifySystemsOfEntityAdded()
        WeakHandle<Entity> entity_weak { entity_id };

        Component *component_ptr = nullptr;
        TypeMap<ComponentID> component_ids;

        {
            MoveEntityGuard move_entity_guard(*this);
            HYP_MT_CHECK_READ(m_entities_data_race_detector);

            EntityData &entity_data = m_entities.GetEntityData(entity_id);

            auto component_it = entity_data.FindComponent<Component>();
            // @TODO: Replace the component if it already exists
            AssertThrowMsg(component_it == entity_data.components.End(), "Entity already has component of type %s", TypeNameWithoutNamespace<Component>().Data());

            static const TypeID component_type_id = TypeID::ForType<Component>();

            const Pair<ComponentID, Component &> component_insert_result = GetContainer<Component>().AddComponent(std::move(component));

            entity_data.components.Set<Component>(component_insert_result.first);

            { // Lock the entity sets mutex
                Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

                auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

                if (component_entity_sets_it != m_component_entity_sets.End()) {
                    for (TypeID entity_set_type_id : component_entity_sets_it->second) {
                        EntitySetBase &entity_set = *m_entity_sets.At(entity_set_type_id);

                        entity_set.OnEntityUpdated(entity_id);
                    }
                }
            }

            component_ptr = &component_insert_result.second;

            component_ids = entity_data.components;
        }

        // Notify systems that entity is being added to them
        NotifySystemsOfEntityAdded(entity_id, component_ids);
        
        return *component_ptr;
    }

    template <class Component>
    bool RemoveComponent(ID<Entity> entity_id)
    {
        EnsureValidComponentType<Component>();

        if (!entity_id.IsValid()) {
            return false;
        }

        Threads::AssertOnThread(m_owner_thread_id);

        TypeMap<ComponentID> removed_component_ids;

        // To keep ID valid while removing the component
        WeakHandle<Entity> entity_weak { entity_id };

        { // critical section for entity data
            MoveEntityGuard move_entity_guard(*this);
            HYP_MT_CHECK_READ(m_entities_data_race_detector);

            EntityData &entity_data = m_entities.GetEntityData(entity_id);

            auto component_it = entity_data.FindComponent<Component>();
            if (component_it == entity_data.components.End()) {
                return false;
            }

            const TypeID component_type_id = component_it->first;
            const ComponentID component_id = component_it->second;

            // Notify systems that entity is being removed from them
            removed_component_ids.Set(component_type_id, component_id);

            entity_data.components.Erase(component_it);

            if (!GetContainer<Component>().RemoveComponent(component_id)) {
                return false;
            }

            // Lock the entity sets mutex
            Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

            auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

            if (component_entity_sets_it != m_component_entity_sets.End()) {
                for (TypeID entity_set_type_id : component_entity_sets_it->second) {
                    EntitySetBase &entity_set = *m_entity_sets.At(entity_set_type_id);

                    entity_set.OnEntityUpdated(entity_id);
                }
            }
        }

        NotifySystemsOfEntityRemoved(entity_id, removed_component_ids);

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
    EntitySet<Components...> &GetEntitySet()
    {
        Mutex::Guard guard(m_entity_sets_mutex);

        static const TypeID entity_set_type_id = TypeID::ForType<EntitySet<Components...>>();
        
        auto entity_sets_it = m_entity_sets.Find(entity_set_type_id);

        if (entity_sets_it == m_entity_sets.End()) {
            auto entity_sets_insert_result = m_entity_sets.Set(
                entity_set_type_id,
                MakeUnique<EntitySet<Components...>>(
                    m_entities,
                    GetContainer<Components>()...
                )
            );

            AssertThrow(entity_sets_insert_result.second); // Make sure the element was inserted (it shouldn't already exist)

            entity_sets_it = entity_sets_insert_result.first;

            if constexpr (sizeof...(Components) > 0) {
                // Make sure the element exists in m_component_entity_sets
                for (TypeID component_type_id : { TypeID::ForType<Components>()... }) {
                    auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

                    if (component_entity_sets_it == m_component_entity_sets.End()) {
                        auto component_entity_sets_insert_result = m_component_entity_sets.Set(component_type_id, { });

                        component_entity_sets_it = component_entity_sets_insert_result.first;
                    }

                    component_entity_sets_it->second.Insert(entity_set_type_id);
                }
            }
        }

        return static_cast<EntitySet<Components...> &>(*entity_sets_it->second);
    }

    template <class SystemType>
    HYP_FORCE_INLINE SystemType *AddSystem(UniquePtr<SystemType> &&system)
    {
        return AddSystemToExecutionGroup(std::move(system));
    }

    template <class SystemType, class... Args>
    HYP_FORCE_INLINE SystemType *AddSystem(Args &&... args)
    {
        return AddSystemToExecutionGroup(MakeUnique<SystemType>(*this, std::forward<Args>(args)...));
    }

    template <class SystemType>
    HYP_FORCE_INLINE SystemType *GetSystem() const
    {
        for (const SystemExecutionGroup &system_execution_group : m_system_execution_groups) {
            if (SystemType *system = system_execution_group.GetSystem<SystemType>()) {
                return system;
            }
        }

        return nullptr;
    }

    template <class Callback>
    HYP_FORCE_INLINE void ForEachEntity(Callback &&callback) const
    {
        MoveEntityGuard move_entity_guard(*this);
        HYP_MT_CHECK_RW(m_entities_data_race_detector);

        ForEach(m_entities, [callback = std::forward<Callback>(callback)](const auto &it)
        {
            const WeakHandle<Entity> &entity_weak = it.first;
            const EntityData &entity_data = it.second;

            Handle<Entity> entity = entity_weak.Lock();

            if (entity.IsValid()) {
                return callback(entity, entity_data);
            }

            return IterationResult::CONTINUE;
        });
    }

    void Initialize();

    void BeginAsyncUpdate(GameCounter::TickUnit delta);
    void EndAsyncUpdate();

    void FlushCommandQueue(GameCounter::TickUnit delta);

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

    template <uint32... Indices>
    HYP_FORCE_INLINE void GetTagsHelper(ID<Entity> id, std::integer_sequence<uint32, Indices...>, Array<EntityTag> &out_tags) const
    {
        ((HasTag<EntityTag(Indices + 1)>(id) ? (void)(out_tags.PushBack(EntityTag(Indices + 1))) : void()), ...);
    }

    template <uint32... Indices>
    HYP_FORCE_INLINE void GetTagsHelper(ID<Entity> id, std::integer_sequence<uint32, Indices...>, uint32 &out_mask) const
    {
        ((HasTag<EntityTag(Indices + 1)>(id) ? (void)(out_mask |= (1u << uint32(Indices))) : void()), ...);
    }

    template <class SystemType>
    SystemType *AddSystemToExecutionGroup(UniquePtr<SystemType> &&system_ptr)
    {
        AssertThrow(system_ptr != nullptr);

        SystemType *ptr = nullptr;

        if ((m_flags & EntityManagerFlags::PARALLEL_SYSTEM_EXECUTION) && system_ptr->AllowParallelExecution()) {
            for (SystemExecutionGroup &system_execution_group : m_system_execution_groups) {
                if (system_execution_group.IsValidForSystem(system_ptr.Get())) {
                    ptr = static_cast<SystemType *>(system_execution_group.AddSystem<SystemType>(std::move(system_ptr)));

                    break;
                }
            }
        }

        if (!ptr) {
            SystemExecutionGroup &system_execution_group = m_system_execution_groups.EmplaceBack(
                system_ptr->RequiresGameThread(),
                system_ptr->AllowUpdate()
            );
            
            ptr = static_cast<SystemType *>(system_execution_group.AddSystem<SystemType>(std::move(system_ptr)));
        }

        return ptr;
    }

    void NotifySystemsOfEntityAdded(ID<Entity> entity, const TypeMap<ComponentID> &component_ids);
    void NotifySystemsOfEntityRemoved(ID<Entity> entity, const TypeMap<ComponentID> &component_ids);

    /*! \brief Removes an entity from the EntityManager.
     *
     *  \param[in] id The Entity to remove.
     *
     *  \return True if the entity was removed, false otherwise.
     */
    bool RemoveEntity(ID<Entity> id);

    bool IsEntityInitializedForSystem(SystemBase *system, ID<Entity> entity) const;

    ThreadID                                                                m_owner_thread_id;
    World                                                                   *m_world;
    Scene                                                                   *m_scene;
    EnumFlags<EntityManagerFlags>                                           m_flags;

    TypeMap<UniquePtr<ComponentContainerBase>>                              m_containers;
    DataRaceDetector                                                        m_containers_data_race_detector;
    EntityContainer                                                         m_entities;
    DataRaceDetector                                                        m_entities_data_race_detector;
    HashMap<TypeID, UniquePtr<EntitySetBase>>                               m_entity_sets;
    mutable Mutex                                                           m_entity_sets_mutex;
    TypeMap<HashSet<TypeID>>                                                m_component_entity_sets;
    EntityManagerCommandQueue                                               m_command_queue;

    mutable AtomicVar<uint32>                                               m_move_entity_rw_mask;

    Array<SystemExecutionGroup>                                             m_system_execution_groups;

    SystemExecutionGroup                                                    *m_root_synchronous_execution_group;

    HashMap<SystemBase *, HashSet<WeakHandle<Entity>>>                      m_system_entity_map;
    mutable Mutex                                                           m_system_entity_map_mutex;

    bool                                                                    m_is_initialized;
};

} // namespace hyperion

#endif