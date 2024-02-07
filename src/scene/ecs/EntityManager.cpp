#include <scene/ecs/EntityManager.hpp>
#include <scene/Entity.hpp>
#include <TaskSystem.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

// EntityManagerCommandQueue

EntityManagerCommandQueue::EntityManagerCommandQueue(EntityManagerCommandQueuePolicy policy)
    : m_policy(policy)
{
}

void EntityManagerCommandQueue::Push(EntityManagerCommandProc &&command)
{
    switch (m_policy) {
    case ENTITY_MANAGER_COMMAND_QUEUE_POLICY_EXEC_ON_OWNER_THREAD: {
        std::unique_lock<std::mutex> guard(m_mutex);

        m_commands.Push(std::move(command));

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
        if (!m_count.Get(MemoryOrder::ACQUIRE)) {
            return;
        }

        std::unique_lock<std::mutex> guard(m_mutex);

        while (m_commands.Any()) {
            m_commands.Pop()(mgr, delta);
        }

        m_count.Set(0, MemoryOrder::RELEASE);

        m_has_commands.notify_all();

        break;
    }
    case ENTITY_MANAGER_COMMAND_QUEUE_POLICY_DISCARD:
        // Do nothing
        break;
    }
}

void EntityManagerCommandQueue::WaitForFree()
{
    switch (m_policy) {
    case ENTITY_MANAGER_COMMAND_QUEUE_POLICY_EXEC_ON_OWNER_THREAD: {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_has_commands.wait(
            lock,
            [this]()
            { 
                return m_count.Get(MemoryOrder::ACQUIRE) == 0;
            }
        );

        break;
    }
    case ENTITY_MANAGER_COMMAND_QUEUE_POLICY_DISCARD:
        // Do nothing
        break;
    }
}

// EntityManager

EntityManager::ComponentSetMutexHolder::MutexMap EntityManager::ComponentSetMutexHolder::s_mutex_map = { };

EntityManager::ComponentSetMutexHolder EntityManager::s_component_set_mutex_holder = { };

ID<Entity> EntityManager::AddEntity()
{
    auto handle = CreateObject<Entity>();

    Threads::AssertOnThread(m_owner_thread_mask);

    return m_entities.AddEntity(std::move(handle));
}

void EntityManager::RemoveEntity(ID<Entity> id)
{
    Threads::AssertOnThread(m_owner_thread_mask);

    auto it = m_entities.Find(id);

    if (it == m_entities.End()) {
        return;
    }

    // Lock the entity sets mutex
    Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

    for (auto component_info_pair_it = it->second.components.Begin(); component_info_pair_it != it->second.components.End();) {
        const TypeID component_type_id = component_info_pair_it->first;

        NotifySystemsOfEntityRemoved(component_type_id, id);

        const ComponentID component_id = component_info_pair_it->second;

        // Erase the component from the entity's component map
        component_info_pair_it = it->second.components.Erase(component_info_pair_it);

        // Mutex *component_set_mutex = s_component_set_mutex_holder.TryGetMutex(component_type_id);
        // AssertThrowMsg(component_set_mutex, "Failed to get mutex for component type %s", component_type_id.Name().LookupString());

        // // Lock the mutex for the component type
        // Mutex::Guard component_set_guard(*component_set_mutex);

        auto component_container_it = m_containers.Find(component_type_id);

        AssertThrowMsg(component_container_it != m_containers.End(), "Component container does not exist");
        component_container_it->second->RemoveComponent(component_id);

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

    const EntityData &entity_data = entity_it->second;

    other.m_entities.AddEntity(entity_it->first, EntityData {
        entity_data.handle
    });

    const auto other_entity_it = other.m_entities.Find(id);
    AssertThrow(other_entity_it != other.m_entities.End());

    Mutex::Guard entity_sets_guard(m_entity_sets_mutex);

    for (const auto &pair : entity_data.components) {
        const TypeID component_type_id = pair.first;

        // Notify systems
        NotifySystemsOfEntityRemoved(component_type_id, id);

        const ComponentID component_id = pair.second;

        // Mutex *component_set_mutex = s_component_set_mutex_holder.TryGetMutex(component_type_id);
        // AssertThrowMsg(component_set_mutex, "Failed to get mutex for component type %s", component_type_id.Name().LookupString());

        // // Lock the mutex for the component type
        // Mutex::Guard component_set_guard(*component_set_mutex);

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

        other_entity_it->second.components.Set(component_type_id, new_component_id.Get());

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

        // Notify other systems
        other.NotifySystemsOfEntityAdded(component_type_id, id);
    }

    m_entities.Erase(entity_it);
}

void EntityManager::NotifySystemsOfEntityAdded(TypeID component_type_id, ID<Entity> id)
{
    for (SystemExecutionGroup &group : m_system_execution_groups) {
        for (auto &system_it : group.GetSystems()) {
            if (system_it.second->HasComponentTypeID(component_type_id)) {
                system_it.second->OnEntityAdded(*this, component_type_id, id);
            }
        }
    }
}

void EntityManager::NotifySystemsOfEntityRemoved(TypeID component_type_id, ID<Entity> id)
{
    for (SystemExecutionGroup &group : m_system_execution_groups) {
        for (auto &system_it : group.GetSystems()) {
            if (system_it.second->HasComponentTypeID(component_type_id)) {
                system_it.second->OnEntityRemoved(*this, component_type_id, id);
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
        system_execution_group.Process(*this, delta);
    }
}

void SystemExecutionGroup::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    TaskSystem::GetInstance().ParallelForEach(
        THREAD_POOL_GENERIC,
        m_systems,
        [&entity_manager, delta](auto &it, UInt, UInt)
        {
            it.second->Process(entity_manager, delta);
        }
    );
}

} // namespace hyperion::v2