/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Net/NetClient.hpp>
#include <Net/NetAddress.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Utilities/Result.hpp>

#include <Core/Containers/String.hpp>

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

    Result Connect(const ANSIString& hostname, uint16 defaultPort);
    Result Connect(const NetAddress& serverAddress);

    void Disconnect();

private:
    net::NetClient m_netClient;
    UniquePtr<GameClientThread> m_thread;
};

} // namespace Hyperion
