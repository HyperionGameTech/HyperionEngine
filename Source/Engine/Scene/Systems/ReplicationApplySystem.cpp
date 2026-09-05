/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Systems/ReplicationApplySystem.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Scene/Components/ReplicationStateComponent.hpp>
#include <Scene/Components/PlayerComponent.hpp>
#include <Scene/Components/CharacterControllerComponent.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>

#include <Scene/Systems/PhysicsSystem.hpp>

#include <Physics/PhysicsWorld.hpp>

#include <Scene/Camera/Camera.hpp>
#include <Scene/Camera/FirstPersonCamera.hpp>
#include <Scene/Camera/Streaming/CameraStreamingVolume.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/GameState.hpp>

#include <Framework/Client/GameClient.hpp>
#include <Framework/Client/ClientReplicationManager.hpp>

#include <Streaming/StreamingManager.hpp>

#include <Net/NetClient.hpp>
#include <Net/NetMemory.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/GlobalContext.hpp>
#include <Core/Utilities/Time.hpp>
#include <Core/Utilities/Traits.hpp>

#include <Core/Util.hpp>

#include <Core/Logging/Logger.hpp>

#include <ReplicationApplySystem.generated.inl>

namespace Hyperion {

struct ReplicationApplyContext {};

void ReplicationApplySystem::OnAddedToWorld(World* world)
{
    SystemBase::OnAddedToWorld(world);

    m_delegateHandlers.Add(
        NAME("OnSceneAdded"),
        World::OnSceneAdded.Bind(
            this,
            [this, world](World* eventWorld, const Handle<Scene>&)
            {
                if (eventWorld != world)
                {
                    return;
                }

                TryResolvePendingSpawns(world->GetScenes().ToSpan());
            }));
}

void ReplicationApplySystem::OnRemovedFromWorld(World* world)
{
    m_delegateHandlers.Remove("OnSceneAdded"_sh);

    SystemBase::OnRemovedFromWorld(world);
}

void ReplicationApplySystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    if (scenes.Size() == 0 || g_gameClient == nullptr)
    {
        return;
    }

    if (!GetWorld()->GetGameState().IsSimulating())
    {
        return;
    }

    GlobalContextScope replicationApplyScope { ReplicationApplyContext {} };

    for (size_t i = 0; i < m_pendingSpawns.Size();)
    {
        PendingSpawn& pending = m_pendingSpawns[i];

        pending.secondsWaited += delta;

        if (pending.secondsWaited >= PendingSpawnTimeoutSeconds)
        {
            HYP_LOG(Replication, Warning,
                "Giving up on EntitySpawn for (netId={}, name={}): scene '{}' never loaded within {} seconds",
                uint32(pending.spawnOp.netId),
                pending.spawnOp.name,
                pending.spawnOp.sceneName,
                PendingSpawnTimeoutSeconds);

            m_pendingSpawns.EraseAt(i);

            continue;
        }

        ++i;
    }

    Array<ReplicationOpBase*, SceneTempAllocator> ops;
    g_gameClient->GetReplicationManager().DrainPendingOps(ops);

