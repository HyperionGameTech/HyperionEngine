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

#include <Scene/Systems/PhysicsSystem.hpp>

#include <Scene/Camera/Camera.hpp>
#include <Scene/Camera/FirstPersonCamera.hpp>
#include <Scene/Camera/Streaming/CameraStreamingVolume.hpp>

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

static Scene* FindTargetScene(Span<Handle<Scene>> scenes, SystemBase* system, Name sceneName)
{
    for (Scene* scene : scenes)
    {
        if (!system->ShouldProcessScene(scene))
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

static inline bool IsWaitingOnParent(const ReplicationOp<ReplicationOpType::Spawn>& spawnOp, const Map<NetId, Handle<Entity>, SceneAllocator>& netIdToEntity)
{
    return spawnOp.parentNetId != InvalidNetId
        && netIdToEntity.Find(spawnOp.parentNetId) == netIdToEntity.End();
}

static Handle<Entity> FindLocalEntityByUuid(const Scene& scene, const UUID& uuid)
{
    for (Node* node : scene.GetRoot()->GetDescendants())
    {
        if (node->GetUUID() != uuid)
        {
            continue;
        }

        if (Entity* entity = DynamicCast<Entity>(node))
        {
            return MakeStrongRef(entity);
        }
    }

    return Handle<Entity>::Null();
}

static Handle<Entity> ExecuteEntitySpawn(const ReplicationOp<ReplicationOpType::Spawn>& spawnOp, const Scene& targetScene, const Map<NetId, Handle<Entity>, SceneAllocator>& netIdToEntity)
{
    const Class* entityClass = GetClass(spawnOp.typeId);
    if (!entityClass)
    {
        HYP_LOG(Replication, Error, "Cannot spawn Entity, no Class with typeid {}", spawnOp.typeId.Value());
        return Handle<Entity>::Null();
    }

    if (!entityClass->IsDerivedFrom(Entity::StaticClass()))
    {
        HYP_LOG(Replication, Error, "Cannot spawn Entity, Class '{}' is not a subclass of Entity", entityClass->GetName());
        return Handle<Entity>::Null();
    }

    Handle<Entity> entity = targetScene.GetEntityManager()->AddTypedEntity(entityClass);
    if (!entity.IsValid())
    {
        HYP_LOG(Replication, Error, "Spawned Entity of Class '{}' could not be instantiated", entityClass->GetName());
        return Handle<Entity>::Null();
    }

    if (spawnOp.parentNetId != InvalidNetId)
    {
        auto parentIt = netIdToEntity.Find(spawnOp.parentNetId);
        Assert(parentIt != netIdToEntity.End(), "Caller must ensure the parent is already resolved");

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

    return entity;
}

static Handle<Entity> FindMyLocalPlayerEntity(const Scene& scene, net::NetConnectionId ownerConnectionId)
{
    if (ownerConnectionId == Invalid<net::NetConnectionId>
        || g_gameClient == nullptr
        || !g_gameClient->IsConnected()
        || g_gameClient->GetNetClient().GetConnectionId() != ownerConnectionId)
    {
        return Handle<Entity>::Null();
    }

    for (auto [entity, playerComponent] : scene.GetEntityManager()->GetEntitySet<PlayerComponent>())
    {
        if (playerComponent.connectionId == ownerConnectionId)
        {
            return MakeStrongRef(entity);
        }
    }

    return Handle<Entity>::Null();
}

static Handle<Entity> TryResolveSpawn(
    const ReplicationOp<ReplicationOpType::Spawn>& spawnOp,
    Span<Handle<Scene>> scenes,
    SystemBase* system,
    const Map<NetId, Handle<Entity>, SceneAllocator>& netIdToEntity)
{
    Scene* targetScene = FindTargetScene(scenes, system, spawnOp.sceneName);

    if (!targetScene)
    {
        return Handle<Entity>::Null();
    }

    // If this Spawn is the replication echo of our own player entity, reconcile onto the
    // locally-driven entity instead of spawning a duplicate.
    Handle<Entity> existing = FindMyLocalPlayerEntity(*targetScene, spawnOp.ownerConnectionId);

    if (!existing.IsValid())
    {
        existing = FindLocalEntityByUuid(*targetScene, spawnOp.uuid);
    }

    if (existing.IsValid())
    {
        if (!existing->HasComponent<ReplicationStateComponent>())
        {
            existing->AddComponent<ReplicationStateComponent>(ReplicationStateComponent { spawnOp.netId });
        }

        existing->SetLocalTransform(spawnOp.transform);

        return existing;
    }

    if (IsWaitingOnParent(spawnOp, netIdToEntity))
    {
        return Handle<Entity>::Null();
    }

    return ExecuteEntitySpawn(spawnOp, *targetScene, netIdToEntity);
}

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

                Array<Handle<Scene>, SceneAllocator> scenes(world->GetScenes());
                TryResolvePendingSpawns(scenes);
            }));
}

