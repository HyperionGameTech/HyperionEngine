/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <scene/Entity.hpp>
#include <scene/Scene.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/utilities/Format.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

// if the number of systems in a group is less than this value, they will be executed sequentially
static const double g_system_execution_group_lag_spike_threshold = 50.0;
static const uint32 g_entity_manager_command_queue_warning_size = 8192;

#define HYP_SYSTEMS_PARALLEL_EXECUTION

#pragma region EntityManagerCommandQueue

EntityManagerCommandQueue::EntityManagerCommandQueue(EnumFlags<EntityManagerCommandQueueFlags> flags)
    : m_flags(flags)
{
}

void EntityManagerCommandQueue::AwaitEmpty()
{
    HYP_SCOPE;

    if (!(m_flags & EntityManagerCommandQueueFlags::EXEC_COMMANDS)) {
        return;
    }
    
    const uint32 current_buffer_index = m_buffer_index.Get(MemoryOrder::ACQUIRE);
    EntityManagerCommandBuffer &buffer = m_command_buffers[current_buffer_index];

    std::unique_lock<std::mutex> guard(buffer.mutex);

    while (m_count.Get(MemoryOrder::ACQUIRE)) {
        m_condition_variable.wait(guard);
    }
}

void EntityManagerCommandQueue::Push(EntityManagerCommandProc &&command)
{
    HYP_SCOPE;

    if (!(m_flags & EntityManagerCommandQueueFlags::EXEC_COMMANDS)) {
        return;
    }
    
    const uint32 current_buffer_index = m_buffer_index.Get(MemoryOrder::ACQUIRE);
    EntityManagerCommandBuffer &buffer = m_command_buffers[current_buffer_index];

    std::unique_lock<std::mutex> guard(buffer.mutex);

    buffer.commands.Push(std::move(command));
    m_count.Increment(1, MemoryOrder::RELEASE);

    if (buffer.commands.Size() >= g_entity_manager_command_queue_warning_size) {
        HYP_LOG(ECS, Warning, "EntityManager command queue has grown past warning threshold size of ({} >= {}). This may indicate FlushCommandQueue() is not being called which would be a memory leak.",
            buffer.commands.Size(), g_entity_manager_command_queue_warning_size);

#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif
    }
}

void EntityManagerCommandQueue::Execute(EntityManager &mgr, GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    if (!(m_flags & EntityManagerCommandQueueFlags::EXEC_COMMANDS)) {
        return;
    }
    
    const uint32 current_buffer_index = m_buffer_index.Get(MemoryOrder::ACQUIRE);

    EntityManagerCommandBuffer &buffer = m_command_buffers[current_buffer_index];

    if (!m_count.Get(MemoryOrder::ACQUIRE)) {
        return;
    }

    std::unique_lock<std::mutex> guard(buffer.mutex);

    // Swap the buffer index to allow new commands to be added to buffer while executing
    const uint32 next_buffer_index = (current_buffer_index + 1) % m_command_buffers.Size();

    m_buffer_index.Set(next_buffer_index, MemoryOrder::RELEASE);

    while (buffer.commands.Any()) {
        buffer.commands.Pop()(mgr, delta);
    }

    // Update count to be the number of commands in the next buffer (0 unless one of the commands added more commands to the queue)
    m_count.Set(uint32(m_command_buffers[next_buffer_index].commands.Size()), MemoryOrder::RELEASE);
}

#pragma endregion EntityManagerCommandQueue

#pragma region EntityToEntityManagerMap

