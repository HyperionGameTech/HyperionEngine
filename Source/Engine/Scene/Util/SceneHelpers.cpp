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
#include <Scene/Components/ReplicationStateComponent.hpp>
#include <Scene/Components/CharacterControllerComponent.hpp>

#include <Physics/PhysicsWorld.hpp>
#include <Physics/PhysicsShape.hpp>

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

            for (auto [camera, _1] : entityManager->GetEntitySet<EntityType<Camera>, TagComponent<EntityTag::PrimaryCamera>>())
            {
                return camera;
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

bool CanSimulateEntityPhysics(const Entity& entity)
{
    // Has authority? Yes, we can simulate physics on it
    if (EngineGlobals::HasAuthority())
    {
        return true;
    }

    // Connected client: we can simulate non-Replicated entities.
    return !entity.HasComponent<ReplicationStateComponent>()
        && !entity.HasTag<EntityTag::Replicated>();
}

float GetCapsuleHeightOffset(const CharacterControllerComponent& component)
{
    if (CapsulePhysicsShape* capsuleShape = DynamicCast<CapsulePhysicsShape>(component.shape.Get()))
    {
        // amount to adjust the the final offset by after applying capsule height
        // otherwise the node will sit directly on top of the capsule,
        // when it should be contained within the capsule
        static constexpr float CapsuleHeightOffset = 0.1f;

        return capsuleShape->GetHeight() - CapsuleHeightOffset;
    }

    return 0.0f;
}

void MoveCharacter(Entity* entity, CharacterControllerComponent& component, const PlayerMove& move, Vec3f& outResultTranslation)
{
    PhysicsWorldBase* physicsWorld = entity->GetWorld()->GetPhysicsWorld();
    Assert(physicsWorld != nullptr);

    outResultTranslation = Vec3f(0.0f);

    if (!component.physicsHandle)
    {
        return;
    }

    // View direction is client-authoritative and carried per-move so both sides
    // derive an identical walk direction.
    const Vec3f horizontalView(move.viewDirection.x, 0.0f, move.viewDirection.z);

    if (horizontalView.LengthSquared() > 0.0001f)
    {
        component.viewDirection = move.viewDirection;
    }

    Vec3f walkDirection;

    if (move.movementInput.LengthSquared() > 0.0001f)
    {
        Vec3f forward = Vec3f(component.viewDirection.x, 0.0f, component.viewDirection.z).Normalize();
        Vec3f right = Vec3f(0.0f, 1.0f, 0.0f).Cross(forward).Normalize();

        walkDirection = (forward * move.movementInput.y + right * move.movementInput.x) * component.moveSpeed;
    }

    if (move.jumpRequested)
    {
        physicsWorld->ApplyCharacterJump(component.physicsHandle);
    }

    physicsWorld->SetCharacterWalkDirection(component.physicsHandle, walkDirection);

    physicsWorld->StepCharacterController(component.physicsHandle, move.deltaTime);
    physicsWorld->GetCharacterState(component.physicsHandle, component.translation, component.isOnGround);

    outResultTranslation = component.translation + Vec3f(0.0f, GetCapsuleHeightOffset(component), 0.0f);

    entity->SetWorldTranslation(outResultTranslation, TransformChangeType::Simulation);
}

} // namespace SceneHelpers
} // namespace Hyperion
