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

void ReplicationApplySystem::OnAddedToWorld(World* world)
{
    SystemBase::OnAddedToWorld(world);

    m_delegateHandlers.Add(
        NAME("OnSceneAdded"),
        World::OnSceneAdded.Bind(
            this,
            [this, world](World* eventWorld, const Handle<Scene>& scene)
            {
                if (eventWorld != world)
                {
                    return;
                }

                TryResolvePendingSpawns(scene);
            }));
}

void ReplicationApplySystem::OnRemovedFromWorld(World* world)
{
    m_delegateHandlers.Remove("OnSceneAdded"_sh);

    SystemBase::OnRemovedFromWorld(world);
}

void ReplicationApplySystem::TryResolvePendingSpawns(Scene* scene)
{
    if (scene == nullptr)
    {
        return;
    }

    for (size_t i = 0; i < m_pendingSpawns.Size();)
    {
        PendingSpawn& pending = m_pendingSpawns[i];

        if (pending.sceneName != scene->GetName())
        {
            ++i;

            continue;
        }

        Handle<Entity> entity = scene->GetEntityManager()->AddEntity();

        scene->GetRoot()->AddChild(entity);

        entity->SetLocalTransform(pending.transform);
        entity->AddComponent<ReplicationStateComponent>(ReplicationStateComponent { pending.netId });

        m_netIdToEntity.Set(pending.netId, entity);

        HYP_LOG(Replication, Info, "Scene '{}' now loaded, applying deferred EntitySpawn for netId={}",
            pending.sceneName, uint32(pending.netId));

        m_pendingSpawns.EraseAt(i);
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
                "Giving up on EntitySpawn for netId={}: scene '{}' never loaded within {} seconds",
                uint32(pending.netId), pending.sceneName, PendingSpawnTimeoutSeconds);

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

            if (targetScene == nullptr)
            {
                m_pendingSpawns.PushBack(PendingSpawn { op.netId, op.sceneName, op.transform });

                break;
            }

            Handle<Entity> entity = targetScene->GetEntityManager()->AddEntity();

            targetScene->GetRoot()->AddChild(entity);

            entity->SetLocalTransform(op.transform);
            entity->AddComponent<ReplicationStateComponent>(ReplicationStateComponent { op.netId });

            m_netIdToEntity.Set(op.netId, entity);

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
