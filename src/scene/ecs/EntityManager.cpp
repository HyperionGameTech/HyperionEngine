#include <scene/ecs/EntityManager.hpp>
#include <scene/Entity.hpp>
#include <TaskSystem.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

// #define HYP_ENTITY_MANAGER_SYSTEMS_EXECUTION_PROFILE
#define HYP_ENTITY_MANAGER_SYSTEMS_EXECUTION_PARALLEL

// if the number of systems in a group is less than this value, they will be executed sequentially
static const uint systems_execution_parallel_threshold = 3;

// EntityManagerCommandQueue

EntityManagerCommandQueue::EntityManagerCommandQueue(EntityManagerCommandQueuePolicy policy)
    : m_policy(policy)
{
}

void EntityManagerCommandQueue::Push(EntityManagerCommandProc &&command)
{
    switch (m_policy) {
    case ENTITY_MANAGER_COMMAND_QUEUE_POLICY_EXEC_ON_OWNER_THREAD: {
        const uint current_buffer_index = m_buffer_index.Get(MemoryOrder::ACQUIRE);
        EntityManagerCommandBuffer &buffer = m_command_buffers[current_buffer_index];

        std::unique_lock<std::mutex> guard(buffer.mutex);

        buffer.commands.Push(std::move(command));
        m_count.Increment(1, MemoryOrder::RELEASE);

        break;
    }
    case ENTITY_MANAGER_COMMAND_QUEUE_POLICY_DISCARD:
        // Do nothing
        break;
    }
}

void EntityManagerCommandQueue::Execute(EntityManager &mgr, GameCounter::TickUnit delta)
{
    switch (m_policy) {
    case ENTITY_MANAGER_COMMAND_QUEUE_POLICY_EXEC_ON_OWNER_THREAD: {
        const uint current_buffer_index = m_buffer_index.Get(MemoryOrder::ACQUIRE);

        EntityManagerCommandBuffer &buffer = m_command_buffers[current_buffer_index];

        if (!m_count.Get(MemoryOrder::ACQUIRE)) {
            return;
        }

        std::unique_lock<std::mutex> guard(buffer.mutex);

        // Swap the buffer index to allow new commands to be added to buffer while executing
        const uint next_buffer_index = (current_buffer_index + 1) % m_command_buffers.Size();

        m_buffer_index.Set(next_buffer_index, MemoryOrder::RELEASE);

        while (buffer.commands.Any()) {
            buffer.commands.Pop()(mgr, delta);
        }

        // Update count to be the number of commands in the next buffer (0 unless one of the commands added more commands to the queue)
        m_count.Set(uint(m_command_buffers[next_buffer_index].commands.Size()), MemoryOrder::RELEASE);

        break;
    }
    case ENTITY_MANAGER_COMMAND_QUEUE_POLICY_DISCARD:
        // Do nothing
        break;
    }
}

// EntityManager

ID<Entity> EntityManager::AddEntity()
{
    auto handle = CreateObject<Entity>();

    Threads::AssertOnThread(m_owner_thread_mask);

    return m_entities.AddEntity(std::move(handle));
}

bool EntityManager::RemoveEntity(ID<Entity> id)
{
    Threads::AssertOnThread(m_owner_thread_mask);

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

    m_entities.Erase(it);

    return true;
}

void EntityManager::MoveEntity(ID<Entity> id, EntityManager &other)
{
    /// @TODO: Ensure that it is thread-safe to move an entity from one EntityManager to another
    // As the other EntityManager may be owned by a different thread.

    if (std::addressof(*this) == std::addressof(other)) {
        return;
    }

    // Threads::AssertOnThread(m_owner_thread_mask);

    const auto entity_it = m_entities.Find(id);
    AssertThrowMsg(entity_it != m_entities.End(), "Entity does not exist");

    // Notify systems of the entity being removed from this EntityManager
    NotifySystemsOfEntityRemoved(id, entity_it->second.components);

    const EntityData &entity_data = entity_it->second;

    other.m_entities.AddEntity(entity_it->first, EntityData {
        entity_data.handle
    });

    const auto other_entity_it = other.m_entities.Find(id);
    AssertThrow(other_entity_it != other.m_entities.End());

    Mutex::Guard entity_sets_guard(m_entity_sets_mutex);
    Mutex::Guard other_entity_sets_guard(other.m_entity_sets_mutex);

    TypeMap<ComponentID> new_component_ids;

    for (auto component_info_pair_it = entity_it->second.components.Begin(); component_info_pair_it != entity_it->second.components.End();) {
        const TypeID component_type_id = component_info_pair_it->first;
        const ComponentID component_id = component_info_pair_it->second;

        auto component_container_it = m_containers.Find(component_type_id);
        AssertThrowMsg(component_container_it != m_containers.End(), "Component container does not exist");
        AssertThrowMsg(component_container_it->second->HasComponent(component_id), "Component does not exist");

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

        // Update our entity sets to reflect the change
        auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

        if (component_entity_sets_it != m_component_entity_sets.End()) {
            for (EntitySetTypeID entity_set_id : component_entity_sets_it->second) {
                auto entity_set_it = m_entity_sets.Find(entity_set_id);
                AssertThrowMsg(entity_set_it != m_entity_sets.End(), "Entity set does not exist");

                entity_set_it->second->OnEntityUpdated(id);
            }
        }

        // Update other's entity sets to reflect the change
        auto other_component_entity_sets_it = other.m_component_entity_sets.Find(component_type_id);

        if (other_component_entity_sets_it != other.m_component_entity_sets.End()) {
            for (EntitySetTypeID entity_set_id : other_component_entity_sets_it->second) {
                auto entity_set_it = other.m_entity_sets.Find(entity_set_id);
                AssertThrowMsg(entity_set_it != other.m_entity_sets.End(), "Entity set does not exist");

                entity_set_it->second->OnEntityUpdated(id);
            }
        }

        component_info_pair_it = entity_it->second.components.Erase(component_info_pair_it);
    }

    m_entities.Erase(entity_it);

    // Notify systems of the entity being added to the other EntityManager
    other.NotifySystemsOfEntityAdded(id, new_component_ids);
}