Task<bool> EntityToEntityManagerMap::PerformActionWithEntity(ID<Entity> id, void(*callback)(EntityManager *entity_manager, ID<Entity> id))
{
    HYP_SCOPE;

    Task<bool> task;

    m_mutex.Lock();

    const auto it = m_map.FindAs(id);

    if (it == m_map.End()) {
        m_mutex.Unlock();

        task.Fulfill(false);

        return task;
    }

    EntityManager *entity_manager = it->second;

    auto Impl = [id, callback, task_executor = task.Initialize()](EntityManager &entity_manager, GameCounter::TickUnit delta)
    {
        AssertThrow(entity_manager.HasEntity(id));

        callback(&entity_manager, id);

        task_executor->Fulfill(true);
    };
    
    if (entity_manager->GetOwnerThreadMask() & Threads::CurrentThreadID().GetValue()) {
        // Mutex must not be locked when callback() is called - the callback could perform
        // actions that directly or undirectly require re-locking the mutex.
        // Instead, unlock the mutex once we are sure the EntityManager will not be deleted before using it
        // (either by ensuring we're on the same thread that owns the EntityManager, or by enqueuing the command)
        m_mutex.Unlock();

        Impl(*entity_manager, 0.0f);
    } else {
        entity_manager->PushCommand(Impl);

        m_mutex.Unlock();
    }

    return task;
}

#pragma endregion EntityToEntityManagerMap

#pragma region EntityManager

EntityToEntityManagerMap EntityManager::s_entity_to_entity_manager_map { };

EntityToEntityManagerMap &EntityManager::GetEntityToEntityManagerMap()
{
    return s_entity_to_entity_manager_map;
}

bool EntityManager::IsValidComponentType(TypeID component_type_id)
{
    return ComponentInterfaceRegistry::GetInstance().GetComponentInterface(component_type_id) != nullptr;
}

bool EntityManager::IsEntityTagComponent(TypeID component_type_id)
{
    const ComponentInterface *component_interface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(component_type_id);

    if (!component_interface) {
        return false;
    }

    return component_interface->GetFlags() & ComponentInterfaceFlags::ENTITY_TAG;
}

EntityManager::EntityManager(ThreadMask owner_thread_mask, Scene *scene, EnumFlags<EntityManagerFlags> flags)
    : m_owner_thread_mask(owner_thread_mask),
      m_world(scene != nullptr ? scene->GetWorld() : nullptr),
      m_scene(scene),
      m_flags(flags),
      m_command_queue(Threads::IsThreadInMask(g_game_thread, owner_thread_mask) ? EntityManagerCommandQueueFlags::EXEC_COMMANDS : EntityManagerCommandQueueFlags::NONE),
      m_is_initialized(false)
{
    AssertThrow(scene != nullptr);

    // add initial component containers
    for (const ComponentInterface *component_interface : ComponentInterfaceRegistry::GetInstance().GetComponentInterfaces()) {
        AssertThrow(component_interface != nullptr);

        ComponentContainerFactoryBase *component_container_factory = component_interface->GetComponentContainerFactory();
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

void EntityManager::Initialize()
{
    // Threads::AssertOnThread(m_owner_thread_mask);
    HYP_MT_CHECK_READ(m_entities_data_race_detector);

    AssertThrow(!m_is_initialized);

    for (SystemExecutionGroup &group : m_system_execution_groups) {
        for (auto &system_it : group.GetSystems()) {
            for (auto entities_it = m_entities.Begin(); entities_it != m_entities.End(); ++entities_it) {
                const Handle<Entity> entity = entities_it->first.Lock();

                if (!entity.IsValid()) {
                    continue;
                }

                const TypeMap<ComponentID> &component_ids = entities_it->second.components;

                if (system_it.second->IsEntityInitialized(entity.GetID())) {
                    continue;
                }

                if (system_it.second->ActsOnComponents(component_ids.Keys(), true)) {
                    system_it.second->OnEntityAdded(entity);
                }
            }
        }
    }

    m_is_initialized = true;
}

void EntityManager::SetWorld(World *world)
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_mask);

    if (world == m_world) {
        return;
    }

    // If EntityManager is initialized we need to notify all of our systems that the world has changed.
    if (m_is_initialized) {
        for (SystemExecutionGroup &group : m_system_execution_groups) {
            for (auto &system_it : group.GetSystems()) {
                system_it.second->SetWorld(world);
            }
        }
    }

    m_world = world;
}

