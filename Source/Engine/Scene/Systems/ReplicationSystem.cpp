/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Systems/ReplicationSystem.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <ReplicationSystem.generated.inl>

namespace Hyperion {

void ReplicationSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);
}

void ReplicationSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void ReplicationSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    // @TODO
}

} // namespace Hyperion