    for (const ReplicationOpBase* opPtr : ops)
    {
        switch (opPtr->type)
        {
        case ReplicationOpType::Spawn:
        {
            const ReplicationOp<ReplicationOpType::Spawn>& op = static_cast<const ReplicationOp<ReplicationOpType::Spawn>&>(*opPtr);

            if (auto existingIt = m_netIdToEntity.Find(op.netId); existingIt != m_netIdToEntity.End())
            {
                if (!SceneHelpers::IsLocalPlayerEntity(*existingIt->second))
                {
                    existingIt->second->SetLocalTransform(op.transform);
                }

                break;
            }

            if (Entity* resolvedEntity = TryResolveSpawn(op, scenes))
            {
                m_netIdToEntity[op.netId] = MakeStrongRef(resolvedEntity);

                // try to resolve, we might have unblocked a child entity waiting on this.
                TryResolvePendingSpawns(scenes);
            }
            else
            {
                m_pendingSpawns.PushBack(PendingSpawn { op });
            }

            break;
        }
        case ReplicationOpType::Despawn:
        {
            const ReplicationOp<ReplicationOpType::Despawn>& op = static_cast<const ReplicationOp<ReplicationOpType::Despawn>&>(*opPtr);

            m_interpolationStates.Erase(op.netId);

            auto it = m_netIdToEntity.Find(op.netId);

            if (it == m_netIdToEntity.End())
            {
                break;
            }

            // Remove from parent
            it->second->Remove();

            m_netIdToEntity.Erase(it);

            break;
        }
        case ReplicationOpType::Snapshot:
        {
            const ReplicationOp<ReplicationOpType::Snapshot>& op = static_cast<const ReplicationOp<ReplicationOpType::Snapshot>&>(*opPtr);

            // Server clock estimate: offset between the local clock and the server's sim clock.
            // Measured from the arrival time vs. the server timestamp the snapshot was captured at.
            const int64 localNowMs = int64(Time::Now().ToMilliseconds());
            const int64 serverTimeMs = int64(op.serverTimeMs.ToMilliseconds());

            // Track the inter-sample cadence in server-time space (ignores intra-burst duplicates
            // where multiple entities stamped with the same server tick arrive together).
            if (m_hasSeenServerTime)
            {
                const int64 intervalMs = int64(op.serverTimeMs.ToMilliseconds()) - int64(m_lastSeenServerTimeMs.ToMilliseconds());

                if (intervalMs > 0 && intervalMs <= 1000)
                {
                    const double alpha = 0.1;
                    m_estimatedSampleIntervalMs = m_estimatedSampleIntervalMs * (1.0 - alpha) + double(intervalMs) * alpha;
                }
            }

            m_lastSeenServerTimeMs = op.serverTimeMs;
            m_hasSeenServerTime = true;

            if (!m_hasClockEstimate)
            {
                m_clockOffsetMs = double(localNowMs - serverTimeMs);
                m_hasClockEstimate = true;
            }
            else
            {
                const double rawOffsetMs = double(localNowMs - serverTimeMs);

                // Discard outliers (e.g. a stalled/rerouted packet) rather than yanking the clock.
                if (MathUtil::Abs(rawOffsetMs - m_clockOffsetMs) <= 250.0)
                {
                    // Ease the estimate toward the raw offset so network jitter doesn't modulate
                    // playback speed (Source cl_smooth-style clock correction).
                    const double alpha = 0.05;
                    m_clockOffsetMs += (rawOffsetMs - m_clockOffsetMs) * alpha;
                }
            }

            auto it = m_netIdToEntity.Find(op.netId);

            if (it == m_netIdToEntity.End())
            {
                break;
            }

            // The client's player entity is client-predicted
            if (SceneHelpers::IsLocalPlayerEntity(*it->second))
            {
                break;
            }

            InterpolationState& interpolation = m_interpolationStates[op.netId];

            // Teleport detection: if the new sample is implausibly far from the previous newest
            // sample (respawn, shove, scene reset), clear history and snap instead of gliding.
            if (interpolation.samples.Any())
            {
                const InterpolationState::Sample& newest = interpolation.samples.Back();

                const float intervalSec = float(MathUtil::Max(m_estimatedSampleIntervalMs, 0.0)) * 0.001f;
                const float plausibleDistance = MathUtil::Max(2.0f, newest.velocity.Length() * intervalSec * 4.0f);

                if ((op.transform.GetTranslation() - newest.transform.GetTranslation()).LengthSquared() > plausibleDistance * plausibleDistance)
                {
                    interpolation.samples.Clear();
                    interpolation.snapNextColliderSync = true;
                }
            }

            InterpolationState::Sample& sample = interpolation.samples.EmplaceBack();
            sample.receiveTimeMs = op.receiveTimeMs;
            sample.serverTimeMs = op.serverTimeMs;
            sample.transform = op.transform;
            sample.velocity = op.velocity;
            sample.angularVelocity = op.angularVelocity;
            sample.isSleeping = bool(op.isSleeping);

            if (op.isSleeping)
            {
                interpolation.velocityEstimate = Vec3f(0.0f);
                interpolation.angularVelocityEstimate = Vec3f(0.0f);
            }
            else
            {
                interpolation.velocityEstimate = interpolation.velocityEstimate + (op.velocity - interpolation.velocityEstimate) * 0.5f;
                interpolation.angularVelocityEstimate = interpolation.angularVelocityEstimate + (op.angularVelocity - interpolation.angularVelocityEstimate) * 0.5f;
            }

            // cap it
            CapArray(interpolation.samples, InterpolationState::MaxSamples);

            break;
        }
        }
    }

