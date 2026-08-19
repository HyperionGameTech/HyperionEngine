/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Math/Transform.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Utilities/Uuid.hpp>

#include <Net/NetMessage.hpp>
#include <Net/NetMessageDispatcher.hpp>
#include <Net/NetMemory.hpp>

#include <Framework/Net/ReplicationQueue.hpp>
#include <Framework/Net/NetId.hpp>

namespace Hyperion {

namespace net {
class NetClient;
enum class NetConnectionId : uint32;
} // namespace net

enum class ReplicationOpType : uint8
{
    Spawn,
    Despawn,
    Snapshot
};

// ALL derived types must be trivially destructible, will be allocated using arena/transient allocators
//  with NO destructor call.
template <ReplicationOpType>
struct ReplicationOp;

struct ReplicationOpBase
{
    ReplicationOpType type;
    NetId netId;

    ReplicationOpBase(ReplicationOpType type, NetId netId)
        : type(type),
          netId(netId)
    {
    }
};

template <>
struct ReplicationOp<ReplicationOpType::Spawn> final : ReplicationOpBase
{
    TypeId typeId;
    NetId parentNetId; // InvalidNetId if unparented (or parent isn't itself replicated)
    UUID uuid; // the entity's persistent Node UUID -- used to correlate against an already-loaded local entity
    net::NetConnectionId ownerConnectionId; // 0 unless this is a player entity clone
    Name name; // the entity name
    Name sceneName;
    Transform transform;

    ReplicationOp(NetId netId, TypeId typeId, Name name, NetId parentNetId, UUID uuid, net::NetConnectionId ownerConnectionId, Name sceneName, const Transform& transform)
        : ReplicationOpBase(ReplicationOpType::Spawn, netId),
          typeId(typeId),
          parentNetId(parentNetId),
          uuid(uuid),
          ownerConnectionId(ownerConnectionId),
          name(name),
          sceneName(sceneName),
          transform(transform)
    {
    }
};

template <>
struct ReplicationOp<ReplicationOpType::Despawn> final : ReplicationOpBase
{
    explicit ReplicationOp(NetId netId)
        : ReplicationOpBase(ReplicationOpType::Despawn, netId)
    {
    }
};

template <>
struct ReplicationOp<ReplicationOpType::Snapshot> final : ReplicationOpBase
{
    Transform transform;

    ReplicationOp(NetId netId, const Transform& transform)
        : ReplicationOpBase(ReplicationOpType::Snapshot, netId),
          transform(transform)
    {
    }
};

// Must all be trivial as they won't be destructed!
static_assert(std::is_trivially_destructible_v<ReplicationOp<ReplicationOpType::Spawn>>);
static_assert(std::is_trivially_destructible_v<ReplicationOp<ReplicationOpType::Despawn>>);
static_assert(std::is_trivially_destructible_v<ReplicationOp<ReplicationOpType::Snapshot>>);

class ENGINE_API ClientReplicationManager
{
public:
    void RegisterHandlers(net::NetClient& netClient);

    void PublishBatch()
    {
        m_queue.PublishBatch();
    }

    template <class AllocatorType>
    void DrainPendingOps(Array<ReplicationOpBase*, AllocatorType>& outOps)
    {
        m_queue.DrainPending(outOps);
    }

private:
    template <ReplicationOpType OpType>
    void PushOp(const ReplicationOp<OpType>& op)
    {
        m_queue.Push(op);
    }

    ReplicationQueue<ReplicationOpBase> m_queue;
};

} // namespace Hyperion
