#include <scene/ecs/EntityManager.hpp>
#include <TaskSystem.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

EntityManager *EntityManager::s_instance = nullptr;

ID<Entity> EntityManager::CreateEntity()
{
    auto handle = CreateObject<Entity>();
    const auto insert_result = m_entities.Insert({ handle.GetID(), std::move(handle) });

    return insert_result.first->first;
}

void EntityManager::Update(GameCounter::TickUnit delta)
{
    EntityManager &entity_manager = *this;

    m_systems.ParallelForEach(
        TaskSystem::GetInstance(),
        [&entity_manager, delta](typename TypeMap<UniquePtr<SystemBase>>::KeyValuePairType &pair, UInt index, UInt batch_index)
        {
            pair.second->Process(entity_manager, delta);
        }
    );
}

} // namespace hyperion::v2