    UpdateInterpolatedEntities(delta);
    UpdateStreamingVolume(scenes);
}

void ReplicationApplySystem::UpdateInterpolatedEntities(float delta)
{
    if (m_interpolationStates.Empty())
    {
        return;
    }

    const double nowMs = double(Time::Now().ToMilliseconds());
    const double spaceNowMs = m_hasClockEstimate ? (nowMs - m_clockOffsetMs) : nowMs;
    const double interpDelayMs = MathUtil::Max(
        double(NetGlobals::GetInterpolationDelay()) * 1000.0,
        double(NetGlobals::GetInterpRatio()) * m_estimatedSampleIntervalMs);

    const double renderTimeMs = spaceNowMs - interpDelayMs;

    for (auto it = m_interpolationStates.Begin(); it != m_interpolationStates.End();)
    {
        auto entityIt = m_netIdToEntity.Find(it->first);

        if (entityIt == m_netIdToEntity.End() || !entityIt->second.IsValid())
        {
            it = m_interpolationStates.Erase(it);

            continue;
        }

        if (SceneHelpers::IsLocalPlayerEntity(*entityIt->second))
        {
            it = m_interpolationStates.Erase(it);

            continue;
        }

        Array<InterpolationState::Sample, SceneAllocator>& samples = it->second.samples;

        const auto sample_time = [this](const InterpolationState::Sample& sample) -> double
        {
            return m_hasClockEstimate ? double(sample.serverTimeMs.ToMilliseconds()) : double(sample.receiveTimeMs.ToMilliseconds());
        };

        //-- Sample cull

        size_t numToChomp = 0;

        while (samples.Size() - numToChomp > 1 && sample_time(samples[numToChomp + 1]) <= renderTimeMs)
        {
            ++numToChomp;
        }

        if (numToChomp > 0)
        {
            samples.Erase(samples.Begin(), samples.Begin() + numToChomp);
        }
        //-- 

        Entity* entity = entityIt->second.Get();

        // While a Kinematic body is locally flipped to Dynamic for push prediction, its visible
        // transform is owned by the local simulation (World::SyncPhysicsToEntities), not this
        // replication path. Keep accumulating samples (so there's a smooth glide-back once the
        // prediction ends) but do not write the entity transform here.
        if (RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>())
        {
            if (rigidBodyComponent->rigidBody.IsValid() && rigidBodyComponent->rigidBody->IsLocallyPredicted())
            {
                ++it;

                continue;
            }
        }

        Transform targetTransform;

        if (!NetGlobals::GetInterpolationEnabled())
        {
            // Debug vis
            targetTransform = samples.Back().transform;
        }
        else if (samples.Size() == 1)
        {
            targetTransform = samples[0].transform;
        }
        else
        {
            const InterpolationState::Sample& from = samples[0];
            const InterpolationState::Sample& to = samples[1];

            const double spanMs = sample_time(to) - sample_time(from);

            if (spanMs <= 0.0)
            {
                targetTransform = to.transform;
            }
            else
            {
                const float alpha = float(MathUtil::Clamp((renderTimeMs - sample_time(from)) / spanMs, 0.0, 1.0));

                // Hermite interpolation using the per-sample velocities when both endpoints carry a
                // real simulation state (smooth acceleration across the boundary, Source-style).
                if (!from.isSleeping && !to.isSleeping)
                {
                    const float spanSec = float(spanMs) * 0.001f;

                    const float a = alpha;
                    const float a2 = a * a;
                    const float a3 = a2 * a;

                    const float h00 = 2.0f * a3 - 3.0f * a2 + 1.0f;
                    const float h10 = a3 - 2.0f * a2 + a;
                    const float h01 = -2.0f * a3 + 3.0f * a2;
                    const float h11 = a3 - a2;

                    const Vec3f p0 = from.transform.GetTranslation();
                    const Vec3f p1 = to.transform.GetTranslation();
                    const Vec3f m0 = from.velocity * spanSec;
                    const Vec3f m1 = to.velocity * spanSec;

                    targetTransform.SetTranslation(p0 * h00 + m0 * h10 + p1 * h01 + m1 * h11);
                }
                else
                {
                    targetTransform.SetTranslation(Vec3(from.transform.GetTranslation()).Lerp(to.transform.GetTranslation(), alpha));
                }

                targetTransform.SetRotation(Quat4f(from.transform.GetRotation()).Slerp(to.transform.GetRotation(), alpha));
                targetTransform.SetScale(Vec3f(from.transform.GetScale()).Lerp(to.transform.GetScale(), alpha));
            }
        }

        // Render transform: interpolated Net.InterpolationDelay into the past.
        entity->SetLocalTransform(targetTransform, TransformChangeType::Simulation);

        if (NetGlobals::GetDeadReckoningEnabled())
        {
            const InterpolationState::Sample& latest = samples.Back();

            const double latestTimeMs = sample_time(latest);

            const double sinceSampleSec = MathUtil::Max(0.0, (spaceNowMs - latestTimeMs) / 1000.0);

            const double oneWayDelaySec = MathUtil::Clamp(
                double(g_gameClient->GetNetClient().GetRoundTripTime()) * 0.5 / 1000.0,
                0.0,
                0.15);

            const float extrapolationSeconds = latest.isSleeping
                ? 0.0f
                : float(sinceSampleSec + oneWayDelaySec);

            Vec3f extrapolation = it->second.velocityEstimate * extrapolationSeconds;

            constexpr float MaxExtrapolationDistance = 0.25f;

            const float extrapolationLength = extrapolation.Length();

            if (extrapolationLength > MaxExtrapolationDistance)
            {
                extrapolation *= MaxExtrapolationDistance / extrapolationLength;
            }

            Transform colliderTransform = latest.transform;
            colliderTransform.SetTranslation(latest.transform.GetTranslation() + extrapolation);

            const Vec3f& angularVelocityEstimate = it->second.angularVelocityEstimate;
            const float angularSpeed = angularVelocityEstimate.Length();

            if (angularSpeed > MathUtil::epsilonF)
            {
                constexpr float MaxAngularExtrapolationRadians = 0.25f;

                const float angleRadians = MathUtil::Clamp(
                    angularSpeed * extrapolationSeconds,
                    0.0f,
                    MaxAngularExtrapolationRadians);

                const Quat4f rotationDelta(
                    angularVelocityEstimate * (1.0f / angularSpeed),
                    angleRadians);

                colliderTransform.SetRotation(rotationDelta * latest.transform.GetRotation());
            }

            // Underrun: the render time has run past the newest buffered sample (packet burst
            // gap). Ride the extrapolated position instead of freezing at the newest sample.
            if (renderTimeMs > latestTimeMs)
            {
                targetTransform = colliderTransform;
            }

            entity->SetLocalTransform(colliderTransform, TransformChangeType::Simulation);

            SyncColliderToEntity(entity, it->second, delta);

            entity->SetLocalTransform(targetTransform, TransformChangeType::Simulation);
        }
        else
        {
            SyncColliderToEntity(entity, it->second, delta);
        }

        ++it;
    }
}

