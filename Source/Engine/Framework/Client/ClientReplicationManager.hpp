/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Threading/AtomicVar.hpp>

#include <Core/Math/Transform.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Utilities/ByteUtil.hpp>

#include <Net/NetMessage.hpp>
#include <Net/NetMessageDispatcher.hpp>
#include <Net/NetMemory.hpp>

namespace Hyperion {

namespace net {
class NetClient;
} // namespace net

enum class NetId : uint32;

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
    Name sceneName;
    Transform transform;

    ReplicationOp(NetId netId, Name sceneName, const Transform& transform)
        : ReplicationOpBase(ReplicationOpType::Spawn, netId),
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
    ClientReplicationManager()
        : m_writeIndex(0),
          m_readIndex(1),
          m_readyIndex(2)
    {
    }

    void RegisterHandlers(net::NetClient& netClient);

    void PublishBatch()
    {
        m_writeIndex = m_readyIndex.Exchange(m_writeIndex, MemoryOrder::ACQUIRE_RELEASE);
    }

    template <class AllocatorType>
    void DrainPendingOps(Array<ReplicationOpBase*, AllocatorType>& outOps)
    {
        PendingOps& previous = m_pendingOps[m_readIndex];
        previous.buffer.SetSize(0);
        previous.startOffsets.Resize(0);
        previous.writeOffset = 0;

        m_readIndex = m_readyIndex.Exchange(m_readIndex, MemoryOrder::ACQUIRE_RELEASE);

        PendingOps& pendingOps = m_pendingOps[m_readIndex];

        outOps.Reserve(outOps.Size() + pendingOps.startOffsets.Size());

        for (size_t startOffset : pendingOps.startOffsets)
        {
            outOps.PushBack(reinterpret_cast<ReplicationOpBase*>(pendingOps.buffer.Data() + startOffset));
        }
    }

private:
    using PendingOpsBuffer = memory::ByteBuffer<net::NetAllocator>;

    template <ReplicationOpType OpType>
    void PushOp(const ReplicationOp<OpType>& op)
    {
        PendingOps& pendingOps = m_pendingOps[m_writeIndex];

        const size_t alignedOffset = ByteUtil::AlignAs(pendingOps.writeOffset, alignof(ReplicationOp<OpType>));

        if (pendingOps.buffer.Size() < alignedOffset + sizeof(ReplicationOp<OpType>))
        {
            pendingOps.buffer.SetSize(MathUtil::NextPowerOf2(alignedOffset + sizeof(ReplicationOp<OpType>)));
        }

        ReplicationOp<OpType>* newOp = reinterpret_cast<ReplicationOp<OpType>*>(pendingOps.buffer.Data() + alignedOffset);
        new (newOp) ReplicationOp<OpType>(op);

        pendingOps.writeOffset = alignedOffset + sizeof(ReplicationOp<OpType>);
        pendingOps.startOffsets.PushBack(alignedOffset);
    }

    struct PendingOps
    {
        PendingOpsBuffer buffer;
        Array<size_t, net::NetAllocator> startOffsets;
        size_t writeOffset = 0;
    };

    PendingOps m_pendingOps[3];

    uint32 m_writeIndex;    // GameClientThread only
    uint32 m_readIndex;     // SimThread only
    AtomicVar<uint32> m_readyIndex;
};

} // namespace Hyperion
