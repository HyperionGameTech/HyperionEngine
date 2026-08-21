/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Systems/ReplicationSystem.hpp>
#include <Scene/Systems/CharacterControllerSystem.hpp> // For CharacterControllerInputHandler. @TODO move elsewhere

#include <Scene/Components/ReplicationStateComponent.hpp>
#include <Scene/Components/PlayerComponent.hpp>
#include <Scene/Components/CharacterControllerComponent.hpp>

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

static constexpr float ReplicationInterestRadius = 50.0f;

struct PlayerPosition
{
    net::NetConnectionId connectionId;
    Vec3f worldTranslation;
};

static Array<PlayerPosition, SceneTempAllocator> CollectPlayerPositions(Span<Handle<Scene>> scenes, SystemBase* system)
{
    Array<PlayerPosition, SceneTempAllocator> positions;

    for (Scene* scene : scenes)
    {
        if (!system->ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, playerComponent] : scene->GetEntityManager()->GetEntitySet<PlayerComponent>())
        {
            if (playerComponent.connectionId == Invalid<net::NetConnectionId>)
            {
                continue;
            }

            positions.PushBack(PlayerPosition { playerComponent.connectionId, entity->GetWorldTranslation() });
        }
    }

    return positions;
}

template <class AllocatorType>
static void CollectInterestedConnections(const Vec3f& position, const Array<PlayerPosition, SceneTempAllocator>& playerPositions, Array<net::NetConnectionId, AllocatorType>& outConnections)
{
    const float radiusSquared = ReplicationInterestRadius * ReplicationInterestRadius;

    for (const PlayerPosition& playerPosition : playerPositions)
    {
        if (position.DistanceSquared(playerPosition.worldTranslation) <= radiusSquared)
        {
            outConnections.PushBack(playerPosition.connectionId);
        }
    }
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

    if (const net::NetConnectionId ownerConnectionId = GetOwnerConnectionId(entity); ownerConnectionId != Invalid<net::NetConnectionId>)
    {
        m_connectionIdToEntity.Set(ownerConnectionId, entity);
    }

    HYP_LOG(Replication, Info, "Entity {} added to replication (netId={}), broadcasting EntitySpawn",
        entity->Id().Value(), uint32(netId));

    net::NetBuffer payload = SerializeEntitySpawnPayload(entity);

    World* world = GetWorld();
    Array<Handle<Scene>, SceneTempAllocator> scenes(world->GetScenes());
    Array<PlayerPosition, SceneTempAllocator> playerPositions = CollectPlayerPositions(scenes, this);

    Array<net::NetConnectionId, SceneTempAllocator> interestedConnections;
    CollectInterestedConnections(entity->GetWorldTranslation(), playerPositions, interestedConnections);

    if (interestedConnections.Any())
    {
        GetGameServerThread()->GetScheduler().Enqueue(
            [netId,
             interestedConnections = Array<net::NetConnectionId, NetAllocator>(interestedConnections),
             payload = std::move(payload)]()
            {
                net::NetServer& netServer = g_gameServer->GetNetServer();

                for (net::NetConnectionId connectionId : interestedConnections)
                {
                    netServer.SendMessageTo(
                        connectionId,
                        NetMessageId::EntitySpawn,
                        NetChannelMode::ReliableOrdered,
                        NetStreamKey(uint32(netId)),
                        payload.ToByteView());
                }
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

void ReplicationSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    ReplicationStateComponent& rsc = entity->GetComponent<ReplicationStateComponent>();
    const NetId netId = rsc.netId;

    entity->RemoveComponent<ReplicationStateComponent>();

    m_netIdToEntity.Erase(netId);

    if (const net::NetConnectionId ownerConnectionId = GetOwnerConnectionId(entity); ownerConnectionId != Invalid<net::NetConnectionId>)
    {
        m_connectionIdToEntity.Erase(ownerConnectionId);
    }

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

struct TargetedSnapshot
{
    net::NetConnectionId connectionId;
    ReplicationSnapshot snapshot;
};

void ReplicationSystem::ApplyPendingRequests()
{
    Array<ServerRequestBase*, SceneTempAllocator> requests;
    g_gameServer->GetRequestManager().DrainPendingRequests(requests);

    for (const ServerRequestBase* requestPtr : requests)
    {
        switch (requestPtr->type)
        {
        case ServerRequestType::TransformEntity:
        {
            auto it = m_netIdToEntity.Find(requestPtr->netId);

            if (it == m_netIdToEntity.End())
            {
                break;
            }

            const ServerRequest<ServerRequestType::TransformEntity>& request = static_cast<const ServerRequest<ServerRequestType::TransformEntity>&>(*requestPtr);

            it->second->SetLocalTransform(request.transform);

            break;
        }
        case ServerRequestType::PlayerInput:
        {
            auto it = m_connectionIdToEntity.Find(requestPtr->connectionId);

            if (it == m_connectionIdToEntity.End())
            {
                break;
            }

            const ServerRequest<ServerRequestType::PlayerInput>& request = static_cast<const ServerRequest<ServerRequestType::PlayerInput>&>(*requestPtr);

            if (CharacterControllerComponent* characterControllerComponent = it->second->TryGetComponent<CharacterControllerComponent>();
                characterControllerComponent != nullptr && characterControllerComponent->inputHandler.IsValid())
            {
                CharacterControllerInputHandler* inputHandler = StaticCast<CharacterControllerInputHandler>(characterControllerComponent->inputHandler);

                inputHandler->SetMovementInput(request.movementInput);
                inputHandler->SetIsJumpRequested(request.jumpRequested);
            }

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
        for (net::NetConnectionId connectionId : newConnections)
        {
            m_pendingCatchUpConnections.PushBack(connectionId);
        }
    }

    ProcessPendingCatchUp(scenes);

    Array<Entity*, SceneTempAllocator> updatedEntities;
    Array<TargetedSnapshot, SceneTempAllocator> targetedSnapshots;

    Array<PlayerPosition, SceneTempAllocator> playerPositions;
    bool playerPositionsComputed = false;

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        EntityManager* entityManager = scene->GetEntityManager();

        targetedSnapshots.Resize(0);
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

            if (!playerPositionsComputed)
            {
                playerPositions = CollectPlayerPositions(scenes, this);
                playerPositionsComputed = true;
            }

            Array<net::NetConnectionId, SceneTempAllocator> interestedConnections;
            CollectInterestedConnections(entity->GetWorldTranslation(), playerPositions, interestedConnections);

            for (net::NetConnectionId connectionId : interestedConnections)
            {
                targetedSnapshots.PushBack(TargetedSnapshot { connectionId, ReplicationSnapshot { netId, payload } });
            }

            updatedEntities.PushBack(entity);
        }

        if (targetedSnapshots.Any())
        {
            HYP_LOG(Replication, Debug, "Sending targeted ComponentSnapshot for {} (connection, entity) pairs in scene '{}'",
                targetedSnapshots.Size(), scene->GetName());

            GetGameServerThread()->GetScheduler().Enqueue(
                [targetedSnapshots = Array<TargetedSnapshot, NetAllocator>(targetedSnapshots)]()
                {
                    net::NetServer& netServer = g_gameServer->GetNetServer();

                    for (const TargetedSnapshot& targeted : targetedSnapshots)
                    {
                        netServer.SendMessageTo(
                            targeted.connectionId,
                            NetMessageId::ComponentSnapshot,
                            NetChannelMode::UnreliableOrdered,
                            NetStreamKey(uint32(targeted.snapshot.netId)),
                            targeted.snapshot.payload.ToByteView());
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

void ReplicationSystem::ProcessPendingCatchUp(Span<Handle<Scene>> scenes)
{
    if (m_pendingCatchUpConnections.Empty())
    {
        return;
    }

    Array<PlayerPosition, SceneTempAllocator> playerPositions = CollectPlayerPositions(scenes, this);
    const float radiusSquared = ReplicationInterestRadius * ReplicationInterestRadius;

    for (size_t i = 0; i < m_pendingCatchUpConnections.Size();)
    {
        const net::NetConnectionId connectionId = m_pendingCatchUpConnections[i];

        const PlayerPosition* playerPosition = nullptr;

        for (const PlayerPosition& candidate : playerPositions)
        {
            if (candidate.connectionId == connectionId)
            {
                playerPosition = &candidate;

                break;
            }
        }

        if (!playerPosition)
        {
            // This connection's player entity hasn't been created/positioned yet -- retry next tick.
            ++i;

            continue;
        }

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
                if (entity->GetWorldTranslation().DistanceSquared(playerPosition->worldTranslation) > radiusSquared)
                {
                    continue;
                }

                catchUpSpawns.PushBack(ReplicationSnapshot { replicationState.netId, SerializeEntitySpawnPayload(entity) });
            }
        }

        if (catchUpSpawns.Any())
        {
            HYP_LOG(Replication, Info, "Sending catch-up EntitySpawn for {} entities to connection {}",
                catchUpSpawns.Size(), uint32(connectionId));

            GetGameServerThread()->GetScheduler().Enqueue(
                [connectionId, catchUpSpawns = Array<ReplicationSnapshot, NetAllocator>(catchUpSpawns)]()
                {
                    net::NetServer& netServer = g_gameServer->GetNetServer();

                    for (const ReplicationSnapshot& spawn : catchUpSpawns)
                    {
                        netServer.SendMessageTo(
                            connectionId,
                            NetMessageId::EntitySpawn,
                            NetChannelMode::ReliableOrdered,
                            NetStreamKey(uint32(spawn.netId)),
                            spawn.payload.ToByteView());
                    }
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }

        m_pendingCatchUpConnections.EraseAt(i);
    }
}

} // namespace Hyperion
