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
    if (EngineGlobals::HasAuthority())
    {
        return true;
    }

    const PlayerComponent* playerComponent = entity->TryGetComponent<PlayerComponent>();

    return playerComponent != nullptr && playerComponent->IsLocalPlayer();
}

static bool IsLocallyControlledPlayerEntity(Entity* entity)
{
    if (EngineGlobals::IsHeadless())
    {
        // dedicated server
        return false;
    }

    if (g_gameClient == nullptr || !g_gameClient->IsConnected())
    {
        // single-player
        return true;
    }

    const PlayerComponent* playerComponent = entity->TryGetComponent<PlayerComponent>();

    return playerComponent == nullptr || playerComponent->IsLocalPlayer();
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

static float GetCapsuleHeightOffset(const CharacterControllerComponent& component)
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

void ApplyCharacterMove(Entity* entity, CharacterControllerComponent& component, const PlayerMove& move, Vec3f& outResultTranslation)
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

/// Handles a server move acknowledgement: drops acked moves and, if our prediction for the
/// acked move deviates too far from the authoritative result, rewinds to the server state
/// and replays all still-unacknowledged moves on top of it.
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

    const float correctionThreshold = EngineGlobals::GetCorrectionThreshold();
    const bool needsCorrection = predictedResult.HasValue()
        || (*predictedResult - ack.authoritativeTranslation).LengthSquared() > correctionThreshold * correctionThreshold;

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
    const float heightOffset = GetCapsuleHeightOffset(component);
    const Vec3f authoritativeCapsuleCenter = ack.authoritativeTranslation - Vec3f(0.0f, heightOffset, 0.0f);

    physicsWorld->SetCharacterTranslation(component.physicsHandle, authoritativeCapsuleCenter);

    // Replay all unacknowledged moves on top of the corrected state
    for (ClientPredictionState::BufferedMove& buffered : state.unacknowledgedMoves)
    {
        Vec3f resultTranslation = Vec3f(0.0f);

        ApplyCharacterMove(entity, component, buffered.move, resultTranslation);

        buffered.resultTranslation = resultTranslation;
    }

    if (predictedResult == nullptr && state.unacknowledgedMoves.Empty())
    {
        // No local state to compare or replay against -- snap directly to the server state.
        component.translation = authoritativeCapsuleCenter;
        component.isOnGround = false;

        entity->SetWorldTranslation(ack.authoritativeTranslation, TransformChangeType::Simulation);
    }

    // Smooth out the visual pop of the rewind/replay over a short time window.
    const Vec3f replayedTranslation = transformComponent.translation;
    const Vec3f correctionOffset = preRewindTranslation - replayedTranslation;
    const float smoothingTime = EngineGlobals::GetCorrectionSmoothingTime();

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

    TransformComponent& transformComponent = entity->GetComponent<TransformComponent>();
    const Vec3f facingDirection = transformComponent.rotation.RotateVector(Vec3f(0.0f, 0.0f, 1.0f));

    // Predict this tick's move locally ("act first")
    PlayerMove move;
    move.moveId = state.nextMoveId++;
    move.deltaTime = entity->GetWorld()->GetGameState().deltaTime;
    move.movementInput = inputHandler->GetMovementInput();
    move.jumpRequested = int8(inputHandler->IsJumpPressed());
    move.viewDirection = facingDirection;

    Vec3f resultTranslation = Vec3f(0.0f);

    ApplyCharacterMove(entity, component, move, resultTranslation);

    state.unacknowledgedMoves.PushBack(ClientPredictionState::BufferedMove { move, resultTranslation });

    // Cap the buffer so sustained ack loss can't grow it without bound
    while (state.unacknowledgedMoves.Size() > ClientPredictionState::MaxBufferedMoves)
    {
        state.unacknowledgedMoves.EraseAt(0);
    }

    // While a correction is being smoothed out, bias the rendered translation back
    // towards where it was before the rewind, decaying to zero.
    if (state.smoothingSecondsRemaining > 0.0f)
    {
        const float smoothingTime = MathUtil::Max(EngineGlobals::GetCorrectionSmoothingTime(), 0.0001f);

        state.smoothingSecondsRemaining = MathUtil::Max(0.0f, state.smoothingSecondsRemaining - delta);

        const float offsetScale = state.smoothingSecondsRemaining / smoothingTime;

        entity->SetWorldTranslation(
            resultTranslation + state.smoothingOffset * offsetScale,
            TransformChangeType::Simulation);
    }

    // Flush batched moves to the server at the configured send rate
    state.secondsSinceLastSend += delta;

    const float sendRate = MathUtil::Max(EngineGlobals::GetClientSendRate(), 1.0f);
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

            if (IsLocallyControlledPlayerEntity(entity))
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

                TransformComponent& transformComponent = entity->GetComponent<TransformComponent>();
                const Vec3f facingDirection = transformComponent.rotation.RotateVector(Vec3f(0.0f, 0.0f, 1.0f));

                PlayerMove move;
                move.moveId = 0;
                move.deltaTime = GetWorld()->GetGameState().deltaTime;
                move.movementInput = inputHandler->GetMovementInput();
                move.jumpRequested = int8(inputHandler->IsJumpPressed());
                move.viewDirection = facingDirection;

                Vec3f resultTranslation = Vec3f(0.0f);

                ApplyCharacterMove(entity, component, move, resultTranslation);
            }
            else if (IsLocallyControlledPlayerEntity(entity))
            {
                // Connected client: predict our own player locally and reconcile against the server.
                ProcessClientPrediction(entity, component, this, delta);
            }
        }
    }
}

#pragma endregion CharacterControllerSystem

} // namespace Hyperion
