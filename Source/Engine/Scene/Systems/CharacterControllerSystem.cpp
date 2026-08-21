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

void CharacterControllerInputHandler::Update()
{
    // Update movement, jump:
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

    m_movementInput = Vec2f(MathUtil::Clamp(strafe, -1.0f, 1.0f), MathUtil::Clamp(forward, -1.0f, 1.0f));
    m_isJumpRequested = IsKeyDown(KeyCode::KEY_SPACE);
}

bool CharacterControllerInputHandler::OnKeyDown(const KeyboardEvent& evt)
{
    InputHandlerBase::OnKeyDown(evt);

    switch (evt.keyCode)
    {
    case KeyCode::KEY_W:
        m_forward = 1.0f;

        Update();

        return true;
    case KeyCode::KEY_S:
        m_forward = -1.0f;

        Update();

        return true;
    case KeyCode::KEY_A:
        m_strafe = -1.0f;

        Update();

        return true;
    case KeyCode::KEY_D:
        m_strafe = 1.0f;

        Update();

        return true;
    case KeyCode::KEY_SPACE:
        // jump

        Update();

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

        Update();

        return true;
    case KeyCode::KEY_S:
        if (m_forward < 0.0f)
            m_forward = 0.0f;

        Update();

        return true;
    case KeyCode::KEY_A:
        if (m_strafe < 0.0f)
            m_strafe = 0.0f;

        Update();

        return true;
    case KeyCode::KEY_D:
        if (m_strafe > 0.0f)
            m_strafe = 0.0f;

        Update();

        return true;
    case KeyCode::KEY_SPACE:
        // jump

        Update();

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

static bool CanControlPlayerEntity(Entity* entity)
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
    {
        return;
    }

    if (!EngineGlobals::HasAuthority())
    {
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
            // Check needs initialization
            if (!component.inputHandler)
            {
                if (!CanControlPlayerEntity(entity))
                {
                    continue;
                }

                component.inputHandler = MakeHandle<CharacterControllerInputHandler>();
                InitObject(component.inputHandler);

                if (Game* game = GetWorld()->GetGame())
                {
                    game->RegisterInputHandler(component.inputHandler);
                }
            }
            
            CharacterControllerInputHandler* inputHandler = StaticCast<CharacterControllerInputHandler>(component.inputHandler.Get());
            inputHandler->SetDeltaTime(GetWorld()->GetGameState().deltaTime);

            float heightOffset = 0.0f;
            if (CapsulePhysicsShape* capsuleShape = DynamicCast<CapsulePhysicsShape>(component.shape.Get()))
            {
                // amount to adjust the the final offset by after applying capsule height
                // otherwise the node will sit directly on top of the capsule,
                // when it should be contained within the capsule
                static constexpr float CapsuleHeightOffset = 0.1f;

                heightOffset = capsuleShape->GetHeight() - CapsuleHeightOffset;
            }
            
            Vec2f movementInput;
            bool jumpPressed = false;

            if (hasAuthority)
            {
                if (!component.physicsHandle)
                {
                    HYP_LOG_ONCE(Scene, Warning, "physicsHandle is null for Entity {}'s character controller.", entity->GetName());
                    continue;
                }

                movementInput = inputHandler->GetMovementInput();
                jumpPressed = inputHandler->IsJumpPressed();
            }
            else
            {
                // Send request
                SendPlayerInputRequest(inputHandler->GetMovementInput(), int8(inputHandler->IsJumpPressed()));

                continue;
            }

            TransformComponent& transformComponent = entity->GetComponent<TransformComponent>();
            const Vec3f facingDirection = transformComponent.rotation.RotateVector(Vec3f(0.0f, 0.0f, 1.0f));
            const Vec3f horizontalFacing(facingDirection.x, 0.0f, facingDirection.z);

            if (horizontalFacing.LengthSquared() > 0.0001f)
            {
                component.viewDirection = facingDirection;
            }
            
            Vec3f walkDirection;

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
