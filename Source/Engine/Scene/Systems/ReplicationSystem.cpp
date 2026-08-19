/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include "Components/ReplicationStateComponent.hpp"
#include <ScenePch.hpp>

#include <Scene/Systems/ReplicationSystem.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Framework/Server/GameServer.hpp>

#include <ReplicationSystem.generated.inl>

namespace Hyperion {

void ReplicationSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    Assert(g_gameServer != nullptr);

    entity->AddComponent<ReplicationStateComponent>(ReplicationStateComponent {
        g_gameServer->AllocNetId()
    });
}

void ReplicationSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    ReplicationStateComponent& rsc = entity->GetComponent<ReplicationStateComponent>();
    g_gameServer->FreeNetId(rsc.netId);

    entity->RemoveComponent<ReplicationStateComponent>();
}

void ReplicationSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        EntityManager* entityManager = scene->GetEntityManager();

        for (auto [entity, replicationState, _] : entityManager->GetEntitySet<ReplicationStateComponent, TagComponent<EntityTag::UpdateReplication>>())
        {
            // @TODO
        }
    }
}

} // namespace Hyperion
