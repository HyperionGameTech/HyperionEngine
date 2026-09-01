/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Framework/Client/ClientReplicationManager.hpp>

#include <Net/NetClient.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Core/Utilities/Time.hpp>

namespace Hyperion {

using net::NetAllocator;
using net::NetMessageContext;
using net::NetMessageId;

static void ReadSnapshotPayload(
    ConstByteView payload,
    Transform& outTransform,
    Vec3f& outVelocity,
    Vec3f& outAngularVelocity,
    Time& outServerTimeMs,
    bool& outIsSleeping)
{
    MemoryByteReader reader(payload);

    Vec3f translation;
    Quat4f rotation;
    Vec3f scale;
    Vec3f velocity;
    Vec3f angularVelocity;
    uint64 serverTimeMs = 0;
    uint8 isSleeping = 0;

    reader.Read(&translation, sizeof(Vec3f));
    reader.Read(&rotation, sizeof(Quat4f));
    reader.Read(&scale, sizeof(Vec3f));
    reader.Read(&velocity, sizeof(Vec3f));
    reader.Read(&angularVelocity, sizeof(Vec3f));
    reader.Read(&serverTimeMs, sizeof(uint64));
    reader.Read(&isSleeping, sizeof(uint8));

    outTransform = Transform(translation, scale, rotation);
    outVelocity = velocity;
    outAngularVelocity = angularVelocity;
    outServerTimeMs = Time(serverTimeMs);
    outIsSleeping = (isSleeping != 0);
}

void ClientReplicationManager::RegisterHandlers(net::NetClient& netClient)
{
    netClient.RegisterHandler(NetMessageId::EntitySpawn,
        [this](const NetMessageContext& context, ConstByteView payload)
        {
            MemoryByteReader reader { payload };

            TypeId typeId;
            NetId parentNetId;
            UUID uuid;
            net::NetConnectionId ownerConnectionId;
            Name entityName;
            Name sceneName;
            Vec3f translation;
            Quat4f rotation;
            Vec3f scale;

            reader.Read(&typeId, sizeof(TypeId));
            reader.Read(&parentNetId, sizeof(NetId));
            reader.Read(&uuid, sizeof(UUID));
            reader.Read(&ownerConnectionId, sizeof(net::NetConnectionId));
            reader.Read(&entityName, sizeof(Name));
            reader.Read(&sceneName, sizeof(Name));
            reader.Read(&translation, sizeof(Vec3f));
            reader.Read(&rotation, sizeof(Quat4f));
            reader.Read(&scale, sizeof(Vec3f));

            PushOp(ReplicationOp<ReplicationOpType::Spawn>(
                NetId(uint32(context.key)),
                typeId,
                entityName,
                parentNetId,
                uuid,
                ownerConnectionId,
                sceneName,
                Transform(translation, scale, rotation)));
        });

    netClient.RegisterHandler(NetMessageId::EntityDespawn,
        [this](const NetMessageContext& context, ConstByteView payload)
        {
            PushOp(ReplicationOp<ReplicationOpType::Despawn>(NetId(uint32(context.key))));
        });

    netClient.RegisterHandler(NetMessageId::ComponentSnapshot,
        [this](const NetMessageContext& context, ConstByteView payload)
        {
            Transform transform;
            Vec3f velocity;
            Vec3f angularVelocity;
            Time serverTimeMs;
            bool isSleeping;

            ReadSnapshotPayload(payload, transform, velocity, angularVelocity, serverTimeMs, isSleeping);

            PushOp(ReplicationOp<ReplicationOpType::Snapshot>(
                NetId(uint32(context.key)),
                transform,
                velocity,
                angularVelocity,
                Time::Now(),
                serverTimeMs,
                isSleeping));
        });

    netClient.RegisterHandler(NetMessageId::PlayerMoveAck,
        [this](const NetMessageContext&, ConstByteView payload)
        {
            MemoryByteReader reader { payload };

            m_moveAckQueue.Push(DeserializePlayerMoveAck(reader));
        });
}

} // namespace Hyperion
