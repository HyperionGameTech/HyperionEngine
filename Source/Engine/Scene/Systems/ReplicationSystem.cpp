/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Systems/ReplicationSystem.hpp>

#include <Scene/Components/ReplicationStateComponent.hpp>
#include <Scene/Components/PlayerComponent.hpp>
#include <Scene/Components/CharacterControllerComponent.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Framework/GameState.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Framework/Server/GameServer.hpp>
#include <Framework/Server/ServerRequestManager.hpp>

#include <Net/NetMessage.hpp>
#include <Net/NetMemory.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Reflection/Class.hpp>

#include <Core/Utilities/Traits.hpp>

#include <Core/Utilities/Time.hpp>

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

/// \see CollectInterestedConnections
static constexpr float ReplicationInterestRadius = 50.0f;
static constexpr float ReplicationInterestRadiusSq = ReplicationInterestRadius * ReplicationInterestRadius;

// Clients may simulate this much faster than real time before moves get dropped (clock drift, jitter)
static constexpr float PlayerMoveTimeBudgetSlack = 1.05f;

using PlayerPosition = Tuple<net::NetConnectionId, Vec3f>;

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

static Array<PlayerPosition, SceneTempAllocator> CollectPlayerPositions(
    Span<const Handle<Scene>> scenes,
    SystemBase* system)
{
    Array<PlayerPosition, SceneTempAllocator> positions;
    positions.Reserve(8);

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

            positions.EmplaceBack(playerComponent.connectionId, entity->GetWorldTranslation());
        }
    }

    return positions;
}

