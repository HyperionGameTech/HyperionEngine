/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <scene/Entity.hpp>
#include <scene/Scene.hpp>

#include <core/Handle.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/utilities/Format.hpp>

#include <core/object/HypClassRegistry.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypData.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

// if the number of systems in a group is less than this value, they will be executed sequentially
static constexpr double g_system_execution_group_lag_spike_threshold = 50.0;
static constexpr uint32 g_entity_manager_command_queue_warning_size = 8192;

#define HYP_SYSTEMS_PARALLEL_EXECUTION
// #define HYP_SYSTEMS_LAG_SPIKE_DETECTION

#pragma region EntityManagerCommandQueue

EntityManagerCommandQueue::EntityManagerCommandQueue(EnumFlags<EntityManagerCommandQueueFlags> flags)
    : m_flags(flags)
{
}

void EntityManagerCommandQueue::AwaitEmpty()
{
    HYP_SCOPE;

    if (!(m_flags & EntityManagerCommandQueueFlags::EXEC_COMMANDS))
    {
        return;
    }

    const uint32 current_buffer_index = m_buffer_index.Get(MemoryOrder::ACQUIRE);
    EntityManagerCommandBuffer& buffer = m_command_buffers[current_buffer_index];

    std::unique_lock<std::mutex> guard(buffer.mutex);

    while (m_count.Get(MemoryOrder::ACQUIRE))
    {
        m_condition_variable.wait(guard);
    }
}

void EntityManagerCommandQueue::Push(EntityManagerCommandProc&& command)
{
    HYP_SCOPE;

    if (!(m_flags & EntityManagerCommandQueueFlags::EXEC_COMMANDS))
    {
        return;
    }

    const uint32 current_buffer_index = m_buffer_index.Get(MemoryOrder::ACQUIRE);
    EntityManagerCommandBuffer& buffer = m_command_buffers[current_buffer_index];

    std::unique_lock<std::mutex> guard(buffer.mutex);

    buffer.commands.Push(std::move(command));
    m_count.Increment(1, MemoryOrder::RELEASE);

    if (buffer.commands.Size() >= g_entity_manager_command_queue_warning_size)
    {
        HYP_LOG(ECS, Warning, "EntityManager command queue has grown past warning threshold size of ({} >= {}). This may indicate FlushCommandQueue() is not being called which would be a memory leak.",
            buffer.commands.Size(), g_entity_manager_command_queue_warning_size);

#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif
    }
}

void EntityManagerCommandQueue::Execute(EntityManager& mgr, GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    if (!(m_flags & EntityManagerCommandQueueFlags::EXEC_COMMANDS))
    {
        return;
    }

    const uint32 current_buffer_index = m_buffer_index.Get(MemoryOrder::ACQUIRE);

    EntityManagerCommandBuffer& buffer = m_command_buffers[current_buffer_index];

    if (!m_count.Get(MemoryOrder::ACQUIRE))
    {
        return;
    }

    std::unique_lock<std::mutex> guard(buffer.mutex);

    // Swap the buffer index to allow new commands to be added to buffer while executing
    const uint32 next_buffer_index = (current_buffer_index + 1) % m_command_buffers.Size();

    m_buffer_index.Set(next_buffer_index, MemoryOrder::RELEASE);

    while (buffer.commands.Any())
    {
        buffer.commands.Pop()(mgr, delta);
    }

    // Update count to be the number of commands in the next buffer (0 unless one of the commands added more commands to the queue)
    m_count.Set(uint32(m_command_buffers[next_buffer_index].commands.Size()), MemoryOrder::RELEASE);
}

#pragma endregion EntityManagerCommandQueue

#pragma region EntityToEntityManagerMap

void EntityToEntityManagerMap::ForEachEntityManager(ProcRef<void(EntityManager* entity_manager)> proc) const
{
    HashSet<Handle<EntityManager>> entity_managers;

    {
        Mutex::Guard guard(m_mutex);

        entity_managers.Reserve(m_map.Size());

        for (auto& it : m_map)
        {
            if (Handle<EntityManager> entity_manager = it.second.Lock(); entity_manager.IsValid())
            {
                entity_managers.Insert(entity_manager);
            }
        }
    }

    for (const Handle<EntityManager>& entity_manager : entity_managers)
    {
        proc(entity_manager.Get());
    }
}

