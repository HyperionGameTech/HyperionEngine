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

#include <Scene/Camera/Camera.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Physics/PhysicsWorld.hpp>
#include <Physics/PhysicsShape.hpp>

#include <Input/Keyboard.hpp>
#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <System/AppContext.hpp>

#include <Framework/Game.hpp>
#include <Framework/EngineGlobals.hpp>
#include <Framework/Client/GameClient.hpp>
#include <Framework/Client/ClientReplicationManager.hpp>

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

    const bool jumpKeyDown = IsKeyDown(KeyCode::KEY_SPACE);

    if (jumpKeyDown && !m_wasJumpKeyDown)
    {
        m_isJumpRequested = true;
    }

    m_wasJumpKeyDown = jumpKeyDown;
    m_isJumpHeld = jumpKeyDown;

    m_isSprintHeld = IsKeyDown(KeyCode::KEY_LSHIFT) || IsKeyDown(KeyCode::KEY_RSHIFT);
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

bool CharacterControllerInputHandler::OnControllerAnalogMove(const ControllerAnalogData& data)
{
    InputHandlerBase::OnControllerAnalogMove(data);

    if (data.actionIndex == 0)
    {
        Update();

        return true;
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

static const Handle<Camera>& GetCameraChild(const Entity& entity)
{
    for (const Handle<Node>& child : entity.GetChildren())
    {
        if (const Handle<Camera>& camera = DynamicCast<Camera>(child); camera.IsValid())
        {
            return camera;
        }
    }

    return Handle<Camera>::Null();
}

static Vec3f GetPlayerViewDirection(const Entity& entity)
{
    const Handle<Camera>& cameraChild = GetCameraChild(entity);
    if (cameraChild.IsValid())
    {
#ifdef HYP_EDITOR
        const Vec3f& localTranslation = cameraChild->GetLocalTranslation();
        if (!MathUtil::ApproxEqual(Vec2f(localTranslation.x, localTranslation.z), Vec2f::Zero()))
        {
            HYP_LOG_ONCE(Scene, Warning, "Camera '{}' has x/z offset from parent node, character may appear to"
                                        " interact with the physical world in bizarre and twisted ways",
                                        cameraChild->GetName());
        }
#endif // HYP_EDITOR

        return cameraChild->GetDirection();
    }

    return entity.GetWorldRotation().RotateVector(Vec3f::UnitZ());
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
    config.groundAcceleration = component.groundAcceleration;
    config.airAcceleration = component.airAcceleration;
    config.friction = component.friction;
    config.stopSpeed = component.stopSpeed;
    config.jumpCutGravityMultiplier = component.jumpCutGravityMultiplier;
    config.apexGravityMultiplier = component.apexGravityMultiplier;
    config.fallGravityMultiplier = component.fallGravityMultiplier;
    config.coyoteTime = component.coyoteTime;
    config.jumpBufferTime = component.jumpBufferTime;
    config.shadowMaxSpeed = component.shadowMaxSpeed;
    config.shadowTeleportDistance = component.shadowTeleportDistance;
    config.pushMassLimit = component.pushMassLimit;
    config.minGroundSupportMass = component.minGroundSupportMass;

    entity->GetWorld()->GetPhysicsWorld()->AddCharacterController(config, component.physicsHandle);

    if (!component.physicsHandle)
    {
        HYP_LOG(Scene, Error, "Failed to add CharacterController to physics world (physicsHandle was null) - Entity = {}", entity->GetName());
    }
}

void CharacterControllerSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    m_predictionStates.Erase(entity);

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

static void SendPlayerMoves(ClientPredictionState& state)
{
    if (g_gameClient == nullptr)
    {
        return;
    }

    PlayerMove moves[MaxPlayerMovesPerRequest];
    uint32 numMoves = 0;

    for (const ClientPredictionState::BufferedMove& buffered : state.unacknowledgedMoves)
    {
        if (numMoves >= MaxPlayerMovesPerRequest)
        {
            break;
        }

        if (buffered.move.moveId <= state.lastSentMoveId)
        {
            continue;
        }

        moves[numMoves++] = buffered.move;
    }

    if (numMoves == 0)
    {
        return;
    }

    state.lastSentMoveId = moves[numMoves - 1].moveId;

    net::NetBuffer payload;
    MemoryByteWriter<net::NetAllocator, 1> writer(&payload);
    SerializePlayerMoves(writer, state.lastAckedMoveId, moves, numMoves);

    g_gameClient->GetThread()->GetScheduler().Enqueue(
        [payload = std::move(payload)]()
        {
            g_gameClient->GetNetClient().Send(
                net::NetMessageId::PlayerMovesRequest,
                net::NetChannelMode::UnreliableOrdered,
                net::NetStreamKey(0),
                payload.ToByteView());
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

static void ReconcileMoveAck(Entity* entity, CharacterControllerComponent& component, ClientPredictionState& state, const PlayerMoveAck& ack)
{
    if (ack.ackedMoveId <= state.lastAckedMoveId)
    {
        // Stale ack (already superseded by a newer one)
        return;
    }

    Optional<Vec3f> predictedResult;

    for (const ClientPredictionState::BufferedMove& buffered : state.unacknowledgedMoves)
    {
        if (buffered.move.moveId == ack.ackedMoveId)
        {
            predictedResult = buffered.resultTranslation;

            break;
        }
    }

    // Drop the acked move and everything older
    for (size_t i = 0; i < state.unacknowledgedMoves.Size();)
    {
        if (state.unacknowledgedMoves[i].move.moveId <= ack.ackedMoveId)
        {
            state.unacknowledgedMoves.EraseAt(i);

            continue;
        }

        ++i;
    }

    state.lastAckedMoveId = ack.ackedMoveId;

    const float correctionThreshold = NetGlobals::GetCorrectionThreshold();

    const bool needsCorrection = !predictedResult.HasValue()
        || (*predictedResult - ack.GetAuthTranslation()).LengthSquared() > correctionThreshold * correctionThreshold;

    if (!needsCorrection)
    {
        return;
    }

    if (!component.physicsHandle)
    {
        return;
    }

    PhysicsWorldBase* physicsWorld = entity->GetWorld()->GetPhysicsWorld();

    TransformComponent& transformComponent = entity->GetComponent<TransformComponent>();
    const Vec3f preRewindTranslation = transformComponent.translation;

    // Rewind the physics character to the server's authoritative state (entity translation
    // carries a capsule height offset; the controller itself is positioned at the capsule center).
    const float heightOffset = SceneHelpers::GetCapsuleHeightOffset(component);
    const Vec3f authoritativeCapsuleCenter = ack.GetAuthTranslation() - Vec3f(0.0f, heightOffset, 0.0f);

    physicsWorld->SetCharacterTranslation(component.physicsHandle, authoritativeCapsuleCenter);

    // Replay all unacknowledged moves on top of the corrected state
    for (ClientPredictionState::BufferedMove& buffered : state.unacknowledgedMoves)
    {
        Vec3f resultTranslation = Vec3f(0.0f);

        SceneHelpers::MoveCharacter(entity, component, buffered.move, resultTranslation);

        buffered.resultTranslation = resultTranslation;
    }

    if (!predictedResult.HasValue() && state.unacknowledgedMoves.Empty())
    {
        // No local state to compare or replay against -- snap directly to the server state.
        component.translation = authoritativeCapsuleCenter;
        component.isOnGround = false;

        entity->SetWorldTranslation(ack.GetAuthTranslation(), TransformChangeType::Simulation);
    }

    // Smooth out the visual pop of the rewind/replay over a short time window.
    const Vec3f replayedTranslation = transformComponent.translation;
    const Vec3f correctionOffset = preRewindTranslation - replayedTranslation;
    const float smoothingTime = NetGlobals::GetCorrectionSmoothingTime();

    if (smoothingTime > 0.0f && correctionOffset.LengthSquared() > MathUtil::epsilonF)
    {
        state.smoothingOffset = correctionOffset;
        state.smoothingSecondsRemaining = smoothingTime;
    }
    else
    {
        state.smoothingOffset = Vec3f(0.0f);
        state.smoothingSecondsRemaining = 0.0f;
    }
}

/// Local prediction of replicated props
static constexpr bool EnableLocalPrediction = false;

static void ProcessClientPredictionBodies(Entity* entity, CharacterControllerComponent& component, ClientPredictionState& state, float delta)
{
    PhysicsWorldBase* physicsWorld = entity->GetWorld()->GetPhysicsWorld();

    if (!physicsWorld || !component.physicsHandle)
    {
        return;
    }
    
    Array<Handle<RigidBody>, PhysicsAllocator> touched;
    
    const float releaseDelay = MathUtil::Max(component.pushPredictionReleaseDelay, 0.0f);

    if constexpr (EnableLocalPrediction)
    {
        physicsWorld->GetCharacterTouchedRigidBodies(component.physicsHandle, touched);

        for (const Handle<RigidBody>& rigidBody : touched)
        {
            if (!rigidBody)
            {
                continue;
            }

            bool tracked = false;

            for (ClientPredictionState::PredictedBodyState& predicted : state.locallyPredictedBodies)
            {
                if (predicted.rigidBody == rigidBody)
                {
                    predicted.timeSinceLastTouch = 0.0f;
                    tracked = true;

                    break;
                }
            }

            if (tracked)
            {
                continue;
            }

            // Only prediction for bodies that are still Kinematic
            if (!rigidBody->IsKinematic())
            {
                continue;
            }

            physicsWorld->SetRigidBodyKinematic(rigidBody, false);
            rigidBody->SetIsLocallyPredicted(true);

            state.locallyPredictedBodies.PushBack(ClientPredictionState::PredictedBodyState { rigidBody, 0.0f });
        }
    }

    for (size_t i = 0; i < state.locallyPredictedBodies.Size();)
    {
        ClientPredictionState::PredictedBodyState& predicted = state.locallyPredictedBodies[i];

        bool touchedThisFrame = false;

        for (const Handle<RigidBody>& rigidBody : touched)
        {
            if (rigidBody == predicted.rigidBody)
            {
                touchedThisFrame = true;

                break;
            }
        }

        if (touchedThisFrame)
        {
            predicted.timeSinceLastTouch = 0.0f;

            ++i;

            continue;
        }

        predicted.timeSinceLastTouch += delta;

        if (predicted.timeSinceLastTouch < releaseDelay)
        {
            ++i;

            continue;
        }

        // Release: hand the body back to Kinematic; MoveRigidBodyKinematic glides it smoothly onto
        // the latest authoritative sample. Guard against a despawned/invalidated body.
        if (predicted.rigidBody.IsValid())
        {
            predicted.rigidBody->SetIsLocallyPredicted(false);
            physicsWorld->SetRigidBodyKinematic(predicted.rigidBody, true);
        }

        state.locallyPredictedBodies.EraseAt(i);
    }
}

static void ProcessClientPrediction(Entity* entity, CharacterControllerComponent& component, CharacterControllerSystem* system, float delta)
{
    ClientPredictionState& state = system->GetPredictionState(entity);

    // The client creates its own physics character lazily here (OnEntityAdded only
    // creates one when running with authority, e.g. single-player or server).
    if (!component.physicsHandle)
    {
        TransformComponent& transformComponent = entity->GetComponent<TransformComponent>();

        component.translation = transformComponent.translation;

        CharacterControllerConfig config;
        config.shape = component.shape;
        config.startTranslation = component.translation;
        config.stepHeight = component.stepHeight;
        config.maxSlopeAngle = component.maxSlopeAngle;
        config.jumpSpeed = component.jumpSpeed;
        config.fallSpeed = component.fallSpeed;
        config.groundAcceleration = component.groundAcceleration;
        config.airAcceleration = component.airAcceleration;
        config.friction = component.friction;
        config.stopSpeed = component.stopSpeed;
        config.jumpCutGravityMultiplier = component.jumpCutGravityMultiplier;
        config.apexGravityMultiplier = component.apexGravityMultiplier;
        config.fallGravityMultiplier = component.fallGravityMultiplier;
        config.coyoteTime = component.coyoteTime;
        config.jumpBufferTime = component.jumpBufferTime;
        config.shadowMaxSpeed = component.shadowMaxSpeed;
        config.shadowTeleportDistance = component.shadowTeleportDistance;
        config.pushMassLimit = component.pushMassLimit;
        config.minGroundSupportMass = component.minGroundSupportMass;

        entity->GetWorld()->GetPhysicsWorld()->AddCharacterController(config, component.physicsHandle);

        if (!component.physicsHandle)
        {
            return;
        }
    }

    // Reconcile against any acks the server sent us since last tick
    if (g_gameClient != nullptr)
    {
        Array<PlayerMoveAck, SceneTempAllocator> acks;

        g_gameClient->GetReplicationManager().DrainPendingMoveAcks(acks);

        for (const PlayerMoveAck& ack : acks)
        {
            ReconcileMoveAck(entity, component, state, ack);
        }
    }

    CharacterControllerInputHandler* inputHandler = StaticCast<CharacterControllerInputHandler>(component.inputHandler.Get());

    const Vec3f viewDirection = GetPlayerViewDirection(*entity);

    // Predict this tick's move locally
    PlayerMove move {};
    move.moveId = state.nextMoveId++;
    move.deltaTime = entity->GetWorld()->GetGameState().deltaTime;
    move.movementInput[0] = inputHandler->GetMovementInput().x;
    move.movementInput[1] = inputHandler->GetMovementInput().y;
    move.viewDirection[0] = viewDirection.x;
    move.viewDirection[1] = viewDirection.y;
    move.viewDirection[2] = viewDirection.z;
    move.jumpRequested = uint8(inputHandler->IsJumpPressed());
    move.sprintHeld = uint8(inputHandler->IsSprintHeld());
    move.jumpHeld = uint8(inputHandler->IsJumpHeld());

    inputHandler->ConsumeJumpRequest();

    Vec3f resultTranslation = Vec3f::Zero();

    SceneHelpers::MoveCharacter(entity, component, move, resultTranslation);

    ProcessClientPredictionBodies(entity, component, state, delta);

    state.unacknowledgedMoves.PushBack(ClientPredictionState::BufferedMove { move, resultTranslation });

    // cap it
    CapArray(state.unacknowledgedMoves, ClientPredictionState::MaxBufferedMoves);

    // bias the rendered translation back towards where it was before the rewind
    if (state.smoothingSecondsRemaining > 0.0f)
    {
        const float smoothingTime = MathUtil::Max(NetGlobals::GetCorrectionSmoothingTime(), 0.0001f);

        state.smoothingSecondsRemaining = MathUtil::Max(0.0f, state.smoothingSecondsRemaining - delta);

        const float offsetScale = state.smoothingSecondsRemaining / smoothingTime;

        entity->SetWorldTranslation(
            resultTranslation + state.smoothingOffset * offsetScale,
            TransformChangeType::Simulation);
    }

    // Flush batched moves
    state.secondsSinceLastSend += delta;

    const float sendRate = MathUtil::Max(NetGlobals::GetClientSendRate(), 1.0f);
    const float sendInterval = 1.0f / sendRate;

    if (state.secondsSinceLastSend >= sendInterval && state.lastSentMoveId < state.nextMoveId - 1)
    {
        SendPlayerMoves(state);

        state.secondsSinceLastSend = 0.0f;
    }
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
            const bool isLocalPlayerEntity = SceneHelpers::IsLocalPlayerEntity(*entity);

            // Check needs initialization
            if (!component.inputHandler)
            {
                if (!isLocalPlayerEntity && !EngineGlobals::HasAuthority())
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

            if (isLocalPlayerEntity && !EngineGlobals::IsHeadless())
            {
                inputHandler->Update();
            }

            if (hasAuthority)
            {
                // Remote-controlled players are simulated from their move queues by ReplicationSystem.
                const PlayerComponent* playerComponent = entity->TryGetComponent<PlayerComponent>();

                if (playerComponent != nullptr && !playerComponent->IsLocalPlayer())
                {
                    continue;
                }

                if (!component.physicsHandle)
                {
                    HYP_LOG_ONCE(Scene, Warning, "physicsHandle is null for Entity {}'s character controller.", entity->GetName());
                    continue;
                }

                const Vec3f viewDirection = GetPlayerViewDirection(*entity);

                PlayerMove move {};
                move.moveId = 0;
                move.deltaTime = GetWorld()->GetGameState().deltaTime;
                move.movementInput[0] = inputHandler->GetMovementInput().x;
                move.movementInput[1] = inputHandler->GetMovementInput().y;
                move.viewDirection[0] = viewDirection.x;
                move.viewDirection[1] = viewDirection.y;
                move.viewDirection[2] = viewDirection.z;
                move.jumpRequested = uint8(inputHandler->IsJumpPressed());
                move.sprintHeld = uint8(inputHandler->IsSprintHeld());
                move.jumpHeld = uint8(inputHandler->IsJumpHeld());

                inputHandler->ConsumeJumpRequest();

                Vec3f resultTranslation;
                SceneHelpers::MoveCharacter(entity, component, move, resultTranslation);
            }
            else if (isLocalPlayerEntity)
            {
                // Connected client: predict our own player locally and reconcile against the server.
                ProcessClientPrediction(entity, component, this, delta);
            }
        }
    }
}

#pragma endregion CharacterControllerSystem

} // namespace Hyperion
