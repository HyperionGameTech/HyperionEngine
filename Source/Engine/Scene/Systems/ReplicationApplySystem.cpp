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

#include <Framework/Client/GameClient.hpp>
#include <Framework/Client/ClientReplicationManager.hpp>

#include <Net/NetMemory.hpp>

#include <Core/Utilities/GlobalContext.hpp>

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

            if (IsWaitingOnParent(pending.spawnOp, m_netIdToEntity))
            {
                ++i;

                continue;
            }

            Scene* targetScene = FindTargetScene(scenes, this, pending.spawnOp.sceneName);

            if (!targetScene)
            {
                ++i;

                continue;
            }

            if (Handle<Entity> spawnedEntity = ExecuteEntitySpawn(pending.spawnOp, *targetScene, m_netIdToEntity); spawnedEntity.IsValid())
            {
                m_netIdToEntity.Set(pending.spawnOp.netId, spawnedEntity);

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

            if (m_netIdToEntity.Find(op.netId) != m_netIdToEntity.End())
            {
                m_netIdToEntity.Find(op.netId)->second->SetLocalTransform(op.transform);

                break;
            }

            Scene* targetScene = FindTargetScene(scenes, this, op.sceneName);

            if (!targetScene || IsWaitingOnParent(op, m_netIdToEntity))
            {
                m_pendingSpawns.PushBack(PendingSpawn { op });

                break;
            }

            if (Handle<Entity> spawnedEntity = ExecuteEntitySpawn(op, *targetScene, m_netIdToEntity); spawnedEntity.IsValid())
            {
                m_netIdToEntity.Set(op.netId, spawnedEntity);

                // try to resolve, we might have unblocked a child entity waiting on this.
                TryResolvePendingSpawns(scenes);
            }

            break;
        }
        case ReplicationOpType::Despawn:
        {
            const ReplicationOp<ReplicationOpType::Despawn>& op = static_cast<const ReplicationOp<ReplicationOpType::Despawn>&>(*opPtr);

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

            // @TODO: snap for now -- lerp toward this instead once this end-to-end path is verified.
            it->second->SetLocalTransform(op.transform);

            break;
        }
        }
    }
}

} // namespace Hyperion