Task<bool> EntityToEntityManagerMap::PerformActionWithEntity(ID<Entity> entity_id, Proc<void(EntityManager*, ID<Entity>)>&& callback)
{
    HYP_SCOPE;

    Task<bool> task;

    if (!callback)
    {
        task.Fulfill(false);

        return task;
    }

    m_mutex.Lock();

    const auto it = m_map.Find(entity_id);

    if (it == m_map.End())
    {
        m_mutex.Unlock();

        task.Fulfill(false);

        return task;
    }

    Handle<EntityManager> entity_manager = it->second.Lock();

    if (!entity_manager.IsValid())
    {
        m_mutex.Unlock();

        task.Fulfill(false);

        return task;
    }

    if (Threads::IsOnThread(entity_manager->GetOwnerThreadID()) || !entity_manager->GetOwnerThreadID().IsValid())
    {
        // Mutex must not be locked when callback() is called - the callback could perform
        // actions that directly or undirectly require re-locking the mutex.
        // Instead, unlock the mutex once we are sure the EntityManager will not be deleted before using it
        // (either by ensuring we're on the same thread that owns the EntityManager, or by enqueuing the command)
        m_mutex.Unlock();

        if (entity_manager->HasEntity(entity_id))
        {
            callback(entity_manager, entity_id);

            task.Fulfill(true);
        }
        else
        {
            task.Fulfill(false);
        }
    }
    else
    {
        Threads::GetThread(entity_manager->GetOwnerThreadID())->GetScheduler().Enqueue([entity_manager_weak = entity_manager.ToWeak(), entity_id, callback = std::move(callback), promise = task.Promise()]()
            {
                Handle<EntityManager> entity_manager = entity_manager_weak.Lock();

                if (!entity_manager)
                {
                    promise->Fulfill(false);

                    return;
                }

                if (entity_manager->HasEntity(entity_id))
                {
                    callback(entity_manager.Get(), entity_id);

                    promise->Fulfill(true);
                }
                else
                {
                    promise->Fulfill(false);
                }
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);

        m_mutex.Unlock();
    }

    return task;
}

void EntityToEntityManagerMap::PerformActionWithEntity_FireAndForget(ID<Entity> entity_id, Proc<void(EntityManager*, ID<Entity>)>&& callback)
{
    HYP_SCOPE;

    if (!callback)
    {
        return;
    }

    m_mutex.Lock();

    const auto it = m_map.Find(entity_id);

    if (it == m_map.End())
    {
        m_mutex.Unlock();
    }

    Handle<EntityManager> entity_manager = it->second.Lock();

    if (!entity_manager.IsValid())
    {
        m_mutex.Unlock();
        return;
    }

    if (Threads::IsOnThread(entity_manager->GetOwnerThreadID()) || !entity_manager->GetOwnerThreadID().IsValid())
    {
        // Mutex must not be locked when callback() is called - the callback could perform
        // actions that directly or undirectly require re-locking the mutex.
        // Instead, unlock the mutex once we are sure the EntityManager will not be deleted before using it
        // (either by ensuring we're on the same thread that owns the EntityManager, or by enqueuing the command)
        m_mutex.Unlock();

        if (entity_manager->HasEntity(entity_id))
        {
            callback(entity_manager, entity_id);
        }
    }
    else
    {
        Threads::GetThread(entity_manager->GetOwnerThreadID())->GetScheduler().Enqueue([entity_manager_weak = entity_manager.ToWeak(), entity_id, callback = std::move(callback)]()
            {
                Handle<EntityManager> entity_manager = entity_manager_weak.Lock();

                if (!entity_manager)
                {
                    return;
                }

                if (entity_manager->HasEntity(entity_id))
                {
                    callback(entity_manager.Get(), entity_id);
                }
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);

        m_mutex.Unlock();
    }
}

#pragma endregion EntityToEntityManagerMap

#pragma region EntityManager

EntityToEntityManagerMap EntityManager::s_entity_to_entity_manager_map {};

EntityToEntityManagerMap& EntityManager::GetEntityToEntityManagerMap()
{
    return s_entity_to_entity_manager_map;
}

bool EntityManager::IsValidComponentType(TypeID component_type_id)
{
    return ComponentInterfaceRegistry::GetInstance().GetComponentInterface(component_type_id) != nullptr;
}

bool EntityManager::IsEntityTagComponent(TypeID component_type_id)
{
    const IComponentInterface* component_interface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(component_type_id);

    if (!component_interface)
    {
        return false;
    }

    return component_interface->IsEntityTag();
}

EntityManager::EntityManager(const ThreadID& owner_thread_id, Scene* scene, EnumFlags<EntityManagerFlags> flags)
    : m_owner_thread_id(owner_thread_id),
      m_world(scene != nullptr ? scene->GetWorld() : nullptr),
      m_scene(scene),
      m_flags(flags),
      m_command_queue(g_game_thread == owner_thread_id ? EntityManagerCommandQueueFlags::EXEC_COMMANDS : EntityManagerCommandQueueFlags::NONE),
      m_root_synchronous_execution_group(nullptr),
      m_move_entity_rw_mask(0)
{
    AssertThrow(scene != nullptr);

    // add initial component containers
    for (const IComponentInterface* component_interface : ComponentInterfaceRegistry::GetInstance().GetComponentInterfaces())
    {
        AssertThrow(component_interface != nullptr);

        ComponentContainerFactoryBase* component_container_factory = component_interface->GetComponentContainerFactory();
        AssertThrow(component_container_factory != nullptr);

        UniquePtr<ComponentContainerBase> component_container = component_container_factory->Create();
        AssertThrow(component_container != nullptr);

        m_containers.Set(component_interface->GetTypeID(), std::move(component_container));
    }
}

EntityManager::~EntityManager()
{
    GetEntityToEntityManagerMap().RemoveEntityManager(this);
}

void EntityManager::InitializeSystem(const Handle<SystemBase>& system)
{
    HYP_SCOPE;

    AssertThrowMsg(m_world != nullptr, "EntityManager must be associated with a World before initializing systems.");

    AssertThrow(system.IsValid());

    InitObject(system);

    for (auto entities_it = m_entities.Begin(); entities_it != m_entities.End(); ++entities_it)
    {
        const Handle<Entity> entity = entities_it->first.Lock();

        if (!entity.IsValid())
        {
            HYP_LOG(ECS, Warning, "Entity with ID #{} is expired or invalid", entities_it->first.GetID().Value());

            continue;
        }

        EntityData& entity_data = entities_it->second;

        const TypeMap<ComponentID>& component_ids = entity_data.components;

        if (system->ActsOnComponents(component_ids.Keys(), true))
        {
            { // critical section
                Mutex::Guard guard(m_system_entity_map_mutex);

                auto system_entity_it = m_system_entity_map.Find(system.Get());

                // Check if the system already has this entity initialized
                if (system_entity_it != m_system_entity_map.End() && (system_entity_it->second.FindAs(entity) != system_entity_it->second.End()))
                {
                    continue;
                }

                m_system_entity_map[system.Get()].Insert(entity);
            }

            HYP_LOG(ECS, Debug, "Adding entity #{} to system {}", entity.GetID().Value(), system->GetName());

            system->OnEntityAdded(entity);
        }
    }
}

void EntityManager::ShutdownSystem(const Handle<SystemBase>& system)
{
    HYP_SCOPE;

    AssertThrowMsg(m_world != nullptr, "EntityManager must be associated with a World before shutting down systems.");

    AssertThrow(system.IsValid());

    for (auto entities_it = m_entities.Begin(); entities_it != m_entities.End(); ++entities_it)
    {
        const Handle<Entity> entity = entities_it->first.Lock();

        if (!entity.IsValid())
        {
            HYP_LOG(ECS, Warning, "Entity with ID #{} is expired or invalid", entities_it->first.GetID().Value());

            continue;
        }

        EntityData& entity_data = entities_it->second;

        const TypeMap<ComponentID>& component_ids = entity_data.components;

        if (system->ActsOnComponents(component_ids.Keys(), true) && IsEntityInitializedForSystem(system.Get(), entity))
        {
            { // critical section
                Mutex::Guard guard(m_system_entity_map_mutex);

                auto system_entity_it = m_system_entity_map.Find(system.Get());

                // Check if the system already has this entity initialized
                if (system_entity_it != m_system_entity_map.End() && system_entity_it->second.Contains(entity))
                {
                    continue;
                }

                system_entity_it->second.Erase(entity);
            }

            HYP_LOG(ECS, Debug, "Removing entity #{} from system {}", entity.GetID().Value(), system->GetName());

            system->OnEntityRemoved(entity);
        }
    }

    system->Shutdown();
}

void EntityManager::Init()
{
    Threads::AssertOnThread(m_owner_thread_id);

    MoveEntityGuard move_entity_guard(*this);

    Array<Handle<SystemBase>> systems;

    for (SystemExecutionGroup& group : m_system_execution_groups)
    {
        for (auto& system_it : group.GetSystems())
        {
            const Handle<SystemBase>& system = system_it.second;
            AssertThrow(system.IsValid());

            systems.PushBack(system);
        }
    }

    for (const Handle<SystemBase>& system : systems)
    {
        // Must be called before InitObject() is called on Systems to ensure the system is initialized if
        // other systems end up adding/removing components that trigger OnEntityAdded() or OnEntityRemoved() calls.
        system->InitComponentInfos_Internal();
    }

    if (m_world != nullptr)
    {
        for (const Handle<SystemBase>& system : systems)
        {
            // Initialize the system
            InitializeSystem(system);
        }
    }

    SetReady(true);
}

void EntityManager::Shutdown()
{
    HYP_SCOPE;

    if (IsReady() && m_world != nullptr)
    {
        MoveEntityGuard move_entity_guard(*this);

        Array<Handle<SystemBase>> systems;

        for (SystemExecutionGroup& group : m_system_execution_groups)
        {
            for (auto& system_it : group.GetSystems())
            {
                const Handle<SystemBase>& system = system_it.second;
                AssertThrow(system.IsValid());

                systems.PushBack(system);
            }
        }

        for (const Handle<SystemBase>& system : systems)
        {
            // Shutdown the system
            ShutdownSystem(system);
        }
    }

    SetReady(false);
}

void EntityManager::SetWorld(World* world)
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_id);

    if (world == m_world)
    {
        return;
    }

    // If EntityManager is initialized we need to notify all of our systems that the world has changed.
    if (IsInitCalled())
    {
        Array<Handle<SystemBase>> systems;

        for (SystemExecutionGroup& group : m_system_execution_groups)
        {
            for (auto& system_it : group.GetSystems())
            {
                const Handle<SystemBase>& system = system_it.second;
                AssertThrow(system.IsValid());

                systems.PushBack(system);
            }
        }

        MoveEntityGuard move_entity_guard(*this);

        if (m_world != nullptr)
        {
            for (const Handle<SystemBase>& system : systems)
            {
                ShutdownSystem(system);
            }
        }

        m_world = world;

        if (m_world != nullptr)
        { // notify systems of entity added for the new world

            for (const Handle<SystemBase>& system : systems)
            {
                InitializeSystem(system);
            }
        }

        return;
    }

    m_world = world;
}

