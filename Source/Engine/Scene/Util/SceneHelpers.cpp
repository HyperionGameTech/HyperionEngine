/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Core/Math/MathUtil.hpp>

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
    // if dedicated server / single player / listen server then we can simulate physics for the Entity.
    if (EngineGlobals::HasAuthority())
    {
        return true;
    }

    // --
    // clients that are connected to a server but not authoritive can simulate NON replicated props.
    // like kicking around a can of beans.
    // server doesn't need to know you're kicking a can of beans.
    // --
    // unless your game necessitates that..? (then make it Replicated!)
    // --
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

    outResultTranslation = Vec3f::Zero();

    if (!component.physicsHandle)
    {
        return;
    }

    // View direction is client-authoritative and carried per-move so both sides derive an identical walk direction
    const Vec3f viewDirection = move.GetViewDirection();
    const Vec2f movementInput = move.GetMovementInput();

    const Vec3f horizontalView = { viewDirection.x, 0.0f, viewDirection.z };

    if (horizontalView.LengthSquared() > 0.0001f)
    {
        component.viewDirection = viewDirection;
    }

    Vec3f walkDirection = Vec3f::Zero();

    if (movementInput.LengthSquared() > 0.0001f)
    {
        Vec3f forward = Vec3f { component.viewDirection.x, 0.0f, component.viewDirection.z }.Normalize();
        Vec3f right = Vec3f::UnitY().Cross(forward).Normalize();

        Vec3f wishDirection = forward * movementInput.y + right * movementInput.x;

        if (wishDirection.LengthSquared() > 1.0f)
        {
            wishDirection.Normalize();
        }

        const float wishSpeed = bool(move.sprintHeld)
            ? MathUtil::Max(component.sprintSpeed, 0.0f)
            : MathUtil::Max(component.moveSpeed, 0.0f);

        walkDirection = wishDirection * wishSpeed;
    }

    physicsWorld->ApplyCharacterJump(component.physicsHandle, bool(move.jumpRequested), bool(move.jumpHeld));

    physicsWorld->SetCharacterWalkDirection(component.physicsHandle, walkDirection);

    physicsWorld->StepCharacterController(component.physicsHandle, move.deltaTime);
    physicsWorld->GetCharacterState(component.physicsHandle, component.translation, component.isOnGround);

    outResultTranslation = component.translation + Vec3f(0.0f, GetCapsuleHeightOffset(component), 0.0f);

    entity->SetWorldTranslation(outResultTranslation, TransformChangeType::Simulation);
}

} // namespace SceneHelpers
} // namespace Hyperion