Handle<Entity> EntityManager::AddEntity()
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_mask);
    HYP_MT_CHECK_RW(m_entities_data_race_detector);
        
    ObjectContainer<Entity> &container = ObjectPool::GetObjectContainerHolder().GetOrCreate<Entity>();
    
    HypObjectMemory<Entity> *memory = container.Allocate();
    memory->Construct();
    
    Handle<Entity> entity { memory };

    HYP_LOG(ECS, Debug, "Add entity #{} to entity manager {}", entity.GetID().Value(), (void *)this);
    GetEntityToEntityManagerMap().Add(entity.GetID(), this);

    m_entities.AddEntity(entity);

    return entity;
}

bool EntityManager::RemoveEntity(ID<Entity> id)
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_owner_thread_mask);
    HYP_MT_CHECK_RW(m_entities_data_race_detector);

    if (!id.IsValid()) {
        return false;
    }

    auto it = m_entities.Find(id);

    if (it == m_entities.End()) {
        return false;
    }

    // Notify systems of the entity being removed from this EntityManager
    NotifySystemsOfEntityRemoved(id, it->second.components);

    // Lock the entity sets mutex
    Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

    for (auto component_info_pair_it = it->second.components.Begin(); component_info_pair_it != it->second.components.End();) {
        const TypeID component_type_id = component_info_pair_it->first;
        const ComponentID component_id = component_info_pair_it->second;

        auto component_container_it = m_containers.Find(component_type_id);

        AssertThrowMsg(component_container_it != m_containers.End(), "Component container does not exist");
        AssertThrow(component_container_it->second->RemoveComponent(component_id));

        // Erase the component from the entity's component map,
        // advance the iterator
        component_info_pair_it = it->second.components.Erase(component_info_pair_it);

        auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

        if (component_entity_sets_it != m_component_entity_sets.End()) {
            // For each entity set that can contain this component type, update the entity set
            for (EntitySetTypeID entity_set_id : component_entity_sets_it->second) {
                auto entity_set_it = m_entity_sets.Find(entity_set_id);
                AssertThrowMsg(entity_set_it != m_entity_sets.End(), "Entity set does not exist");

                entity_set_it->second->OnEntityUpdated(id);
            }
        }
    }
    
    HYP_LOG(ECS, Debug, "Remove entity #{} from entity manager {}", id.Value(), (void *)this);
    GetEntityToEntityManagerMap().Remove(id);

    m_entities.Erase(it);

    return true;
}