Handle<Entity> EntityManager::AddEntity()
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    Handle<Entity> entity = CreateObject<Entity>();

    MoveEntityGuard move_entity_guard(*this);
    HYP_MT_CHECK_RW(m_entities_data_race_detector);

    m_entities.AddEntity(entity);

    GetEntityToEntityManagerMap().Add(entity.GetID(), WeakHandleFromThis());

    return entity;
}

void EntityManager::AddExistingEntity(const Handle<Entity>& entity)
{
    HYP_SCOPE;

    if (!entity.IsValid())
    {
        return;
    }

    Threads::AssertOnThread(m_owner_thread_id);

    // Get the current EntityManager for the entity, if it exists
    Handle<EntityManager> other_entity_manager = GetEntityToEntityManagerMap().GetEntityManager(entity.GetID());

    if (other_entity_manager.IsValid())
    {
        if (other_entity_manager.Get() == this)
        {
            // Entity is already in this EntityManager, no need to add it again
            return;
        }

        // Move the Entity from the other EntityManager to this one.
        // NOTE: Non async since we have asserted we are on our owner thread.
        other_entity_manager->MoveEntity(entity, HandleFromThis());

        return;
    }

    MoveEntityGuard move_entity_guard(*this);
    HYP_MT_CHECK_RW(m_entities_data_race_detector);

    m_entities.AddEntity(entity);

    GetEntityToEntityManagerMap().Add(entity.GetID(), WeakHandleFromThis());
}

