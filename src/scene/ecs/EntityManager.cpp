#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/SceneComponent.hpp>
#include <scene/Entity.hpp>
#include <TaskSystem.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

ID<Entity> EntityManager::AddEntity()
{
    auto handle = CreateObject<Entity>();

    const ID<Entity> id = m_entities.AddEntity(std::move(handle));

    if (m_scene) {
        AddComponent(id, SceneComponent {
            m_scene->GetID()
        });
    }

    return id;
}

void EntityManager::RemoveEntity(ID<Entity> id)
{
    auto it = m_entities.Find(id);

    if (it == m_entities.End()) {
        return;
    }

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