/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Net/NetMemory.hpp>
#include <Core/Net/NetSocketUDP.hpp>
#include <Core/Net/NetAddress.hpp>

#include <Core/Utilities/Result.hpp>
#include <Core/Utilities/Time.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Containers/String.hpp>

namespace Hyperion {
namespace net {

struct NetServerDisconnectedData
{
    NetAddress serverAddress;
};

class CORE_API NetClient
{
public:
    NetClient();

    NetClient(const NetClient& other) = delete;
    NetClient& operator=(const NetClient& other) = delete;

    ~NetClient();

    bool IsConnected() const
    {
        return m_isConnected;
    }

    const NetAddress& GetServerAddress() const
    {
        return m_serverAddress;
    }

    Result Connect(const NetAddress& serverAddress);
    void Disconnect();

    void Update();

    Delegate<void, const NetServerDisconnectedData&> OnDisconnected;

private:
    NetSocketUDP m_socket;
    NetAddress m_serverAddress;
    Time m_lastActivityTime;
    Time m_lastKeepAliveTime;
    bool m_isConnected;
};

} // namespace net

using net::NetClient;
using net::NetServerDisconnectedData;

} // namespace Hyperion