bool EntityManager::RemoveEntity(ID<Entity> entity_id)
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_id);

    if (!entity_id.IsValid())
    {
        return false;
    }

    // to keep ID valid while removing the entity
    WeakHandle<Entity> entity_weak { entity_id };

    MoveEntityGuard move_entity_guard(*this);
    HYP_MT_CHECK_RW(m_entities_data_race_detector);

    auto entities_it = m_entities.Find(entity_id);

    if (entities_it == m_entities.End())
    {
        return false;
    }

    EntityData& entity_data = entities_it->second;

    // Notify systems of the entity being removed from this EntityManager
    NotifySystemsOfEntityRemoved(entity_id, entity_data.components);

    // Lock the entity sets mutex
    Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

    for (auto component_info_pair_it = entity_data.components.Begin(); component_info_pair_it != entity_data.components.End();)
    {
        const TypeID component_type_id = component_info_pair_it->first;
        const ComponentID component_id = component_info_pair_it->second;

        auto component_container_it = m_containers.Find(component_type_id);

        AssertThrowMsg(component_container_it != m_containers.End(), "Component container does not exist");
        AssertThrow(component_container_it->second->RemoveComponent(component_id));

        // Erase the component from the entity's component map,
        // advance the iterator
        component_info_pair_it = entity_data.components.Erase(component_info_pair_it);

        auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

        if (component_entity_sets_it != m_component_entity_sets.End())
        {
            // For each entity set that can contain this component type, update the entity set
            for (TypeID entity_set_type_id : component_entity_sets_it->second)
            {
                EntitySetBase& entity_set = *m_entity_sets.At(entity_set_type_id);

                entity_set.OnEntityUpdated(entity_id);
            }
        }
    }

    GetEntityToEntityManagerMap().Remove(entity_id);

    m_entities.Erase(entities_it);

    return true;
}

