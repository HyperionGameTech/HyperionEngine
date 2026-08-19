/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Systems/ReplicationSystem.hpp>

#include <Scene/Components/ReplicationStateComponent.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Framework/Server/GameServer.hpp>

#include <Net/NetMessage.hpp>
#include <Net/NetMemory.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Task.hpp>

#include <ReplicationSystem.generated.inl>

namespace Hyperion {

using net::NetAllocator;
using net::NetChannelMode;
using net::NetMessageId;
using net::NetStreamKey;

static ThreadBase* GetGameServerThread()
{
    return g_gameServer->GetThread();
}

void ReplicationSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    Assert(g_gameServer != nullptr);

    const NetId netId = g_gameServer->AllocNetId();

    entity->AddComponent<ReplicationStateComponent>(ReplicationStateComponent { netId });

    const Name sceneName = entity->GetEntityManager()->GetScene()->GetName();
    const Transform& transform = entity->GetLocalTransform();

    net::NetBuffer payload;
    MemoryByteWriter<NetAllocator, 1> writer(&payload);
    writer.Write(sceneName);
    writer.Write(transform.GetTranslation());
    writer.Write(transform.GetRotation());
    writer.Write(transform.GetScale());

    GetGameServerThread()->GetScheduler().Enqueue(
        [netId, payload = std::move(payload)]()
        {
            g_gameServer->GetNetServer().Broadcast(
                NetMessageId::EntitySpawn,
                NetChannelMode::ReliableOrdered,
                NetStreamKey(uint32(netId)),
                payload.ToByteView());
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

void ReplicationSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    ReplicationStateComponent& rsc = entity->GetComponent<ReplicationStateComponent>();
    const NetId netId = rsc.netId;

    entity->RemoveComponent<ReplicationStateComponent>();

    GetGameServerThread()->GetScheduler().Enqueue(
        [netId]()
        {
            g_gameServer->GetNetServer().Broadcast(
                NetMessageId::EntityDespawn,
                NetChannelMode::ReliableOrdered,
                NetStreamKey(uint32(netId)),
                ConstByteView());
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);

    g_gameServer->FreeNetId(netId);
}

struct ReplicationSnapshot
{
    NetId netId;
    net::NetBuffer payload;
};

void ReplicationSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    Array<Entity*, SceneTempAllocator> updatedEntities;
    Array<ReplicationSnapshot, SceneTempAllocator> snapshots;

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        EntityManager* entityManager = scene->GetEntityManager();

        snapshots.Resize(0);
        updatedEntities.Resize(0);

        for (auto [entity, replicationState, _] : entityManager->GetEntitySet<ReplicationStateComponent, TagComponent<EntityTag::UpdateReplication>>())
        {
            const NetId netId = replicationState.netId;
            const Transform& transform = entity->GetLocalTransform();

            net::NetBuffer payload;
            MemoryByteWriter<NetAllocator, 1> writer(&payload);
            writer.Write(transform.GetTranslation());
            writer.Write(transform.GetRotation());
            writer.Write(transform.GetScale());

            snapshots.PushBack(ReplicationSnapshot { netId, std::move(payload) });

            updatedEntities.PushBack(entity);
        }

        if (snapshots.Any())
        {
            GetGameServerThread()->GetScheduler().Enqueue(
                [snapshots = Array<ReplicationSnapshot, NetAllocator>(snapshots)]()
                {
                    net::NetServer& netServer = g_gameServer->GetNetServer();

                    for (const ReplicationSnapshot& snapshot : snapshots)
                    {
                        netServer.Broadcast(
                            NetMessageId::ComponentSnapshot,
                            NetChannelMode::UnreliableOrdered,
                            NetStreamKey(uint32(snapshot.netId)),
                            snapshot.payload.ToByteView());
                    }
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }

        AfterProcess([updatedEntities = Array<Entity*, SceneAllocator>(updatedEntities)]()
                     {
                         for (Entity* entity : updatedEntities)
                         {
                             entity->RemoveTag<EntityTag::UpdateReplication>();
                         }
                     });
    }
}

} // namespace Hyperion
