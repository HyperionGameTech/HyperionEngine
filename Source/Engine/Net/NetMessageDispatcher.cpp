/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
*/

#include <Net/NetMessageDispatcher.hpp>
#include <Net/NetChannel.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {
namespace net {

HYP_DEFINE_LOG_CHANNEL(Net);

void NetMessageDispatcher::RegisterHandler(NetMessageId messageId, NetMessageHandler&& handler)
{
    m_handlers.Set(messageId, std::move(handler));
}

void NetMessageDispatcher::Dispatch(
    NetSocketUDP& socket,
    const NetAddress& srcAddr,
    NetConnectionId connectionId,
    NetChannel& reliableChannel,
    NetChannel& unreliableChannel,
    ConstByteView datagram)
{
    MemoryByteReader reader { datagram };

    NetMessageHeader header;
    header.Deserialize(reader);

    if (header.protocolVersion != CurrentProtocolVersion)
    {
        HYP_LOG(Net, Warning, "Mismatched protocol version {} (expected {})",
            srcAddr.ToString(), header.protocolVersion, CurrentProtocolVersion);

        return;
    }

    const ConstByteView payload = datagram.Slice(reader.Position());

    if (header.messageId == NetMessageId::Ack)
    {
        reliableChannel.OnAck(header.key, header.sequence);

        return;
    }

    NetChannel& channel = IsReliable(header.channelMode) ? reliableChannel : unreliableChannel;

    auto dispatchFn = [this, &srcAddr, connectionId, streamKey = header.key](NetMessageId messageId, ConstByteView messagePayload)
    {
        auto it = m_handlers.Find(messageId);

        if (it == m_handlers.End())
        {
            HYP_LOG(Net, Warning, "No handler registered for NetMessageId: {}", uint16(messageId));

            return;
        }

        it->second(NetMessageContext { srcAddr, connectionId, streamKey }, messagePayload);
    };

    channel.HandleIncoming(socket, srcAddr, header, payload, dispatchFn);
}

} // namespace net
} // namespace Hyperion