void ReplicationApplySystem::SyncColliderToEntity(Entity* entity, InterpolationState& interpolation, float deltaTime)
{
    RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>();

    if (rigidBodyComponent == nullptr || !rigidBodyComponent->rigidBody.IsValid())
    {
        return;
    }

    World* world = GetWorld();

    if (!world)
    {
        return;
    }

    PhysicsWorldBase* physicsWorld = world->GetPhysicsWorld();

    if (!physicsWorld)
    {
        return;
    }

    Transform worldTransform;
    worldTransform.SetTranslation(entity->GetWorldTranslation());
    worldTransform.SetRotation(entity->GetWorldRotation());
    worldTransform.SetScale(entity->GetWorldScale());

    // Kinematic bodies here are driven purely by replication (see World::SyncPhysicsBodyKinematicStates);
    // move them with a derived velocity rather than teleporting, so contacts against them (a character
    // standing on/pushing one) solve smoothly instead of every network update popping them in place.
    if (rigidBodyComponent->rigidBody->IsLocallyPredicted())
    {
        // While the body is temporarily Dynamic for local push prediction, Jolt's own solver owns it;
        // don't write into the physics body at all.
        return;
    }

    if (rigidBodyComponent->rigidBody->IsKinematic())
    {
        if (interpolation.snapNextColliderSync)
        {
            // Large authoritative jump (respawn/teleport): hard-snap the body rather than gliding.
            physicsWorld->SetRigidBodyTransform(rigidBodyComponent->rigidBody, worldTransform);

            interpolation.snapNextColliderSync = false;
        }
        else
        {
            physicsWorld->MoveRigidBodyKinematic(rigidBodyComponent->rigidBody, worldTransform, deltaTime);
        }
    }
    else
    {
        physicsWorld->SetRigidBodyTransform(rigidBodyComponent->rigidBody, worldTransform);

        interpolation.snapNextColliderSync = false;
    }
}

