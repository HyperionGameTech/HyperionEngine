/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Systems/CharacterControllerSystem.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Physics/PhysicsWorld.hpp>
#include <Physics/PhysicsShape.hpp>

#include <Input/Keyboard.hpp>
#include <Input/InputManager.hpp>

#include <System/AppContext.hpp>

#include <Framework/Game.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/Client/GameClient.hpp>

#include <Net/NetClient.hpp>
#include <Net/NetMessage.hpp>
#include <Net/NetMemory.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Task.hpp>

#include <Core/Utilities/Traits.hpp>

#include <CharacterControllerSystem.generated.inl>

namespace Hyperion {

#pragma region CharacterControllerInputHandler

Vec2f CharacterControllerInputHandler::GetMovementInput() const
{
    float forward = 0.0f;
    float strafe = 0.0f;

    if (IsKeyDown(KeyCode::KEY_W))
    {
        forward += 1.0f;
    }

    if (IsKeyDown(KeyCode::KEY_S))
    {
        forward -= 1.0f;
    }

    if (IsKeyDown(KeyCode::KEY_A))
    {
        strafe -= 1.0f;
    }

    if (IsKeyDown(KeyCode::KEY_D))
    {
        strafe += 1.0f;
    }

    const Vec2f& touchDelta = GetTouchMovementDelta();
    strafe += touchDelta.x;
    forward -= touchDelta.y;

    const Vec2f& controllerMove = GetControllerMoveDelta();
    strafe += controllerMove.x;
    forward += controllerMove.y;

    return Vec2f(MathUtil::Clamp(strafe, -1.0f, 1.0f), MathUtil::Clamp(forward, -1.0f, 1.0f));
}

bool CharacterControllerInputHandler::IsJumpPressed() const
{
    return IsKeyDown(KeyCode::KEY_SPACE);
}

bool CharacterControllerInputHandler::OnKeyDown(const KeyboardEvent& evt)
{
    InputHandlerBase::OnKeyDown(evt);

    switch (evt.keyCode)
    {
    case KeyCode::KEY_W:
        m_forward = 1.0f;
        return true;
    case KeyCode::KEY_S:
        m_forward = -1.0f;
        return true;
    case KeyCode::KEY_A:
        m_strafe = -1.0f;
        return true;
    case KeyCode::KEY_D:
        m_strafe = 1.0f;
        return true;
    case KeyCode::KEY_SPACE:
        m_jump = true;
        return true;
    default:
        break;
    }

    return false;
}

bool CharacterControllerInputHandler::OnKeyUp(const KeyboardEvent& evt)
{
    InputHandlerBase::OnKeyUp(evt);

    switch (evt.keyCode)
    {
    case KeyCode::KEY_W:
        if (m_forward > 0.0f)
            m_forward = 0.0f;
        return true;
    case KeyCode::KEY_S:
        if (m_forward < 0.0f)
            m_forward = 0.0f;
        return true;
    case KeyCode::KEY_A:
        if (m_strafe < 0.0f)
            m_strafe = 0.0f;
        return true;
    case KeyCode::KEY_D:
        if (m_strafe > 0.0f)
            m_strafe = 0.0f;
        return true;
    case KeyCode::KEY_SPACE:
        m_jump = false;
        return true;
    default:
        break;
    }

    return false;
}

#pragma endregion CharacterControllerInputHandler

#pragma region CharacterControllerSystem

bool CharacterControllerSystem::ShouldProcessScene(Scene* scene) const
{
    static constexpr EnumFlags<SceneFlags> ExpectedFlags = SceneFlags::FOREGROUND;

    return (scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED | ExpectedFlags)) == ExpectedFlags;
}

// Whether this entity's movement should be driven from locally-captured keyboard/controller input:
//  - HasAuthority() with no owning connection (single-player/editor) drives directly from local input, unchanged.
//  - A client drives its OWN player's entity from local input, to be forwarded to the server below.
//  - Everything else (a dedicated server's per-connection clones, or another player's clone visible
//    on this client) must NOT read local input -- the former gets its input over the network, the
//    latter is driven entirely by replication.
// NOTE: deliberately evaluated in Process() rather than OnEntityAdded() -- for a freshly-cloned
// per-connection player entity (see PlayerSystem::TrySpawnClone), PlayerComponent::connectionId is
// still Invalid at the moment OnEntityAdded fires during Entity::Clone()'s component deserialization;
// it's only set afterward, once TrySpawnClone gets the cloned handle back. By Process() time it's settled.

