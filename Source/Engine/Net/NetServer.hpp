/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Net/NetMemory.hpp>
#include <Net/NetSocketUDP.hpp>
#include <Net/NetAddress.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Utilities/Result.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Functional/Delegate.hpp>

namespace Hyperion {
namespace net {

enum class NetConnectionId : uint32;
class NetConnection;

struct NetClientConnectedData
{
    NetConnectionId connectionId;
    NetAddress address;
};

struct NetClientDisconnectedData
{
    NetConnectionId connectionId;
    NetAddress address;
};

class CORE_API NetServer
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

    Delegate<void, const NetClientConnectedData&> OnClientConnected;
    Delegate<void, const NetClientDisconnectedData&> OnClientDisconnected;

private:
    NetSocketUDP m_socket;
    Map<NetAddress, NetConnectionId, NetAllocator> m_addrToConnectionId;
    Map<NetConnectionId, UniquePtr<NetConnection, NetAllocator>, NetAllocator> m_connections;
    uint32 m_nextConnectionId;
};

} // namespace net

using net::NetServer;
using net::NetClientConnectedData;
using net::NetClientDisconnectedData;

} // namespace Hyperion
