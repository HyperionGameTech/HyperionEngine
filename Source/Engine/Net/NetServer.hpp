/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Net/NetMemory.hpp>
#include <Net/NetSocketUDP.hpp>
#include <Net/NetAddress.hpp>
#include <Net/NetMessage.hpp>
#include <Net/NetMessageDispatcher.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Utilities/Result.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Functional/Delegate.hpp>

namespace Hyperion {
namespace net {

enum class NetConnectionId : uint32;
class NetConnection;

struct NetServerConnectionStateChangedData
{
    NetConnectionId connectionId;
    NetAddress address;
};

class NET_API NetServer
{
public:
    NetServer();

    NetServer(const NetServer &other) = delete;
    NetServer &operator=(const NetServer &other) = delete;

    ~NetServer();

    Result Listen(uint16 port);

    bool IsListening() const;
    void StopListening();

    void Update();

    void RegisterHandler(NetMessageId messageId, NetMessageHandler&& handler);

    void SendMessageTo(NetConnectionId connectionId, NetMessageId messageId, NetChannelMode mode, NetStreamKey key, ConstByteView payload);
    void Broadcast(NetMessageId messageId, NetChannelMode mode, NetStreamKey key, ConstByteView payload);

    Delegate<void, NetServerConnectionStateChangedData> OnClientConnected;
    Delegate<void, NetServerConnectionStateChangedData> OnClientDisconnected;

private:
    NetSocketUDP m_socket;
    Map<NetAddress, NetConnectionId, NetAllocator> m_addrToConnectionId;
    Map<NetConnectionId, UniquePtr<NetConnection, NetAllocator>, NetAllocator> m_connections;
    uint32 m_nextConnectionId;
    NetMessageDispatcher m_dispatcher;
};

} // namespace net

using net::NetServer;
using net::NetServerConnectionStateChangedData;
using net::NetServerConnectionStateChangedData;

} // namespace Hyperion
