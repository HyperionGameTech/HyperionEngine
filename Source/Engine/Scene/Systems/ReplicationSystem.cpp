/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Systems/ReplicationSystem.hpp>

#include <Scene/Components/ReplicationStateComponent.hpp>
#include <Scene/Components/PlayerComponent.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Framework/Server/GameServer.hpp>
#include <Framework/Server/ServerRequestManager.hpp>

#include <Net/NetMessage.hpp>
#include <Net/NetMemory.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Reflection/Class.hpp>

#include <Core/Utilities/Traits.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Task.hpp>

#include <Core/Logging/Logger.hpp>

#include <ReplicationSystem.generated.inl>

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(Replication);

using net::NetAllocator;
using net::NetChannelMode;
using net::NetMessageId;
using net::NetStreamKey;

static ThreadBase* GetGameServerThread()
{
    return g_gameServer->GetThread();
}

static NetId GetReplicatedParentNetId(Entity* entity)
{
    Entity* parentEntity = DynamicCast<Entity>(entity->GetParent());

    if (!parentEntity)
    {
        return Invalid<NetId>;
    }

    if (ReplicationStateComponent* parentRsc = parentEntity->TryGetComponent<ReplicationStateComponent>())
    {
        return parentRsc->netId;
    }

    return Invalid<NetId>;
}

static net::NetConnectionId GetOwnerConnectionId(Entity* entity)
{
    if (PlayerComponent* playerComponent = entity->TryGetComponent<PlayerComponent>())
    {
        return playerComponent->connectionId;
    }

    return Invalid<net::NetConnectionId>;
}

static net::NetBuffer SerializeEntitySpawnPayload(Entity* entity)
{
    const TypeId typeId = entity->InstanceClass()->GetTypeId();

    const Name entityName = entity->GetName();
    const NetId parentNetId = GetReplicatedParentNetId(entity);
    const UUID uuid = entity->GetUUID();
    const net::NetConnectionId ownerConnectionId = GetOwnerConnectionId(entity);
    const Name sceneName = entity->GetEntityManager()->GetScene()->GetName();

    const Transform& transform = entity->GetLocalTransform();

    net::NetBuffer payload;
    MemoryByteWriter<NetAllocator, 1> writer(&payload);

    writer.Write(typeId.Value());
    writer.Write(parentNetId);
    writer.Write(uuid);
    writer.Write(ownerConnectionId);
    writer.Write(entityName);
    writer.Write(sceneName);
    writer.Write(transform.GetTranslation());
    writer.Write(transform.GetRotation());
    writer.Write(transform.GetScale());

    return payload;
}

void ReplicationSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    Assert(g_gameServer != nullptr);

    const NetId netId = g_gameServer->AllocNetId();

    entity->AddComponent<ReplicationStateComponent>(ReplicationStateComponent { netId });

    m_netIdToEntity.Set(netId, MakeStrongRef(entity));

    HYP_LOG(Replication, Info, "Entity {} added to replication (netId={}), broadcasting EntitySpawn",
        entity->Id().Value(), uint32(netId));

    net::NetBuffer payload = SerializeEntitySpawnPayload(entity);

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

    m_netIdToEntity.Erase(netId);

    HYP_LOG(Replication, Info, "Entity {} removed from replication (netId={}), broadcasting EntityDespawn",
        entity->Id().Value(), uint32(netId));

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

void ReplicationSystem::ApplyPendingRequests()
{
    Array<ServerRequestBase*, SceneTempAllocator> requests;
    g_gameServer->GetRequestManager().DrainPendingRequests(requests);

    for (const ServerRequestBase* requestPtr : requests)
    {
        auto it = m_netIdToEntity.Find(requestPtr->netId);

        if (it == m_netIdToEntity.End())
        {
            continue;
        }

        Entity* entity = it->second.Get();

        switch (requestPtr->type)
        {
        case ServerRequestType::TransformEntity:
        {
            const ServerRequest<ServerRequestType::TransformEntity>& request = static_cast<const ServerRequest<ServerRequestType::TransformEntity>&>(*requestPtr);

            entity->SetLocalTransform(request.transform);

            break;
        }
        }
    }
}

void ReplicationSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    ApplyPendingRequests();

    Array<net::NetConnectionId, SceneTempAllocator> newConnections;
    g_gameServer->DrainNewConnections(newConnections);

    if (newConnections.Any())
    {
        Array<ReplicationSnapshot, SceneTempAllocator> catchUpSpawns;

        for (Scene* scene : scenes)
        {
            if (!ShouldProcessScene(scene))
            {
                continue;
            }

            EntityManager* entityManager = scene->GetEntityManager();

            for (auto [entity, replicationState] : entityManager->GetEntitySet<ReplicationStateComponent>())
            {
                catchUpSpawns.PushBack(ReplicationSnapshot { replicationState.netId, SerializeEntitySpawnPayload(entity) });
            }
        }

        if (catchUpSpawns.Any())
        {
            HYP_LOG(Replication, Info, "Sending catch-up EntitySpawn for {} entities to {} newly-connected client(s)",
                catchUpSpawns.Size(), newConnections.Size());

            GetGameServerThread()->GetScheduler().Enqueue(
                [newConnections = Array<net::NetConnectionId, NetAllocator>(newConnections),
                 catchUpSpawns = Array<ReplicationSnapshot, NetAllocator>(catchUpSpawns)]()
                {
                    net::NetServer& netServer = g_gameServer->GetNetServer();

                    for (net::NetConnectionId connectionId : newConnections)
                    {
                        for (const ReplicationSnapshot& spawn : catchUpSpawns)
                        {
                            netServer.SendMessageTo(
                                connectionId,
                                NetMessageId::EntitySpawn,
                                NetChannelMode::ReliableOrdered,
                                NetStreamKey(uint32(spawn.netId)),
                                spawn.payload.ToByteView());
                        }
                    }
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }

        newConnections.Clear();
    }

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
            HYP_LOG(Replication, Debug, "Broadcasting ComponentSnapshot for {} dirty entities in scene '{}'",
                snapshots.Size(), scene->GetName());

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
