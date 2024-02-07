#include <scene/ecs/EntityManagerCommand.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <Threads.hpp>

namespace hyperion::v2 {

void EntityManagerCommand::Execute(EntityManager &manager)
{
    Threads::AssertOnThread(THREAD_GAME);

    m_proc(manager);
}

// void AddComponentCommand::Execute(EntityManager &manager)
// {
//     manager.AddComponent(m_entity_id, m_component_type_id, std::move(m_component));
// }

// void RemoveComponentCommand::Execute(EntityManager &manager)
// {
//     manager.RemoveComponent(m_entity_id, m_component_type_id, m_component_id);
// }

} // namespace hyperion::v2