void ReplicationApplySystem::UpdateStreamingVolume(Span<const Handle<Scene>> scenes)
{
    Handle<Entity> myPlayerEntity = GetMyPlayerEntity();

    if (!myPlayerEntity.IsValid())
    {
        return;
    }

    if (!m_playerStreamingVolume.IsValid())
    {
        m_playerStreamingVolume = MakeHandle<CameraStreamingVolume>();
        InitObject(m_playerStreamingVolume);

        g_streamingManager->AddStreamingVolume(m_playerStreamingVolume);
    }

    const Vec3f worldTranslation = myPlayerEntity->GetWorldTranslation();
    m_playerStreamingVolume->SetBoundingBox(BoundingBox(worldTranslation - 10.0f, worldTranslation + 10.0f));
}

Handle<Entity> ReplicationApplySystem::GetMyPlayerEntity() const
{
    if (g_gameClient != nullptr && g_gameClient->IsConnected())
    {
        const net::NetConnectionId myConnectionId = g_gameClient->GetNetClient().GetConnectionId();

        // @Fixme this sucks
        for (const auto& pair : m_netIdToEntity)
        {
            if (PlayerComponent* playerComponent = pair.second->TryGetComponent<PlayerComponent>())
            {
                if (playerComponent->connectionId == myConnectionId)
                {
                    return pair.second;
                }
            }
        }

        return Handle<Entity>::Null();
    }

    World* world = GetWorld();

    if (!world)
    {
        return Handle<Entity>::Null();
    }

    for (const Handle<Scene>& scene : world->GetScenes())
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, playerComponent] : scene->GetEntityManager()->GetEntitySet<PlayerComponent>())
        {
            return MakeStrongRef(entity);
        }
    }

    return Handle<Entity>::Null();
}

Scene* ReplicationApplySystem::FindTargetScene(Span<const Handle<Scene>> scenes, Name sceneName)
{
    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        if (scene->GetName() == sceneName)
        {
            return scene;
        }
    }

    return nullptr;
}