/// @TODO Verify this and shore it up
static bool NeedsLocalInputHandler(Entity* entity)
{
    const PlayerComponent* playerComponent = entity->TryGetComponent<PlayerComponent>();
    const bool hasOwner = playerComponent != nullptr && playerComponent->connectionId != Invalid<net::NetConnectionId>;

    if (EngineGlobals::HasAuthority())
    {
        return !hasOwner;
    }

    return hasOwner
        && g_gameClient != nullptr
        && g_gameClient->GetNetClient().GetConnectionId() == playerComponent->connectionId;
}

void CharacterControllerSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    if (!ShouldProcessScene(entity->GetScene()))
        return;

    if (!EngineGlobals::HasAuthority())
    {
        // Non-authoritative processes never locally simulate character physics -- the server owns
        // this and the resulting transform reaches us via replication (ComponentSnapshot).
        return;
    }

    CharacterControllerComponent& component = entity->GetComponent<CharacterControllerComponent>();

    TransformComponent& transformComponent = entity->GetComponent<TransformComponent>();
    component.translation = transformComponent.translation;

    CharacterControllerConfig config;
    config.shape = component.shape;
    config.startTranslation = component.translation;
    config.stepHeight = component.stepHeight;
    config.maxSlopeAngle = component.maxSlopeAngle;
    config.jumpSpeed = component.jumpSpeed;
    config.fallSpeed = component.fallSpeed;

    entity->GetWorld()->GetPhysicsWorld()->AddCharacterController(config, component.physicsHandle);

    if (!component.physicsHandle)
    {
        HYP_LOG(Scene, Error, "Failed to add CharacterController to physics world (physicsHandle was null) - Entity = {}", entity->GetName());
    }
}

void CharacterControllerSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    if (!ShouldProcessScene(entity->GetScene()))
    {
        return;
    }

    CharacterControllerComponent& component = entity->GetComponent<CharacterControllerComponent>();

    if (component.inputHandler)
    {
        if (Game* game = GetWorld()->GetGame())
        {
            game->UnregisterInputHandler(component.inputHandler);
        }
    }

    if (component.physicsHandle)
    {
        entity->GetWorld()->GetPhysicsWorld()->RemoveCharacterController(component.physicsHandle);
    }
}

