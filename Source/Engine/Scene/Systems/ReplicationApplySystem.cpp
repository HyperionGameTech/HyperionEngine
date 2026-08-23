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
        HYP_LOG(Replication, Info, "Client got message of type {}", opPtr->type);

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

            interpolation.samples.PushBack(InterpolationState::Sample { op.receiveTimeMs, op.transform });

            while (interpolation.samples.Size() > InterpolationState::MaxSamples)
            {
                interpolation.samples.EraseAt(0);
            }

            break;
        }
        }
    }

    UpdateInterpolatedEntities();
    UpdateStreamingVolume(scenes);
}

void ReplicationApplySystem::UpdateInterpolatedEntities()
{
    if (m_interpolationStates.Empty())
    {
        return;
    }

    const uint64 nowMs = Time::Now().ToMilliseconds();
    const uint64 renderTimeMs = nowMs - uint64(EngineGlobals::GetInterpolationDelay() * 1000.0f);

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

        while (samples.Size() > 1 && samples[1].receiveTimeMs <= renderTimeMs)
        {
            samples.EraseAt(0);
        }

        Entity* entity = entityIt->second.Get();

        Transform targetTransform;

        if (samples.Size() == 1)
        {
            targetTransform = samples[0].transform;
        }
        else
        {
            const InterpolationState::Sample& from = samples[0];
            const InterpolationState::Sample& to = samples[1];

            const double spanMs = double(to.receiveTimeMs) - double(from.receiveTimeMs);

            if (spanMs <= 0.0)
            {
                targetTransform = to.transform;
            }
            else
            {
                // Clamp to the newest sample: on a burst gap we hold rather than extrapolate.
                const float alpha = float(MathUtil::Clamp((double(renderTimeMs) - double(from.receiveTimeMs)) / spanMs, 0.0, 1.0));

                // Because we made Lerp()/Slerp() mutate for some reason years ago, we need to wrap in temp objects
                // @TODO: Change this when we fix those
                targetTransform.SetTranslation(Vec3(from.transform.GetTranslation()).Lerp(to.transform.GetTranslation(), alpha));
                targetTransform.SetRotation(Quat4f(from.transform.GetRotation()).Slerp(to.transform.GetRotation(), alpha));
                targetTransform.SetScale(Vec3f(from.transform.GetScale()).Lerp(to.transform.GetScale(), alpha));
            }
        }

        entity->SetLocalTransform(targetTransform);

        SyncColliderToEntity(entity);

        ++it;
    }
}

void ReplicationApplySystem::SyncColliderToEntity(Entity* entity)
{
    RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>();

    if (rigidBodyComponent == nullptr || !rigidBodyComponent->rigidBody.IsValid())
    {
        return;
    }

    World* world = GetWorld();

    if (world == nullptr)
    {
        return;
    }

    PhysicsWorldBase* physicsWorld = world->GetPhysicsWorld();

    if (physicsWorld == nullptr)
    {
        return;
    }

    Transform worldTransform;
    worldTransform.SetTranslation(entity->GetWorldTranslation());
    worldTransform.SetRotation(entity->GetWorldRotation());
    worldTransform.SetScale(entity->GetWorldScale());

    physicsWorld->SetRigidBodyTransform(rigidBodyComponent->rigidBody, worldTransform);
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
