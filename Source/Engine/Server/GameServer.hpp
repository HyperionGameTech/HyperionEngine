/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Net/NetServer.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Utilities/Result.hpp>

namespace Hyperion {

class GameServerThread;

class ENGINE_API GameServer
{
public:
    GameServer();

    GameServer(const GameServer& other) = delete;
    GameServer& operator=(const GameServer& other) = delete;

    ~GameServer();

    bool IsRunning() const;

    Result Start(uint16 port);
    void Stop();

private:
    net::NetServer m_netServer;
    UniquePtr<GameServerThread> m_thread;
};

} // namespace Hyperion
