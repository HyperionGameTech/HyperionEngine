/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Net/NetMemory.hpp>
#include <Core/Net/NetSocketUDP.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Utilities/Result.hpp>

#include <Core/Containers/Map.hpp>

namespace Hyperion {
namespace net {

enum class NetConnectionId : uint32;
class NetConnection;

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

private:
    NetSocketUDP m_socket;
    Map<NetAddress, NetConnectionId, NetAllocator> m_addrToConnectionId;
    Map<NetConnectionId, UniquePtr<NetConnection, NetAllocator>, NetAllocator> m_addrToConnection;
};

} // namespace net

using net::NetServer;

} // namespace Hyperion