void EntityManager::MoveEntity(const Handle<Entity> &entity, EntityManager &other)
{
    HYP_SCOPE;

    if (std::addressof(*this) == std::addressof(other)) {
        return;
    }

    HYP_MT_CHECK_RW(m_entities_data_race_detector);
    HYP_MT_CHECK_RW(other.m_entities_data_race_detector);

    // we could be on either entity manager owner thread
    // @TODO Use appropriate mutexes to protect the entity sets
    Threads::AssertOnThread(m_owner_thread_mask | other.m_owner_thread_mask);

    AssertThrow(entity.IsValid());

    HYP_LOG(ECS, Debug, "Moving Entity #{} from EntityManager {} to {}", entity.GetID().Value(), (void *)this, (void *)&other);

    // Threads::AssertOnThread(m_owner_thread_mask);

    const auto entity_it = m_entities.Find(entity);
    AssertThrowMsg(entity_it != m_entities.End(), "Entity does not exist");

    // Notify systems of the entity being removed from this EntityManager
    NotifySystemsOfEntityRemoved(entity, entity_it->second.components);

    other.m_entities.AddEntity(entity);

    GetEntityToEntityManagerMap().Remap(entity, &other);

    const auto other_entity_it = other.m_entities.Find(entity);
    AssertThrow(other_entity_it != other.m_entities.End());

    TypeMap<ComponentID> new_component_ids;

    { // Critical section
        Mutex::Guard entity_sets_guard(m_entity_sets_mutex);
        Mutex::Guard other_entity_sets_guard(other.m_entity_sets_mutex);

        for (auto component_info_pair_it = entity_it->second.components.Begin(); component_info_pair_it != entity_it->second.components.End();) {
            const TypeID component_type_id = component_info_pair_it->first;
            const ComponentID component_id = component_info_pair_it->second;

            auto component_container_it = m_containers.Find(component_type_id);
            AssertThrowMsg(component_container_it != m_containers.End(), "Component container does not exist");
            AssertThrowMsg(component_container_it->second->HasComponent(component_id), "Component does not exist in component container");

            auto other_component_container_it = other.m_containers.Find(component_type_id);

            if (other_component_container_it == other.m_containers.End()) {
                auto insert_result = other.m_containers.Set(component_type_id, component_container_it->second->GetFactory()->Create());
                AssertThrowMsg(insert_result.second, "Failed to insert component container");

                other_component_container_it = insert_result.first;
            }

            Optional<ComponentID> new_component_id = component_container_it->second->MoveComponent(component_id, *other_component_container_it->second);
            AssertThrowMsg(new_component_id.HasValue(), "Failed to move component");

            new_component_ids.Set(component_type_id, *new_component_id);

            other_entity_it->second.components.Set(component_type_id, *new_component_id);

            // Update iterator, erase the component from the entity's component map
            component_info_pair_it = entity_it->second.components.Erase(component_info_pair_it);

            // Update our entity sets to reflect the change
            auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

            if (component_entity_sets_it != m_component_entity_sets.End()) {
                for (EntitySetTypeID entity_set_id : component_entity_sets_it->second) {
                    auto entity_set_it = m_entity_sets.Find(entity_set_id);
                    AssertThrowMsg(entity_set_it != m_entity_sets.End(), "Entity set does not exist");

                    entity_set_it->second->OnEntityUpdated(entity);
                }
            }

            // Update other's entity sets to reflect the change
            auto other_component_entity_sets_it = other.m_component_entity_sets.Find(component_type_id);

            if (other_component_entity_sets_it != other.m_component_entity_sets.End()) {
                for (EntitySetTypeID entity_set_id : other_component_entity_sets_it->second) {
                    auto entity_set_it = other.m_entity_sets.Find(entity_set_id);
                    AssertThrowMsg(entity_set_it != other.m_entity_sets.End(), "Entity set does not exist");

                    entity_set_it->second->OnEntityUpdated(entity);
                }
            }
        }

        m_entities.Erase(entity_it);
    }

    // Notify systems of the entity being added to the other EntityManager
    other.NotifySystemsOfEntityAdded(entity, new_component_ids);
}

void EntityManager::NotifySystemsOfEntityAdded(ID<Entity> entity, const TypeMap<ComponentID> &component_ids)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_entities_data_race_detector);

    if (!entity.IsValid()) {
        return;
    }

    // If the EntityManager is initialized, notify systems of the entity being added
    // otherwise, the systems will be notified when the EntityManager is initialized
    if (!m_is_initialized) {
        return;
    }

    Handle<Entity> entity_handle { entity };

    for (SystemExecutionGroup &group : m_system_execution_groups) {
        for (auto &system_it : group.GetSystems()) {
            if (system_it.second->IsEntityInitialized(entity)) {
                continue;
            }

            if (system_it.second->ActsOnComponents(component_ids.Keys(), true)) {
                system_it.second->OnEntityAdded(entity_handle);
            }
        }
    }
}

void EntityManager::NotifySystemsOfEntityRemoved(ID<Entity> entity, const TypeMap<ComponentID> &component_ids)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_entities_data_race_detector);

    if (!entity.IsValid()) {
        return;
    }

    if (!m_is_initialized) {
        return;
    }

    for (SystemExecutionGroup &group : m_system_execution_groups) {
        for (auto &system_it : group.GetSystems()) {
            if (!system_it.second->IsEntityInitialized(entity)) {
                continue;
            }

            if (system_it.second->ActsOnComponents(component_ids.Keys(), true)) {
                system_it.second->OnEntityRemoved(entity);
            }
        }
    }
}

