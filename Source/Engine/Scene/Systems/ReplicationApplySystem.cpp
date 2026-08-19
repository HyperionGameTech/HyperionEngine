/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Systems/ReplicationApplySystem.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>

#include <Scene/Components/ReplicationStateComponent.hpp>

#include <Framework/Client/GameClient.hpp>
#include <Framework/Client/ClientReplicationManager.hpp>

#include <Net/NetMemory.hpp>

#include <ReplicationApplySystem.generated.inl>

namespace Hyperion {

void ReplicationApplySystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    if (scenes.Size() == 0 || g_gameClient == nullptr)
    {
        return;
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

            Scene* targetScene = nullptr;

            for (Scene* scene : scenes)
            {
                if (!ShouldProcessScene(scene))
                {
                    continue;
                }

                if (scene->GetName() == op.sceneName)
                {
                    targetScene = scene;

                    break;
                }
            }

            if (targetScene == nullptr)
            {
                // Scene not present on this client (not loaded yet, or a mismatched world) --
                // drop it. The server doesn't currently resend EntitySpawn, so if this scene
                // loads later this entity won't retroactively appear.
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
