/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Net/NetClient.hpp>
#include <Net/NetAddress.hpp>

#include <Framework/Client/ClientReplicationManager.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Utilities/Result.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Functional/Delegate.hpp>

namespace Hyperion {

class GameClientThread;

class ENGINE_API GameClient
{
public:
    GameClient();

    GameClient(const GameClient& other) = delete;
    GameClient& operator=(const GameClient& other) = delete;

    ~GameClient();

    HYP_FORCE_INLINE ThreadBase* GetThread() const
    {
        return reinterpret_cast<ThreadBase*>(m_thread.Get());
    }

    NetClientConnectionState GetConnectionState() const;

    bool IsConnected() const;

    Result GetLastError() const;

    /// Round-trip time to the server in milliseconds (0 until the first keep-alive pong).
    HYP_FORCE_INLINE TimeDiff GetRtt() const
    {
        return TimeDiff(m_netClient.GetRttMilliseconds());
    }

    Result Connect(const ANSIString& hostname, uint16 defaultPort);
    Result Connect(const NetAddress& serverAddress);

    void Disconnect();

    HYP_FORCE_INLINE ClientReplicationManager& GetReplicationManager()
    {
        return m_replicationManager;
    }

    HYP_FORCE_INLINE net::NetClient& GetNetClient()
    {
        return m_netClient;
    }

    Delegate<void, net::NetConnectionId> OnConnected;
    Delegate<void, net::NetConnectionId> OnDisconnected;

private:
    net::NetClient m_netClient;
    UniquePtr<GameClientThread> m_thread;
    ClientReplicationManager m_replicationManager;
};

} // namespace Hyperion
