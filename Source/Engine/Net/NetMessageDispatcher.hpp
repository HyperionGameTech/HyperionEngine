/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Functional/Proc.hpp>

#include <Net/NetMessage.hpp>
#include <Net/NetMemory.hpp>
#include <Net/NetAddress.hpp>
#include <Net/NetSocketUDP.hpp>

namespace Hyperion {
namespace net {

class NetChannel;

enum class NetConnectionId : uint32;

struct NetMessageContext
{
    NetAddress senderAddress;
    NetConnectionId connectionId;
    NetStreamKey key;
};

using NetMessageHandler = Proc<void(const NetMessageContext&, ConstByteView)>;

class NET_API NetMessageDispatcher
{
public:
    NetMessageDispatcher() = default;

    NetMessageDispatcher(const NetMessageDispatcher&) = delete;
    NetMessageDispatcher& operator=(const NetMessageDispatcher&) = delete;

    void RegisterHandler(NetMessageId messageId, NetMessageHandler&& handler);

    void Dispatch(
        NetSocketUDP& socket,
        const NetAddress& srcAddr,
        NetConnectionId connectionId,
        NetChannel& reliableChannel,
        NetChannel& unreliableChannel,
        ConstByteView datagram);

private:
    Map<NetMessageId, NetMessageHandler, NetAllocator> m_handlers;
};

} // namespace net
} // namespace Hyperion