void EntityManager::MoveEntity(const Handle<Entity>& entity, const Handle<EntityManager>& other)
{
    HYP_SCOPE;

    AssertThrow(entity.IsValid());
    AssertThrow(other.IsValid());

    if (this == other.Get())
    {
        return;
    }

    {
        MoveEntityGuard move_entity_guard(*this);

        HYP_MT_CHECK_RW(m_entities_data_race_detector);

        const auto entities_it = m_entities.Find(entity);
        AssertThrowMsg(entities_it != m_entities.End(), "Entity does not exist");

        EntityData& entity_data = entities_it->second;
        NotifySystemsOfEntityRemoved(entity, entity_data.components);
    }

    uint64 state = m_move_entity_rw_mask.BitOr(g_move_entity_write_flag, MemoryOrder::ACQUIRE_RELEASE)
        | other->m_move_entity_rw_mask.BitOr(g_move_entity_write_flag, MemoryOrder::ACQUIRE_RELEASE);

    // Wait for read mask to be empty
    while (state & g_move_entity_read_mask)
    {
        state = m_move_entity_rw_mask.Get(MemoryOrder::ACQUIRE)
            | other->m_move_entity_rw_mask.Get(MemoryOrder::ACQUIRE);

        Threads::Sleep(0);
    }

    TypeMap<ComponentID> new_component_ids;

    {
        HYP_MT_CHECK_RW(m_entities_data_race_detector);
        HYP_MT_CHECK_RW(other->m_entities_data_race_detector);

        const auto entities_it = m_entities.Find(entity);
        AssertThrowMsg(entities_it != m_entities.End(), "Entity does not exist");

        EntityData& entity_data = entities_it->second;

        other->m_entities.AddEntity(entity);
        EntityData& other_entity_data = other->m_entities.GetEntityData(entity);

        { // Critical section
            Mutex::Guard entity_sets_guard(m_entity_sets_mutex);
            Mutex::Guard other_entity_sets_guard(other->m_entity_sets_mutex);

            for (auto component_info_pair_it = entity_data.components.Begin(); component_info_pair_it != entity_data.components.End();)
            {
                const TypeID component_type_id = component_info_pair_it->first;
                const ComponentID component_id = component_info_pair_it->second;

                auto component_container_it = m_containers.Find(component_type_id);
                AssertThrowMsg(component_container_it != m_containers.End(), "Component container does not exist");
                AssertThrowMsg(component_container_it->second->HasComponent(component_id), "Component does not exist in component container");

                auto other_component_container_it = other->m_containers.Find(component_type_id);

                if (other_component_container_it == other->m_containers.End())
                {
                    auto insert_result = other->m_containers.Set(component_type_id, component_container_it->second->GetFactory()->Create());
                    AssertThrowMsg(insert_result.second, "Failed to insert component container");

                    other_component_container_it = insert_result.first;
                }

                Optional<ComponentID> new_component_id = component_container_it->second->MoveComponent(component_id, *other_component_container_it->second);
                AssertThrowMsg(new_component_id.HasValue(), "Failed to move component");

                new_component_ids.Set(component_type_id, *new_component_id);

                other_entity_data.components.Set(component_type_id, *new_component_id);

                // Update iterator, erase the component from the entity's component map
                component_info_pair_it = entity_data.components.Erase(component_info_pair_it);

                // Update our entity sets to reflect the change
                auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

                if (component_entity_sets_it != m_component_entity_sets.End())
                {
                    for (TypeID entity_set_type_id : component_entity_sets_it->second)
                    {
                        EntitySetBase& entity_set = *m_entity_sets.At(entity_set_type_id);

                        entity_set.OnEntityUpdated(entity);
                    }
                }

                // Update other's entity sets to reflect the change
                auto other_component_entity_sets_it = other->m_component_entity_sets.Find(component_type_id);

                if (other_component_entity_sets_it != other->m_component_entity_sets.End())
                {
                    for (TypeID entity_set_type_id : other_component_entity_sets_it->second)
                    {
                        EntitySetBase& entity_set = *other->m_entity_sets.At(entity_set_type_id);

                        entity_set.OnEntityUpdated(entity);
                    }
                }
            }
        }

        m_entities.Erase(entities_it);

        GetEntityToEntityManagerMap().Remap(entity, other);
    }

    other->m_move_entity_rw_mask.Increment(2, MemoryOrder::RELEASE);

    m_move_entity_rw_mask.BitAnd(~g_move_entity_write_flag, MemoryOrder::RELEASE);

    if (Threads::IsOnThread(other->GetOwnerThreadID()))
    {
        other->m_move_entity_rw_mask.BitAnd(~g_move_entity_write_flag, MemoryOrder::RELEASE);
        other->NotifySystemsOfEntityAdded(entity, new_component_ids);
        other->m_move_entity_rw_mask.Decrement(2, MemoryOrder::RELEASE);
    }
    else
    {
        Threads::GetThread(other->GetOwnerThreadID())->GetScheduler().Enqueue([other = other, entity = entity, new_component_ids = std::move(new_component_ids)]()
            {
                other->m_move_entity_rw_mask.BitAnd(~g_move_entity_write_flag, MemoryOrder::RELEASE);
                other->NotifySystemsOfEntityAdded(entity, new_component_ids);
                other->m_move_entity_rw_mask.Decrement(2, MemoryOrder::RELEASE);
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

void EntityManager::AddComponent(const Handle<Entity>& entity, AnyRef component)
{
    const TypeID component_type_id = component.GetTypeID();
    EnsureValidComponentType(component_type_id);

    AssertThrowMsg(entity.IsValid(), "Invalid entity");

    Threads::AssertOnThread(m_owner_thread_id);

    TypeMap<ComponentID> component_ids;

    { // critical section for entity data
        MoveEntityGuard move_entity_guard(*this);
        HYP_MT_CHECK_READ(m_entities_data_race_detector);

        EntityData* entity_data = m_entities.TryGetEntityData(entity);
        AssertThrowMsg(entity_data != nullptr, "Entity with ID #%u does not exist", entity.GetID().Value());

        // Update the EntityData
        auto component_it = entity_data->FindComponent(component_type_id);
        // @TODO: Replace the component if it already exists
        AssertThrowMsg(component_it == entity_data->components.End(), "Entity already has component of type %u", component_type_id.Value());

        ComponentContainerBase* container = TryGetContainer(component_type_id);
        AssertThrowMsg(container != nullptr, "Component container does not exist for component type %u", component_type_id.Value());

        const ComponentID component_id = container->AddComponent(component);
        entity_data->components.Set(component.GetTypeID(), component_id);

        { // Lock the entity sets mutex
            Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

            // Update entity sets
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

        component_ids = entity_data->components;
    }

    // Notify systems that entity is being added to them
    NotifySystemsOfEntityAdded(entity, component_ids);
}

bool EntityManager::RemoveComponent(TypeID component_type_id, ID<Entity> entity_id)
{
    HYP_SCOPE;

    EnsureValidComponentType(component_type_id);

    Threads::AssertOnThread(m_owner_thread_id);

    if (!entity_id.IsValid())
    {
        return false;
    }

    MoveEntityGuard move_entity_guard(*this);
    HYP_MT_CHECK_READ(m_entities_data_race_detector);

    EntityData* entity_data = m_entities.TryGetEntityData(entity_id);

    if (!entity_data)
    {
        return false;
    }

    auto component_it = entity_data->FindComponent(component_type_id);
    if (component_it == entity_data->components.End())
    {
        return false;
    }

    const ComponentID component_id = component_it->second;

    // Notify systems that entity is being removed from them
    TypeMap<ComponentID> removed_component_ids;
    removed_component_ids.Set(component_type_id, component_id);

    NotifySystemsOfEntityRemoved(entity_id, removed_component_ids);

    ComponentContainerBase* container = TryGetContainer(component_type_id);

    if (!container)
    {
        return false;
    }

    if (!container->RemoveComponent(component_id))
    {
        return false;
    }

    entity_data->components.Erase(component_it);

    // Lock the entity sets mutex
    Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

    auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

    if (component_entity_sets_it != m_component_entity_sets.End())
    {
        for (TypeID entity_set_type_id : component_entity_sets_it->second)
        {
            EntitySetBase& entity_set = *m_entity_sets.At(entity_set_type_id);

            entity_set.OnEntityUpdated(entity_id);
        }
    }

    return true;
}

bool EntityManager::HasTag(ID<Entity> entity_id, EntityTag tag) const
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_id);

    if (!entity_id.IsValid())
    {
        return false;
    }

    const IComponentInterface* component_interface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(tag);

    if (!component_interface)
    {
        HYP_LOG(ECS, Error, "No EntityTagComponent registered for EntityTag {}", tag);

        return false;
    }

    const TypeID component_type_id = component_interface->GetTypeID();

    return HasComponent(component_type_id, entity_id);
}

void EntityManager::AddTag(ID<Entity> entity_id, EntityTag tag)
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_id);

    if (!entity_id.IsValid())
    {
        return;
    }

    Handle<Entity> entity { entity_id };

    const IComponentInterface* component_interface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(tag);

    if (!component_interface)
    {
        HYP_LOG(ECS, Error, "No EntityTagComponent registered for EntityTag {}", tag);

        return;
    }

    const TypeID component_type_id = component_interface->GetTypeID();

    if (HasComponent(component_type_id, entity_id))
    {
        return;
    }

    ComponentContainerBase* container = TryGetContainer(component_type_id);
    AssertThrowMsg(container != nullptr, "Component container does not exist for component type %u", component_type_id.Value());

    HypData component_hyp_data;

    if (!component_interface->CreateInstance(component_hyp_data))
    {
        HYP_LOG(ECS, Error, "Failed to create EntityTagComponent for EntityTag {}", tag);

        return;
    }

    AddComponent(entity, component_hyp_data.ToRef());
}

bool EntityManager::RemoveTag(ID<Entity> entity_id, EntityTag tag)
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_id);

    if (!entity_id.IsValid())
    {
        return false;
    }

    const IComponentInterface* component_interface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(tag);

    if (!component_interface)
    {
        HYP_LOG(ECS, Error, "No EntityTagComponent registered for EntityTag {}", tag);

        return false;
    }

    const TypeID component_type_id = component_interface->GetTypeID();

    return RemoveComponent(component_type_id, entity_id);
}