static void SendPlayerInputRequest(const Vec2f& movementInput, int8 jumpRequested)
{
    if (g_gameClient == nullptr)
    {
        return;
    }

    net::NetBuffer payload;
    MemoryByteWriter<net::NetAllocator, 1> writer(&payload);
    writer.Write(movementInput);
    writer.Write(jumpRequested);

    g_gameClient->GetThread()->GetScheduler().Enqueue(
        [payload = std::move(payload)]()
        {
            g_gameClient->GetNetClient().Send(
                net::NetMessageId::PlayerInputRequest,
                net::NetChannelMode::UnreliableOrdered,
                net::NetStreamKey(0),
                payload.ToByteView());
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

void CharacterControllerSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    HYP_SCOPE;

    if (!GetWorld()->GetGameState().IsSimulating())
    {
        return;
    }

    const bool hasAuthority = EngineGlobals::HasAuthority();

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, component] : scene->GetEntityManager()->GetEntitySet<CharacterControllerComponent>().GetScopedView(GetComponentInfos()))
        {
            HYP_LOG(Scene, Info, "Has entity {} with character controller, has player component? {} (id = {}) has player tag? {}",
                    entity->GetName(),
                    entity->HasComponent<PlayerComponent>(),
                    entity->HasComponent<PlayerComponent>() ? entity->GetComponent<PlayerComponent>().connectionId : Invalid<net::NetConnectionId>,
                    entity->HasTag<EntityTag::Player>());

            if (!component.inputHandler && NeedsLocalInputHandler(entity))
            {
                component.inputHandler = MakeHandle<CharacterControllerInputHandler>();
                InitObject(component.inputHandler);

                if (Game* game = GetWorld()->GetGame())
                {
                    game->RegisterInputHandler(component.inputHandler);

                    HYP_LOG(Scene, Info, "Registered local CharacterControllerInputHandler for Entity '{}'", entity->GetName());
                }
                else
                {
                    HYP_LOG(Scene, Warning, "NeedsLocalInputHandler() was true for Entity '{}' but GetWorld()->GetGame() returned null -- handler created but not registered", entity->GetName());
                }
            }

            if (!hasAuthority)
            {
                // Non-authoritative: only entities we locally control (i.e. that got an
                // inputHandler above) need anything done here -- forward their input to the
                // server. Everything else (other players' clones) is driven purely by
                // replication and must not be touched here.
                if (component.inputHandler)
                {
                    CharacterControllerInputHandler* inputHandler = StaticCast<CharacterControllerInputHandler>(component.inputHandler.Get());
                    inputHandler->SetDeltaTime(GetWorld()->GetGameState().deltaTime);

                    SendPlayerInputRequest(inputHandler->GetMovementInput(), int8(inputHandler->IsJumpPressed()));
                }

                continue;
            }

            if (!component.physicsHandle)
            {
                HYP_LOG_ONCE(Scene, Warning, "physicsHandle is null for Entity {}'s character controller.", entity->GetName());
                continue;
            }

            Vec3f walkDirection;

            float heightOffset = 0.0f;

            if (CapsulePhysicsShape* capsuleShape = DynamicCast<CapsulePhysicsShape>(component.shape.Get()))
            {
                // amount to adjust the the final offset by after applying capsule height
                // otherwise the node will sit directly on top of the capsule,
                // when it should be contained within the capsule
                static constexpr float CapsuleHeightOffset = 0.1f;

                heightOffset = capsuleShape->GetHeight() - CapsuleHeightOffset;
            }

            // Authoritative: drive from local keyboard input if we have an inputHandler (single-
            // player/editor, no owning connection); otherwise this is a dedicated server driving a
            // connected client's entity, so use the latest input received over the network instead.
            Vec2f movementInput;
            bool jumpPressed = false;

            if (component.inputHandler)
            {
                CharacterControllerInputHandler* inputHandler = StaticCast<CharacterControllerInputHandler>(component.inputHandler.Get());
                inputHandler->SetDeltaTime(GetWorld()->GetGameState().deltaTime);

                movementInput = inputHandler->GetMovementInput();
                jumpPressed = inputHandler->IsJumpPressed();
            }
            else
            {
                movementInput = component.networkMovementInput;
                jumpPressed = component.networkJumpRequested;
            }

            TransformComponent& transformComponent = entity->GetComponent<TransformComponent>();
            const Vec3f facingDirection = transformComponent.rotation.RotateVector(Vec3f(0.0f, 0.0f, 1.0f));
            const Vec3f horizontalFacing(facingDirection.x, 0.0f, facingDirection.z);

            if (horizontalFacing.LengthSquared() > 0.0001f)
            {
                component.viewDirection = facingDirection;
            }

            if (movementInput.LengthSquared() > 0.0001f)
            {
                Vec3f forward = Vec3f(component.viewDirection.x, 0.0f, component.viewDirection.z).Normalize();
                Vec3f right = Vec3f(0.0f, 1.0f, 0.0f).Cross(forward).Normalize();

                walkDirection = (forward * movementInput.y + right * movementInput.x) * component.moveSpeed;
            }

            if (jumpPressed)
            {
                entity->GetWorld()->GetPhysicsWorld()->ApplyCharacterJump(component.physicsHandle);
            }

            entity->GetWorld()->GetPhysicsWorld()->SetCharacterWalkDirection(component.physicsHandle, walkDirection);
            entity->GetWorld()->GetPhysicsWorld()->GetCharacterState(component.physicsHandle, component.translation, component.isOnGround);

            entity->SetWorldTranslation(
                component.translation + Vec3f(0.0f, heightOffset, 0.0f),
                TransformChangeType::Simulation);
        }
    }
}

#pragma endregion CharacterControllerSystem

} // namespace Hyperion
