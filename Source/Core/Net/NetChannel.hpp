/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/Map.hpp>
#include <Core/Containers/FlatMap.hpp>
#include <Core/Containers/Array.hpp>

#include <Core/Utilities/Time.hpp>

#include <Core/Net/NetMessage.hpp>
#include <Core/Net/NetMemory.hpp>

#include <type_traits>

namespace Hyperion {
namespace net {

class NetChannel final
{
    struct PendingReliableMessage
    {
        NetMessageId messageId;
        Array<uint8, NetAllocator> payload;
        Time lastSentTime;
        uint32 resendCount;
    };

    struct OutgoingState
    {
        uint32 nextSequence = 1;
        FlatMap<uint32, PendingReliableMessage, NetAllocator> unacked;
    };

    struct IncomingState
    {
        uint32 nextExpectedSequence = 1;    // ReliableOrdered
        uint32 lastAcceptedSequence = 0;    // UnreliableOrdered
        FlatMap<uint32, Array<uint8, NetAllocator>, NetAllocator> outOfOrderBuffer; // ReliableOrdered only
    };

    struct StreamState
    {
        OutgoingState outgoing;
        IncomingState incoming;
    };

public:
    explicit NetChannel(NetChannelMode mode)
        : m_mode(mode)
    {
    }

    HYP_FORCE_INLINE NetChannelMode GetMode() const
    {
        return m_mode;
    }

    void Send(NetSocketUDP& socket, const NetAddress& destAddr, const NetMessage& message)
    {

    }

    void OnAck(NetStreamKey key, uint32 sequence)
    {
        auto it = m_streams.Find(key);

        if (it == m_streams.End())
        {
            return;
        }

        StreamState& stream = *it->second;

        // erase all messages with sequence <= the acked sequence
        stream.outgoing.unacked.Erase(stream.outgoing.unacked.Begin(), stream.outgoing.unacked.UpperBound(sequence));
    }

private:
    using StreamsMap = Map<NetStreamKey, UniquePtr<StreamState, NetAllocator>, NetAllocator>;

    const NetChannelMode m_mode;
    StreamsMap m_streams;
};

} // namespace net
} // namespace Hyperion