void EntityManager::NotifySystemsOfEntityAdded(const Handle<Entity>& entity, const TypeMap<ComponentID>& component_ids)
{
    HYP_SCOPE;

    if (!entity.IsValid())
    {
        return;
    }

    // If the EntityManager is initialized, notify systems of the entity being added
    // otherwise, the systems will be notified when the EntityManager is initialized
    if (!IsInitCalled() || m_world == nullptr)
    {
        return;
    }

    for (SystemExecutionGroup& group : m_system_execution_groups)
    {
        for (auto& system_it : group.GetSystems())
        {
            if (system_it.second->ActsOnComponents(component_ids.Keys(), true))
            {
                { // critical section
                    Mutex::Guard guard(m_system_entity_map_mutex);

                    auto system_entity_it = m_system_entity_map.Find(system_it.second.Get());

                    // Check if the system already has this entity initialized
                    if (system_entity_it != m_system_entity_map.End() && (system_entity_it->second.FindAs(entity) != system_entity_it->second.End()))
                    {
                        continue;
                    }

                    m_system_entity_map[system_it.second.Get()].Insert(entity);
                }

                system_it.second->OnEntityAdded(entity);
            }
        }
    }
}

void EntityManager::NotifySystemsOfEntityRemoved(ID<Entity> entity_id, const TypeMap<ComponentID>& component_ids)
{
    HYP_SCOPE;

    if (!entity_id.IsValid())
    {
        return;
    }

    if (!IsInitCalled() || m_world == nullptr)
    {
        return;
    }

    WeakHandle<Entity> entity_weak { entity_id };

    for (SystemExecutionGroup& group : m_system_execution_groups)
    {
        for (auto& system_it : group.GetSystems())
        {
            if (system_it.second->ActsOnComponents(component_ids.Keys(), true))
            {
                { // critical section
                    Mutex::Guard guard(m_system_entity_map_mutex);

                    auto system_entity_it = m_system_entity_map.Find(system_it.second.Get());

                    if (system_entity_it == m_system_entity_map.End())
                    {
                        continue;
                    }

                    auto entity_it = system_entity_it->second.FindAs(entity_id);

                    if (entity_it == system_entity_it->second.End())
                    {
                        continue;
                    }

                    system_entity_it->second.Erase(entity_it);
                }

                system_it.second->OnEntityRemoved(entity_id);
            }
        }
    }
}

