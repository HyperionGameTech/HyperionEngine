#include <scene/ecs/EntityManager.hpp>
#include <scene/Entity.hpp>
#include <TaskSystem.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

EntityManager::ComponentSetMutexHolder EntityManager::s_component_set_mutex_holder = { };

ID<Entity> EntityManager::AddEntity()
{
    auto handle = CreateObject<Entity>();

    return m_entities.AddEntity(std::move(handle));
}

void EntityManager::RemoveEntity(ID<Entity> id)
{
    auto it = m_entities.Find(id);

    if (it == m_entities.End()) {
        return;
    }

    Mutex::Guard guard(m_entity_sets_mutex);

    for (auto &pair : it->second.components) {
        const TypeID component_type_id = pair.first;
        const ComponentID component_id = pair.second;

        auto component_it = m_containers.Find(component_type_id);

        AssertThrowMsg(component_it != m_containers.End(), "Component container does not exist");
        component_it->second->RemoveComponent(component_id);

        auto component_entity_sets_it = m_component_entity_sets.Find(component_type_id);

        if (component_entity_sets_it != m_component_entity_sets.End()) {
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
    const auto entity_it = m_entities.Find(id);
    AssertThrowMsg(entity_it != m_entities.End(), "Entity does not exist");

    const EntityData &entity_data = entity_it->second;

    other.m_entities.AddEntity(entity_it->first, EntityData {
        entity_data.handle
    });

    const auto other_entity_it = other.m_entities.Find(id);
    AssertThrow(other_entity_it != other.m_entities.End());

    Mutex::Guard guard(m_entity_sets_mutex);

    for (const auto &pair : entity_data.components) {
        const TypeID component_type_id = pair.first;
        const ComponentID component_id = pair.second;

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
    }

    m_entities.Erase(entity_it);
}

void EntityManager::Update(GameCounter::TickUnit delta)
{
    // Process the execution groups in sequential order
    for (auto &system_execution_group : m_system_execution_groups) {
        system_execution_group.Process(*this, delta);
    }
}

void SystemExecutionGroup::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    // DebugLog(
    //     LogType::Debug,
    //     "SystemExecutionGroup::Process: Processing execution group with %zu systems\n",
    //     m_systems.Size()
    // );

    m_systems.ParallelForEach(
        TaskSystem::GetInstance(),
        [&entity_manager, delta](auto &it, UInt, UInt)
        {
            it.second->Process(entity_manager, delta);
        }
    );
}

} // namespace hyperion::v2