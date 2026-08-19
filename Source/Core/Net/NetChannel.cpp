/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Net/NetChannel.hpp>

namespace Hyperion {
namespace net {

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

struct IncomingState
{
    uint32 nextExpectedSequence = 1;    // ReliableOrdered
    uint32 lastAcceptedSequence = 0;    // UnreliableOrdered
    FlatMap<uint32, NetBuffer, NetAllocator> outOfOrderBuffer; // ReliableOrdered only
};

} // anonymous namespace

struct StreamState
{
    OutgoingState outgoing;
    IncomingState incoming;
};

NetChannel::~NetChannel() = default;

void NetChannel::Send(NetSocketUDP& socket, const NetAddress& destAddr, const NetMessage& message)
{
    StreamState& stream = GetOrCreateStream(message.key);

    NetMessageHeader header {
        CurrentProtocolVersion,
        uint8(m_mode),
        uint16(message.messageId),
        stream.outgoing.nextSequence++,
        message.key
    };

    ValueStorage<MemoryByteWriter<NetAllocator>> writerMem;

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
        
        // keep the same size buffer to reduce allocations.
        writerMem.Get().Seek(0, /* truncate */ false);
    }
    
    MemoryByteWriter<NetAllocator>& writer = writerMem.Get();

    header.Serialize(writer);
    writer.Write(message.payload);

    socket.SendTo(destAddr, m_tempBuffer.ToByteView().Slice(0, writer.Position()));
}

void NetChannel::OnAck(NetStreamKey key, uint32 sequence)
{
    auto it = m_streams.Find(key);

    if (it == m_streams.End())
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
    auto it = m_streams.Find(key);

    if (it == m_streams.End())
    {
        it = m_streams.Set(key, MakeUniqueWithAllocator<StreamState, NetAllocator>()).first;
    }

    return *it->second;
}

} // namespace net
} // namespace Hyperion