void EntityManager::BeginAsyncUpdate(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    m_root_synchronous_execution_group = nullptr;

    TaskBatch* root_task_batch = nullptr;
    TaskBatch* last_task_batch = nullptr;

    // Prepare task dependencies
    for (SizeType index = 0; index < m_system_execution_groups.Size(); index++)
    {
        SystemExecutionGroup& system_execution_group = m_system_execution_groups[index];

        if (!system_execution_group.AllowUpdate())
        {
            continue;
        }

        TaskBatch* current_task_batch = system_execution_group.GetTaskBatch();
        AssertDebug(current_task_batch != nullptr);

        AssertDebugMsg(current_task_batch->IsCompleted(), "TaskBatch for SystemExecutionGroup is not completed: %u tasks enqueued", current_task_batch->num_enqueued);

        current_task_batch->ResetState();

        // Add tasks to batches before kickoff
        system_execution_group.StartProcessing(delta);

        if (system_execution_group.RequiresGameThread())
        {
            if (m_root_synchronous_execution_group != nullptr)
            {
                m_root_synchronous_execution_group->GetTaskBatch()->next_batch = current_task_batch;
            }
            else
            {
                m_root_synchronous_execution_group = &system_execution_group;
            }

            continue;
        }

        if (!root_task_batch)
        {
            root_task_batch = current_task_batch;
        }

        if (last_task_batch != nullptr)
        {
            if (current_task_batch->executors.Any())
            {
                last_task_batch->next_batch = current_task_batch;
                last_task_batch = current_task_batch;
            }
        }
        else
        {
            last_task_batch = current_task_batch;
        }
    }

    // Kickoff first task
    if (root_task_batch != nullptr && (root_task_batch->executors.Any() || root_task_batch->next_batch != nullptr))
    {
#ifdef HYP_SYSTEMS_PARALLEL_EXECUTION
        TaskSystem::GetInstance().EnqueueBatch(root_task_batch);
#endif
    }
}

