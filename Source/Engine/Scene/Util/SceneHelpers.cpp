/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/EntityTag.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Scene/Components/PlayerComponent.hpp>

#include <Framework/Client/GameClient.hpp>

namespace Hyperion {
namespace SceneHelpers {

Camera* FindMainCamera(World& world)
{
    for (Scene* scene : world.GetScenes())
    {
        Assert(scene != nullptr);
        
        if (scene->GetSceneFlags() & SceneFlags::FOREGROUND)
        {
            EntityManager* entityManager = scene->GetEntityManager();
            Assert(entityManager != nullptr);

            if (!entityManager)
            {
                continue;
            }

            for (auto [entity, _0, _1] : entityManager->GetEntitySet<EntityType<Camera>, TagComponent<EntityTag::PrimaryCamera>>())
            {
                return StaticCast<Camera>(entity);
            }
        }
    }
    
    return nullptr;
}

Entity* FindMyLocalPlayerEntity(const Scene& scene, net::NetConnectionId ownerConnectionId)
{
    if (ownerConnectionId == Invalid<net::NetConnectionId>
        || g_gameClient == nullptr
        || !g_gameClient->IsConnected()
        || g_gameClient->GetNetClient().GetConnectionId() != ownerConnectionId)
    {
        return nullptr;
    }

    for (auto [entity, playerComponent] : scene.GetEntityManager()->GetEntitySet<PlayerComponent>())
    {
        if (playerComponent.connectionId == ownerConnectionId)
        {
            return entity;
        }
    }

    return nullptr;
}

bool IsLocalPlayerEntity(const Entity& entity)
{
    const PlayerComponent* playerComponent = entity.TryGetComponent<PlayerComponent>();

    return playerComponent != nullptr && playerComponent->IsLocalPlayer();
}

} // namespace SceneHelpers
} // namespace Hyperion