Entity* ReplicationApplySystem::TryResolveSpawn(
    const ReplicationOp<ReplicationOpType::Spawn>& spawnOp,
    Span<const Handle<Scene>> scenes)
{
    Scene* targetScene = FindTargetScene(scenes, spawnOp.sceneName);

    if (!targetScene)
    {
        return nullptr;
    }

    // If this Spawn is the replication echo of our own player entity, reconcile onto the
    // locally-driven entity instead of spawning a duplicate.
    Entity* existing = SceneHelpers::FindMyLocalPlayerEntity(*targetScene, spawnOp.ownerConnectionId);

    if (!existing)
    {
        existing = DynamicCast<Entity>(targetScene->FindNodeByUUID(spawnOp.uuid));
    }

    if (existing != nullptr)
    {
        if (!existing->HasComponent<ReplicationStateComponent>())
        {
            existing->AddComponent<ReplicationStateComponent>(ReplicationStateComponent { spawnOp.netId });
        }

        existing->SetLocalTransform(spawnOp.transform);

        return existing;
    }

    if (IsWaitingOnParent(spawnOp))
    {
        return nullptr;
    }

    return ExecuteEntitySpawn(spawnOp, *targetScene);
}

void ReplicationApplySystem::TryResolvePendingSpawns(Span<const Handle<Scene>> scenes)
{
    bool shouldContinue = true;

    while (shouldContinue)
    {
        shouldContinue = false;

        for (size_t i = 0; i < m_pendingSpawns.Size();)
        {
            PendingSpawn& pending = m_pendingSpawns[i];

            if (Entity* resolvedEntity = TryResolveSpawn(pending.spawnOp, scenes))
            {
                m_netIdToEntity[pending.spawnOp.netId] = MakeStrongRef(resolvedEntity);

                m_pendingSpawns.EraseAt(i);

                shouldContinue = true;

                continue;
            }

            ++i;
        }
    }
}

bool ReplicationApplySystem::IsWaitingOnParent(
    const ReplicationOp<ReplicationOpType::Spawn>& spawnOp)
{
    return spawnOp.parentNetId != InvalidNetId
        && m_netIdToEntity.Find(spawnOp.parentNetId) == m_netIdToEntity.End();
}

Entity* ReplicationApplySystem::ExecuteEntitySpawn(
    const ReplicationOp<ReplicationOpType::Spawn>& spawnOp,
    const Scene& targetScene)
{
    const Class* entityClass = GetClass(spawnOp.typeId);
    if (!entityClass)
    {
        HYP_LOG(Replication, Error, "Cannot spawn Entity, no Class with typeid {}", spawnOp.typeId.Value());
        return nullptr;
    }

    if (!entityClass->IsDerivedFrom(Entity::StaticClass()))
    {
        HYP_LOG(Replication, Error, "Cannot spawn Entity, Class '{}' is not a subclass of Entity", entityClass->GetName());
        return nullptr;
    }

    Handle<Entity> entity = targetScene.GetEntityManager()->AddTypedEntity(entityClass);
    if (!entity.IsValid())
    {
        HYP_LOG(Replication, Error, "Spawned Entity of Class '{}' could not be instantiated", entityClass->GetName());
        return Handle<Entity>::Null();
    }

    if (spawnOp.parentNetId != InvalidNetId)
    {
        auto parentIt = m_netIdToEntity.Find(spawnOp.parentNetId);
        Assert(parentIt != m_netIdToEntity.End(), "Caller must ensure the parent is already resolved");

        parentIt->second->AddChild(entity);
    }
    else
    {
        targetScene.GetRoot()->AddChild(entity);
    }

    entity->SetName(spawnOp.name);
    entity->SetLocalTransform(spawnOp.transform);
    entity->AddComponent<ReplicationStateComponent>(ReplicationStateComponent { spawnOp.netId });

    HYP_LOG(Replication, Info, "Spawned Entity '{}' of Class '{}'", entity->GetName(), entityClass->GetName());

    // attached to another node so ref remains valid
    return entity.Get();
}
} // namespace Hyperion