void EntityManager::EndAsyncUpdate()
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    for (SystemExecutionGroup& system_execution_group : m_system_execution_groups)
    {
        if (!system_execution_group.AllowUpdate() || system_execution_group.RequiresGameThread())
        {
            continue;
        }

        system_execution_group.FinishProcessing();
    }

    if (m_root_synchronous_execution_group != nullptr)
    {
        m_root_synchronous_execution_group->FinishProcessing(/* execute_blocking */ true);

        m_root_synchronous_execution_group = nullptr;
    }

#if defined(HYP_DEBUG_MODE) && defined(HYP_SYSTEMS_LAG_SPIKE_DETECTION)
    for (SystemExecutionGroup& system_execution_group : m_system_execution_groups)
    {
        const PerformanceClock& performance_clock = system_execution_group.GetPerformanceClock();
        const double elapsed_time_ms = performance_clock.Elapsed() / 1000.0;

        if (elapsed_time_ms >= g_system_execution_group_lag_spike_threshold)
        {
            HYP_LOG(ECS, Warning, "SystemExecutionGroup spike detected: {} ms", elapsed_time_ms);

            for (const auto& it : system_execution_group.GetPerformanceClocks())
            {
                HYP_LOG(ECS, Debug, "\tSystem {} performance: {}", it.first->GetName(), it.second.Elapsed() / 1000.0);
            }
        }
    }
#endif
}

void EntityManager::FlushCommandQueue(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_id);

    if (m_command_queue.HasUpdates())
    {
        m_command_queue.Execute(*this, delta);
    }
}

bool EntityManager::IsEntityInitializedForSystem(SystemBase* system, ID<Entity> entity_id) const
{
    HYP_SCOPE;

    if (!system)
    {
        return false;
    }

    Mutex::Guard guard(m_system_entity_map_mutex);

    const auto it = m_system_entity_map.Find(system);

    if (it == m_system_entity_map.End())
    {
        return false;
    }

    return it->second.FindAs(entity_id) != it->second.End();
}

void EntityManager::GetSystemClasses(Array<const HypClass*>& out_classes) const
{
    HYP_NOT_IMPLEMENTED();
}

#pragma endregion EntityManager

#pragma region SystemExecutionGroup

SystemExecutionGroup::SystemExecutionGroup(bool requires_game_thread, bool allow_update)
    : m_requires_game_thread(requires_game_thread),
      m_allow_update(allow_update),
      m_task_batch(MakeUnique<TaskBatch>())
{
}

SystemExecutionGroup::~SystemExecutionGroup()
{
}

void SystemExecutionGroup::StartProcessing(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    AssertDebug(AllowUpdate());

#if defined(HYP_DEBUG_MODE) && defined(HYP_SYSTEMS_LAG_SPIKE_DETECTION)
    m_performance_clock.Start();

    for (auto& it : m_systems)
    {
        SystemBase* system = it.second.Get();

        m_performance_clocks.Set(system, PerformanceClock());
    }
#endif

    for (auto& it : m_systems)
    {
        SystemBase* system = it.second.Get();

        m_task_batch->AddTask([this, system, delta]
            {
                HYP_NAMED_SCOPE_FMT("Processing system {}", system->GetName());

#if defined(HYP_DEBUG_MODE) && defined(HYP_SYSTEMS_LAG_SPIKE_DETECTION)
                PerformanceClock& performance_clock = m_performance_clocks[system];
                performance_clock.Start();
#endif

                system->Process(delta);

#if defined(HYP_DEBUG_MODE) && defined(HYP_SYSTEMS_LAG_SPIKE_DETECTION)
                performance_clock.Stop();
#endif
            });
    }
}

void SystemExecutionGroup::FinishProcessing(bool execute_blocking)
{
    AssertDebug(AllowUpdate());

#ifdef HYP_SYSTEMS_PARALLEL_EXECUTION
    if (execute_blocking)
    {
        m_task_batch->ExecuteBlocking(/* execute_dependent_batches */ true);
    }
    else
    {
        m_task_batch->AwaitCompletion();
    }
#else
    m_task_batch->ExecuteBlocking(/* execute_dependent_batches */ true);
#endif

    for (auto& it : m_systems)
    {
        SystemBase* system = it.second.Get();

        if (system->m_after_process_procs.Any())
        {
            for (auto& proc : system->m_after_process_procs)
            {
                proc();
            }

            system->m_after_process_procs.Clear();
        }
    }

#ifdef HYP_DEBUG_MODE
    m_performance_clock.Stop();
#endif
}

#pragma endregion SystemExecutionGroup

} // namespace hyperion
