/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Systems/PlayerComponentSystem.hpp>

#include <Scene/EntityManager.hpp>

#include <PlayerComponentSystem.generated.inl>

namespace Hyperion {

void PlayerComponentSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    if (!entity->HasComponent<PlayerComponent>())
    {
        entity->AddComponent<PlayerComponent>(PlayerComponent {});
    }
}

} // namespace Hyperion