template <class AllocatorType>
static void CollectInterestedConnections(
    const Vec3f& position,
    const Array<PlayerPosition, SceneTempAllocator>& playerPositions,
    Array<net::NetConnectionId, AllocatorType>& outConnections)
{
    outConnections.Reserve(outConnections.Size() + playerPositions.Size());

    for (const auto& [netId, pos] : playerPositions)
    {
        if (position.DistanceSquared(pos) <= ReplicationInterestRadiusSq)
        {
            outConnections.PushBack(netId);
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

    Array<PlayerPosition, SceneTempAllocator> playerPositions = CollectPlayerPositions(world->GetScenes(), this);

    Array<net::NetConnectionId, SceneTempAllocator> interestedConnections;
    CollectInterestedConnections(entity->GetWorldTranslation(), playerPositions, interestedConnections);

    ThreadBase* gameServerThread = GetGameServerThread();

    if (interestedConnections.Any() && gameServerThread != nullptr)
    {
        for (net::NetConnectionId connectionId : interestedConnections)
        {
            m_knownEntities[connectionId].Insert(netId);
        }

        gameServerThread->GetScheduler().Enqueue(
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

    for (auto it = m_connectionIdToEntity.Begin(); it != m_connectionIdToEntity.End(); ++it)
    {
        if (it->second != entity)
        {
            continue;
        }

        m_playerMoveQueues.Erase(it->first);
        m_knownEntities.Erase(it->first);
        m_connectionIdToEntity.Erase(it);

        break;
    }

    // NetIds get recycled, so a stale entry would stop the next entity with this id from being spawned
    for (auto knownIt = m_knownEntities.Begin(); knownIt != m_knownEntities.End(); ++knownIt)
    {
        knownIt->second.Erase(netId);
    }

    HYP_LOG(Replication, Info, "Entity {} removed from replication (netId={}), broadcasting EntityDespawn",
        entity->Id().Value(), uint32(netId));

    // null once an editor-owned server has been stopped
    if (ThreadBase* gameServerThread = GetGameServerThread())
    {
        gameServerThread->GetScheduler().Enqueue(
            [netId]()
            {
                g_gameServer->GetNetServer().Broadcast(
                    NetMessageId::EntityDespawn,
                    NetChannelMode::ReliableOrdered,
                    NetStreamKey(uint32(netId)),
                    ConstByteView());
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }

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

            Entity* entity = it->second.Get();

            auto ownerIt = m_connectionIdToEntity.Find(requestPtr->connectionId);

            if (ownerIt == m_connectionIdToEntity.End() || ownerIt->second != entity)
            {
                HYP_LOG_ONCE(Replication, Warning, "Dropping transform request from connection {} for netId {}: connection does not own it",
                    uint32(requestPtr->connectionId), uint32(requestPtr->netId));

                break;
            }

            // character movement has to go through PlayerMoves, a raw transform would skip the move time budget
            if (entity->HasComponent<CharacterControllerComponent>())
            {
                break;
            }

            entity->SetLocalTransform(request.transform);

            break;
        }
        case ServerRequestType::PlayerMoves:
        {
            const ServerRequest<ServerRequestType::PlayerMoves>& request = static_cast<const ServerRequest<ServerRequestType::PlayerMoves>&>(*requestPtr);

            auto it = m_connectionIdToEntity.Find(requestPtr->connectionId);

            if (it == m_connectionIdToEntity.End())
            {
                HYP_LOG_ONCE(Replication, Warning, "Dropping player moves from connection {}: no replicated entity is owned by it, so they will never be acked",
                    uint32(requestPtr->connectionId));

                break;
            }

            if (it->second->TryGetComponent<CharacterControllerComponent>() == nullptr)
            {
                break;
            }

            PlayerMoveQueueState& queue = m_playerMoveQueues[requestPtr->connectionId];

            for (uint32 i = 0; i < request.numMoves; ++i)
            {
                const PlayerMove& move = request.moves[i];

                // Ignore duplicates/retransmits and stale stragglers
                if (move.moveId <= queue.lastQueuedMoveId)
                {
                    continue;
                }

                queue.moves.PushBack(move);
                queue.lastQueuedMoveId = move.moveId;
            }

            // cap it
            CapArray(queue.moves, PlayerMoveQueueState::MaxQueuedMoves);

            break;
        }
        }
    }
}

void ReplicationSystem::ProcessPlayerMoves(float delta)
{
    if (m_playerMoveQueues.Empty())
    {
        return;
    }

    struct TargetedMoveAck
    {
        net::NetConnectionId connectionId;
        NetId netId;
        net::NetBuffer payload;
    };

    Array<TargetedMoveAck, SceneTempAllocator> targetedAcks;

    for (auto it = m_playerMoveQueues.Begin(); it != m_playerMoveQueues.End(); ++it)
    {
        PlayerMoveQueueState& queue = it->second;

        queue.timeBudget = MathUtil::Min(queue.timeBudget + MathUtil::Max(delta, 0.0f) * PlayerMoveTimeBudgetSlack, PlayerMoveQueueState::MaxTimeBudget);

        if (queue.moves.Empty())
        {
            continue;
        }

        auto entityIt = m_connectionIdToEntity.Find(it->first);

        Entity* entity = entityIt != m_connectionIdToEntity.End() ? entityIt->second : nullptr;

        CharacterControllerComponent* component = entity != nullptr
            ? entity->TryGetComponent<CharacterControllerComponent>()
            : nullptr;

        const ReplicationStateComponent* replicationState = entity != nullptr
            ? entity->TryGetComponent<ReplicationStateComponent>()
            : nullptr;

        if (component == nullptr || !component->physicsHandle || replicationState == nullptr)
        {
            // Player entity (or its controller) is gone -- drop pending moves.
            queue.moves.Clear();

            continue;
        }

        const uint32 numToProcess = uint32(queue.moves.Size());

        // MoveCharacter leaves the entity at its result, so this is the right answer if every move gets dropped
        Vec3f resultTranslation = entity->GetWorldTranslation();
        uint32 lastProcessedMoveId = 0;
        uint32 numDroppedMoves = 0;

        for (uint32 i = 0; i < numToProcess; ++i)
        {
            const PlayerMove& move = queue.moves[i];

            // dropped moves still get acked, the client reconciles against the unmoved position
            lastProcessedMoveId = move.moveId;

            const float moveCost = MathUtil::Clamp(move.deltaTime, 0.0f, MaxPlayerMoveDeltaTime);

            if (moveCost > queue.timeBudget)
            {
                numDroppedMoves++;

                continue;
            }

            queue.timeBudget -= moveCost;

            SceneHelpers::MoveCharacter(entity, *component, move, resultTranslation);
        }

        if (numDroppedMoves != 0)
        {
            HYP_LOG(Replication, Debug, "Dropped {} player moves from connection {}: move time budget exceeded",
                numDroppedMoves, uint32(it->first));
        }

        queue.moves.Erase(queue.moves.Begin(), queue.moves.Begin() + numToProcess);

        net::NetBuffer payload;
        MemoryByteWriter<NetAllocator, 1> writer(&payload);

        SerializePlayerMoveAck(writer, PlayerMoveAck { resultTranslation, lastProcessedMoveId });

        targetedAcks.PushBack(TargetedMoveAck { it->first, replicationState->netId, std::move(payload) });
    }

    if (!targetedAcks.Any())
    {
        return;
    }

    GetGameServerThread()->GetScheduler().Enqueue(
        [targetedAcks = Array<TargetedMoveAck, NetAllocator>(targetedAcks)]()
        {
            net::NetServer& netServer = g_gameServer->GetNetServer();

            for (const TargetedMoveAck& targeted : targetedAcks)
            {
                netServer.SendMessageTo(
                    targeted.connectionId,
                    NetMessageId::PlayerMoveAck,
                    NetChannelMode::UnreliableOrdered,
                    NetStreamKey(uint32(targeted.netId)),
                    targeted.payload.ToByteView());
            }
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

void ReplicationSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    ApplyPendingRequests();

    ProcessPlayerMoves(delta);

    Array<net::NetConnectionId, SceneTempAllocator> newConnections;
    g_gameServer->DrainNewConnections(newConnections);

    for (net::NetConnectionId connectionId : newConnections)
    {
        // a fresh client knows nothing, everything in range gets spawned for it below
        m_knownEntities.Erase(connectionId);
    }

    Array<PlayerPosition, SceneTempAllocator> playerPositions = CollectPlayerPositions(scenes, this);

    ProcessInterestSpawns(scenes, playerPositions);

    Array<Entity*, SceneTempAllocator> updatedEntities;
    Array<TargetedSnapshot, SceneTempAllocator> targetedSnapshots;

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        EntityManager* entityManager = scene->GetEntityManager();

        targetedSnapshots.Resize(0);
        updatedEntities.Resize(0);

        for (auto [entity, replicationState] : entityManager->GetEntitySet<ReplicationStateComponent>())
        {
            const RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>();

            const bool isSleeping = rigidBodyComponent != nullptr
                && rigidBodyComponent->rigidBody.IsValid()
                && rigidBodyComponent->rigidBody->isSleeping;

            if (isSleeping != replicationState.isSleepingReplicated)
            {
                entity->AddTag<EntityTag::UpdateReplication>();
            }
        }

        for (auto [entity, replicationState, _] : entityManager->GetEntitySet<ReplicationStateComponent, TagComponent<EntityTag::UpdateReplication>>())
        {
            const NetId netId = replicationState.netId;
            const Transform& transform = entity->GetLocalTransform();

            Vec3f linearVelocity = Vec3f::Zero();
            Vec3f angularVelocity = Vec3f::Zero();
            
            bool isSleeping = false;

            if (const RigidBodyComponent* rigidBodyComponent = entity->TryGetComponent<RigidBodyComponent>())
            {
                if (rigidBodyComponent->rigidBody.IsValid())
                {
                    isSleeping = rigidBodyComponent->rigidBody->isSleeping;

                    linearVelocity = isSleeping ? Vec3f::Zero() : rigidBodyComponent->rigidBody->GetVelocity();
                    angularVelocity = isSleeping ? Vec3f::Zero() : rigidBodyComponent->rigidBody->GetAngularVelocity();
                }
            }

            // Stamp with the server's monotonic simulation time (ms) so clients interpolate on the
            // server timeline rather than on packet arrival times (which jitter).
            const uint64 serverTimeMs = uint64(GetWorld()->GetGameState().gameTime * 1000.0f);

            net::NetBuffer payload;
            MemoryByteWriter<NetAllocator, 1> writer(&payload);
            writer.Write(transform.GetTranslation());
            writer.Write(transform.GetRotation());
            writer.Write(transform.GetScale());
            writer.Write(linearVelocity);
            writer.Write(angularVelocity);
            writer.Write(serverTimeMs);
            writer.Write(uint8(isSleeping));

            replicationState.isSleepingReplicated = uint8(isSleeping);

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

void ReplicationSystem::ProcessInterestSpawns(Span<const Handle<Scene>> scenes, const Array<PlayerPosition, SceneTempAllocator>& playerPositions)
{
    ThreadBase* gameServerThread = GetGameServerThread();

    // nothing can be sent without the server thread, so don't mark anything as known either
    if (playerPositions.Empty() || gameServerThread == nullptr)
    {
        return;
    }

    struct TargetedSpawn
    {
        net::NetConnectionId connectionId;
        ReplicationSnapshot spawn;
    };

    // insert every set up front so the pointers stay put while we scan
    for (const auto& [connectionId, playerPosition] : playerPositions)
    {
        m_knownEntities[connectionId];
    }

    Array<Set<NetId, SceneAllocator>*, SceneTempAllocator> knownSets;
    knownSets.Reserve(playerPositions.Size());

    for (const auto& [connectionId, playerPosition] : playerPositions)
    {
        knownSets.PushBack(&m_knownEntities[connectionId]);
    }

    Array<TargetedSpawn, SceneTempAllocator> targetedSpawns;

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        EntityManager* entityManager = scene->GetEntityManager();

        for (auto [entity, replicationState] : entityManager->GetEntitySet<ReplicationStateComponent>())
        {
            const Vec3f entityPosition = entity->GetWorldTranslation();

            Optional<net::NetBuffer> spawnPayload;

            for (size_t i = 0; i < playerPositions.Size(); ++i)
            {
                const auto& [connectionId, playerPosition] = playerPositions[i];

                if (entityPosition.DistanceSquared(playerPosition) > ReplicationInterestRadiusSq)
                {
                    continue;
                }

                if (!knownSets[i]->Insert(replicationState.netId).second)
                {
                    continue;
                }

                if (!spawnPayload.HasValue())
                {
                    spawnPayload = SerializeEntitySpawnPayload(entity);
                }

                targetedSpawns.PushBack(TargetedSpawn { connectionId, ReplicationSnapshot { replicationState.netId, *spawnPayload } });
            }
        }
    }

    if (!targetedSpawns.Any())
    {
        return;
    }

    HYP_LOG(Replication, Debug, "Sending EntitySpawn for {} (connection, entity) pairs entering interest range", targetedSpawns.Size());

    gameServerThread->GetScheduler().Enqueue(
        [targetedSpawns = Array<TargetedSpawn, NetAllocator>(targetedSpawns)]()
        {
            net::NetServer& netServer = g_gameServer->GetNetServer();

            for (const TargetedSpawn& targeted : targetedSpawns)
            {
                netServer.SendMessageTo(
                    targeted.connectionId,
                    NetMessageId::EntitySpawn,
                    NetChannelMode::ReliableOrdered,
                    NetStreamKey(uint32(targeted.spawn.netId)),
                    targeted.spawn.payload.ToByteView());
            }
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

} // namespace Hyperion
