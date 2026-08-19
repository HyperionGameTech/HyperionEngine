/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Framework/Client/ClientReplicationManager.hpp>

#include <Net/NetClient.hpp>

#include <Core/IO/ByteReader.hpp>

namespace Hyperion {

using net::NetAllocator;
using net::NetMessageContext;
using net::NetMessageId;

static Transform ReadTransform(ConstByteView payload)
{
    MemoryByteReader reader(payload);

    Vec3f translation;
    Quat4f rotation;
    Vec3f scale;

    reader.Read(&translation, sizeof(Vec3f));
    reader.Read(&rotation, sizeof(Quat4f));
    reader.Read(&scale, sizeof(Vec3f));

    return Transform(translation, scale, rotation);
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
            PushOp(ReplicationOp<ReplicationOpType::Snapshot>(
                NetId(uint32(context.key)),
                ReadTransform(payload)));
        });
}

} // namespace Hyperion