void EntityManager::BeginAsyncUpdate(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_mask);
    
    TaskBatch *root_task_batch = nullptr;
    TaskBatch *last_task_batch = nullptr;

    // Prepare task dependencies
    for (SizeType index = 0; index < m_system_execution_groups.Size(); index++) {
        SystemExecutionGroup &system_execution_group = m_system_execution_groups[index];

        TaskBatch *current_task_batch = system_execution_group.GetTaskBatch();

        if (!root_task_batch) {
            root_task_batch = current_task_batch;
        }

        current_task_batch->ResetState();

        // Add tasks to batches before kickoff
        system_execution_group.StartProcessing(delta);

        if (last_task_batch != nullptr) {
            if (current_task_batch->num_enqueued > 0) {
                last_task_batch->next_batch = current_task_batch;
                last_task_batch = current_task_batch;
            }
        } else {
            last_task_batch = current_task_batch;
        }
    }

    // Kickoff first task
    if (root_task_batch != nullptr && (root_task_batch->num_enqueued > 0 || root_task_batch->next_batch != nullptr)) {
#ifdef HYP_SYSTEMS_PARALLEL_EXECUTION
        TaskSystem::GetInstance().EnqueueBatch(root_task_batch);
#endif
    }
}

void EntityManager::EndAsyncUpdate()
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_mask);

    for (SystemExecutionGroup &system_execution_group : m_system_execution_groups) {
        system_execution_group.FinishProcessing();
    }

#ifdef HYP_DEBUG_MODE
    for (SystemExecutionGroup &system_execution_group : m_system_execution_groups) {
        const PerformanceClock &performance_clock = system_execution_group.GetPerformanceClock();
        const double elapsed_time_ms = performance_clock.Elapsed() / 1000.0;

        if (elapsed_time_ms >= g_system_execution_group_lag_spike_threshold) {
            HYP_LOG(ECS, Warning, "SystemExecutionGroup spike detected: {} ms", elapsed_time_ms);

            for (const auto &it : system_execution_group.GetPerformanceClocks()) {
                HYP_LOG(ECS, Debug, "\tSystem {} performance: {}", it.first->GetName(), it.second.Elapsed() / 1000.0);
            }
        }
    }
#endif
}

void EntityManager::FlushCommandQueue(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_owner_thread_mask);
    
    if (m_command_queue.HasUpdates()) {
        m_command_queue.Execute(*this, delta);
    }
}

#pragma endregion EntityManager

#pragma region SystemExecutionGroup

SystemExecutionGroup::SystemExecutionGroup()
    : m_task_batch(MakeUnique<TaskBatch>())
{
}

SystemExecutionGroup::~SystemExecutionGroup()
{
}

void SystemExecutionGroup::StartProcessing(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

#ifdef HYP_DEBUG_MODE
    m_performance_clock.Start();

    for (auto &it : m_systems) {
        SystemBase *system = it.second.Get();

        m_performance_clocks.Set(system, PerformanceClock());
    }
#endif

    for (auto &it : m_systems) {
        SystemBase *system = it.second.Get();

        if (system->GetInitializedEntities().Empty()) {
            continue;
        }

        StaticMessage debug_name;
        debug_name.value = system->GetName();

        m_task_batch->AddTask([this, system, delta]
        {
            HYP_NAMED_SCOPE_FMT("Processing system {}", system->GetName());

#ifdef HYP_DEBUG_MODE
            PerformanceClock &performance_clock = m_performance_clocks[system];
            performance_clock.Start();
#endif

            system->Process(delta);

#ifdef HYP_DEBUG_MODE
            performance_clock.Stop();
#endif
        }, debug_name);
    }
}

void SystemExecutionGroup::FinishProcessing()
{
    if (m_task_batch->num_enqueued != 0 || m_task_batch->next_batch != nullptr) {
#ifdef HYP_SYSTEMS_PARALLEL_EXECUTION
        m_task_batch->AwaitCompletion();
#else
        m_task_batch->ExecuteBlocking(/* execute_dependent_batches */ true);
#endif
    }

#ifdef HYP_DEBUG_MODE
    m_performance_clock.Stop();
#endif
}

#pragma endregion SystemExecutionGroup

} // namespace hyperion