void ReplicationApplySystem::OnRemovedFromWorld(World* world)
{
    m_delegateHandlers.Remove("OnSceneAdded"_sh);

    SystemBase::OnRemovedFromWorld(world);
}

void ReplicationApplySystem::TryResolvePendingSpawns(Span<Handle<Scene>> scenes)
{
    bool shouldContinue = true;

    while (shouldContinue)
    {
        shouldContinue = false;

        for (size_t i = 0; i < m_pendingSpawns.Size();)
        {
            PendingSpawn& pending = m_pendingSpawns[i];

            if (Handle<Entity> resolvedEntity = TryResolveSpawn(pending.spawnOp, scenes, this, m_netIdToEntity); resolvedEntity.IsValid())
            {
                m_netIdToEntity.Set(pending.spawnOp.netId, resolvedEntity);

                m_pendingSpawns.EraseAt(i);

                shouldContinue = true;

                continue;
            }

            ++i;
        }
    }
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

    // Don't use temp - not enough mem
    Array<ReplicationOpBase*, SceneAllocator> ops;
    g_gameClient->GetReplicationManager().DrainPendingOps(ops);

    // The local player's entity is predicted locally; its snapshots are ignored.
    NetId localPlayerNetId = Invalid<NetId>;

    if (Handle<Entity> myPlayerEntity = GetMyPlayerEntity(); myPlayerEntity.IsValid())
    {
        if (const ReplicationStateComponent* replicationState = myPlayerEntity->TryGetComponent<ReplicationStateComponent>())
        {
            localPlayerNetId = replicationState->netId;
        }
    }

    for (const ReplicationOpBase* opPtr : ops)
    {
        switch (opPtr->type)
        {
        case ReplicationOpType::Spawn:
        {
        HYP_LOG(Replication, Info, "Client got message of type {}", opPtr->type);

            const ReplicationOp<ReplicationOpType::Spawn>& op = static_cast<const ReplicationOp<ReplicationOpType::Spawn>&>(*opPtr);

            if (m_netIdToEntity.Find(op.netId) != m_netIdToEntity.End())
            {
                m_netIdToEntity.Find(op.netId)->second->SetLocalTransform(op.transform);

                break;
            }

            if (Handle<Entity> resolvedEntity = TryResolveSpawn(op, scenes, this, m_netIdToEntity); resolvedEntity.IsValid())
            {
                m_netIdToEntity.Set(op.netId, resolvedEntity);

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
                // Snapshot for an entity we haven't seen an EntitySpawn for yet (reordering,
                // or the spawn is still in flight) -- drop it, a later snapshot will catch up.
                break;
            }

            // Our own player entity is client-predicted (see CharacterControllerSystem);
            // authoritative state for it arrives via PlayerMoveAck reconciliation instead.
            if (op.netId == localPlayerNetId)
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

        Array<InterpolationState::Sample, SceneAllocator>& samples = it->second.samples;

        // Advance past samples the render time has already passed, keeping at least the
        // last one as the hold pose.
        while (samples.Size() > 1 && samples[1].receiveTimeMs <= renderTimeMs)
        {
            samples.EraseAt(0);
        }

        Entity* entity = entityIt->second.Get();

        if (samples.Size() == 1)
        {
            entity->SetLocalTransform(samples[0].transform);

            ++it;

            continue;
        }

        const InterpolationState::Sample& from = samples[0];
        const InterpolationState::Sample& to = samples[1];

        const double spanMs = double(to.receiveTimeMs) - double(from.receiveTimeMs);

        if (spanMs <= 0.0)
        {
            entity->SetLocalTransform(to.transform);

            ++it;

            continue;
        }

        // Clamp to the newest sample: on a burst gap we hold rather than extrapolate.
        const float alpha = float(MathUtil::Clamp((double(renderTimeMs) - double(from.receiveTimeMs)) / spanMs, 0.0, 1.0));

        Transform blended;

        // Because we made Lerp()/Slerp() mutate for some reason years ago, we need to wrap in temp objects
        // @TODO: Change this when we fix those
        blended.SetTranslation(Vec3(from.transform.GetTranslation()).Lerp(to.transform.GetTranslation(), alpha));
        blended.SetRotation(Quat4f(from.transform.GetRotation()).Slerp(to.transform.GetRotation(), alpha));
        blended.SetScale(Vec3f(from.transform.GetScale()).Lerp(to.transform.GetScale(), alpha));

        entity->SetLocalTransform(blended);

        ++it;
    }
}

static Handle<Camera> FindCameraChild(const Handle<Entity>& entity)
{
    for (const Handle<Node>& child : entity->GetChildren())
    {
        if (Handle<Camera> camera = DynamicCast<Camera>(child); camera.IsValid())
        {
            return camera;
        }
    }

    return Handle<Camera>::Null();
}

void ReplicationApplySystem::UpdateStreamingVolume(Span<Handle<Scene>> scenes)
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

} // namespace Hyperion