void EntityManager::NotifySystemsOfEntityAdded(ID<Entity> id, const TypeMap<ComponentID> &component_ids)
{
    Array<TypeID> component_type_ids;
    component_type_ids.Reserve(component_ids.Size());

    for (const auto &pair : component_ids) {
        component_type_ids.PushBack(pair.first);
    }

    for (SystemExecutionGroup &group : m_system_execution_groups) {
        for (auto &system_it : group.GetSystems()) {
            if (system_it.second->IsEntityInitialized(id)) {
                continue;
            }

            if (system_it.second->ActsOnComponents(component_type_ids, true)) {
                system_it.second->OnEntityAdded(*this, id);
            }
        }
    }
}

void EntityManager::NotifySystemsOfEntityRemoved(ID<Entity> id, const TypeMap<ComponentID> &component_ids)
{
    Array<TypeID> component_type_ids;
    component_type_ids.Reserve(component_ids.Size());

    for (const auto &pair : component_ids) {
        component_type_ids.PushBack(pair.first);
    }

    for (SystemExecutionGroup &group : m_system_execution_groups) {
        for (auto &system_it : group.GetSystems()) {
            if (system_it.second->ActsOnComponents(component_type_ids, true)) {
                system_it.second->OnEntityRemoved(*this, id);
            }
        }
    }
}

void EntityManager::Update(GameCounter::TickUnit delta)
{
    if (m_command_queue.HasUpdates()) {
        m_command_queue.Execute(*this, delta);
    }

    // Process the execution groups in sequential order
    for (auto &system_execution_group : m_system_execution_groups) {
#ifdef HYP_ENTITY_MANAGER_SYSTEMS_EXECUTION_PROFILE
        const auto start = std::chrono::high_resolution_clock::now();

        puts("----------");
#endif

        system_execution_group.Process(*this, delta);

#ifdef HYP_ENTITY_MANAGER_SYSTEMS_EXECUTION_PROFILE
        const auto stop = std::chrono::high_resolution_clock::now();

        const double ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(stop - start).count();

        printf("Total time: (%f ms)\n", ms);
        puts("----------");
#endif
    }
}

void SystemExecutionGroup::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
#ifdef HYP_ENTITY_MANAGER_SYSTEMS_EXECUTION_PROFILE
    Array<Pair<ThreadMask, double>> times;
    times.Resize(m_systems.Size());
#endif

#ifdef HYP_ENTITY_MANAGER_SYSTEMS_EXECUTION_PARALLEL
    if (m_systems.Size() >= systems_execution_parallel_threshold) {
        TaskSystem::GetInstance().ParallelForEach(
            THREAD_POOL_GENERIC,
            m_systems,
            [&, delta](auto &it, uint index, uint)
            {
#ifdef HYP_ENTITY_MANAGER_SYSTEMS_EXECUTION_PROFILE
                const auto start = std::chrono::high_resolution_clock::now();
#endif

                it.second->Process(entity_manager, delta);

#ifdef HYP_ENTITY_MANAGER_SYSTEMS_EXECUTION_PROFILE
                auto stop = std::chrono::high_resolution_clock::now();

                // set to ms
                times[index] = {
                    Threads::CurrentThreadID(),
                    std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(stop - start).count()
                };
#endif
            }
        );
    } else {
#endif
        for (SizeType i = 0; i < m_systems.Size(); i++) {
            const auto &it = *(m_systems.Begin() + i);
#ifdef HYP_ENTITY_MANAGER_SYSTEMS_EXECUTION_PROFILE
            const auto start = std::chrono::high_resolution_clock::now();
#endif

            it.second->Process(entity_manager, delta);

#ifdef HYP_ENTITY_MANAGER_SYSTEMS_EXECUTION_PROFILE
            const auto stop = std::chrono::high_resolution_clock::now();

            // set to ms
            times[i] = {
                Threads::CurrentThreadID(),
                (std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(stop - start).count())
            };
#endif
        }

#ifdef HYP_ENTITY_MANAGER_SYSTEMS_EXECUTION_PARALLEL
    }
#endif

#ifdef HYP_ENTITY_MANAGER_SYSTEMS_EXECUTION_PROFILE
    for (SizeType i = 0; i < m_systems.Size(); i++) {
        printf(
            "%s (%f ms, thread: %d)\t",
            (*(m_systems.Begin() + i)).first.Name().LookupString(),
            times[i].second,
            times[i].first
        );
    }

    puts("");
#endif
}

} // namespace hyperion::v2
