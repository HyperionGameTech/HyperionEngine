/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Net/NetChannel.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Logging/Logger.hpp>

namespace Hyperion {
namespace net {

static constexpr uint32 BaseResendIntervalMs = 150;
static constexpr uint32 MaxResendIntervalMs = 2400;
static constexpr uint32 MaxBackoffShift = 4; // 150 << 4 == 2400

static constexpr uint32 ResendWarnThreshold = 10;

namespace {

struct PendingReliableMessage
{
    NetMessageId messageId;
    NetBuffer payload;
    Time lastSentTime;
    uint32 resendCount;
};

struct OutgoingState
{
    uint32 nextSequence = 1;
    FlatMap<uint32, PendingReliableMessage, NetAllocator> unackedReliable;
};

struct BufferedMessage
{
    NetMessageId messageId;
    NetBuffer payload;
};

struct IncomingState
{
    uint32 nextExpectedSequence = 1;    // ReliableOrdered
    uint32 lastAcceptedSequence = 0;    // UnreliableOrdered
    FlatMap<uint32, BufferedMessage, NetAllocator> outOfOrderBuffer; // ReliableOrdered only
};

} // anonymous namespace

struct StreamState
{
    OutgoingState outgoing;
    IncomingState incoming;
};

struct StreamStateMap : Map<NetStreamKey, UniquePtr<StreamState, NetAllocator>, NetAllocator>
{
};

NetChannel::NetChannel(NetChannelMode mode)
    : m_mode(mode),
      m_streams(MakePimplWithAllocator<StreamStateMap, NetAllocator>())
{
}

NetChannel::~NetChannel() = default;

void NetChannel::Send(NetSocketUDP& socket, const NetAddress& destAddr, const NetMessage& message)
{
    StreamState& stream = GetOrCreateStream(message.key);

    NetMessageHeader header {
        CurrentProtocolVersion,
        m_mode,
        message.messageId,
        message.key,
        stream.outgoing.nextSequence++
    };

    ValueStorage<MemoryByteWriter<NetAllocator, 1>> writerMem;

    if (IsReliable(m_mode))
    {
        auto insertResult = stream.outgoing.unackedReliable.Set(
            header.sequence,
            PendingReliableMessage { message.messageId, {}, Time::Now(), 0 });

        NetBuffer& buffer = insertResult.first->second.payload;
        writerMem.Construct(&buffer);
    }
    else
    {
        writerMem.Construct(&m_tempBuffer);
        writerMem.Get().Seek(0, /* truncate */ false);
    }
    
    MemoryByteWriter<NetAllocator, 1>& writer = writerMem.Get();

    header.Serialize(writer);
    writer.Write(message.payload);

    socket.SendTo(destAddr, writer.GetBuffer().ToByteView().Slice(0, writer.Position()));
}

void NetChannel::HandleIncoming(
    NetSocketUDP& socket,
    const NetAddress& srcAddr,
    const NetMessageHeader& header,
    ConstByteView payload,
    const ProcRef<void(NetMessageId, ConstByteView)>& dispatch)
{
    Assert(dispatch);

    StreamState& stream = GetOrCreateStream(header.key);

    const NetMessageId messageId = NetMessageId(header.messageId);

    switch (m_mode)
    {
    case NetChannelMode::UnreliableUnordered:
    {
        dispatch(messageId, payload);

        return;
    }
    case NetChannelMode::UnreliableOrdered:
    {
        if (!IsMoreRecent(header.sequence, stream.incoming.lastAcceptedSequence))
        {
            return;
        }

        stream.incoming.lastAcceptedSequence = header.sequence;
        dispatch(messageId, payload);

        return;
    }
    case NetChannelMode::ReliableUnordered:
    {
        SendAck(socket, srcAddr, header.key, header.sequence);
        dispatch(messageId, payload);

        return;
    }
    case NetChannelMode::ReliableOrdered:
    {
        if (header.sequence < stream.incoming.nextExpectedSequence)
        {
            SendAck(socket, srcAddr, header.key, stream.incoming.nextExpectedSequence - 1);

            return;
        }

        if (header.sequence > stream.incoming.nextExpectedSequence)
        {
            stream.incoming.outOfOrderBuffer.Set(header.sequence,
                BufferedMessage { messageId, NetBuffer(payload.Size(), payload.Data()) });

            SendAck(socket, srcAddr, header.key, stream.incoming.nextExpectedSequence - 1);

            return;
        }

        stream.incoming.nextExpectedSequence++;
        dispatch(messageId, payload);

        for (;;)
        {
            auto it = stream.incoming.outOfOrderBuffer.Find(stream.incoming.nextExpectedSequence);

            if (it == stream.incoming.outOfOrderBuffer.End())
            {
                break;
            }

            BufferedMessage buffered = std::move(it->second);
            stream.incoming.outOfOrderBuffer.Erase(it);

            stream.incoming.nextExpectedSequence++;
            dispatch(buffered.messageId, buffered.payload.ToByteView());
        }

        SendAck(socket, srcAddr, header.key, stream.incoming.nextExpectedSequence - 1);

        return;
    }
    default:
        HYP_UNREACHABLE();
    }
}

void NetChannel::Update(NetSocketUDP& socket, const NetAddress& destAddr)
{
    if (!IsReliable(m_mode))
    {
        return;
    }

    const Time now = Time::Now();

    for (auto streamIt = m_streams->Begin(); streamIt != m_streams->End(); ++streamIt)
    {
        StreamState& stream = *streamIt->second;

        for (auto pendingIt = stream.outgoing.unackedReliable.Begin(); pendingIt != stream.outgoing.unackedReliable.End(); ++pendingIt)
        {
            PendingReliableMessage& pending = pendingIt->second;

            const uint32 backoffMs = BaseResendIntervalMs << MathUtil::Min(pending.resendCount, MaxBackoffShift);
            const TimeDiff interval = TimeDiff(MathUtil::Min(backoffMs, MaxResendIntervalMs));

            if (now - pending.lastSentTime < interval)
            {
                continue;
            }

            socket.SendTo(destAddr, pending.payload.ToByteView());

            pending.lastSentTime = now;
            pending.resendCount++;

            if (pending.resendCount == ResendWarnThreshold)
            {
                HYP_LOG(Net, Warning, "Message (id={}) on stream {} has not been acked after {} resends",
                    uint16(pending.messageId), streamIt->first, pending.resendCount);
            }
        }
    }
}

void NetChannel::OnAck(NetStreamKey key, uint32 sequence)
{
    auto it = m_streams->Find(key);

    if (it == m_streams->End())
    {
        return;
    }

    StreamState& stream = *it->second;

    // erase all messages with sequence <= the acked sequence
    stream.outgoing.unackedReliable.Erase(
        stream.outgoing.unackedReliable.Begin(),
        stream.outgoing.unackedReliable.UpperBound(sequence));
}

StreamState& NetChannel::GetOrCreateStream(NetStreamKey key)
{
    auto it = m_streams->Find(key);

    if (it == m_streams->End())
    {
        it = m_streams->Set(key, MakeUniqueWithAllocator<StreamState, NetAllocator>()).first;
    }

    return *it->second;
}

void NetChannel::SendAck(NetSocketUDP& socket, const NetAddress& destAddr, NetStreamKey key, uint32 sequence)
{
    NetMessageHeader ackHeader {
        CurrentProtocolVersion,
        NetChannelMode::UnreliableUnordered,
        NetMessageId::Ack,
        key,
        sequence
    };

    MemoryByteWriter<NetAllocator, 1> writer(&m_tempBuffer);
    writer.Seek(0, /* truncate */ false);

    ackHeader.Serialize(writer);

    socket.SendTo(destAddr, m_tempBuffer.ToByteView().Slice(0, writer.Position()));
}

} // namespace net
} // namespace Hyperion
