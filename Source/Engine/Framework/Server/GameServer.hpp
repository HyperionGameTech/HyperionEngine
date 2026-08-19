/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Net/NetServer.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Utilities/Result.hpp>
#include <Core/Utilities/IndexAllocator.hpp>

namespace Hyperion {

class GameServerThread;
class ConsoleInputThread;

enum class NetId : uint32;

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

    HYP_NODISCARD NetId AllocNetId();
    void FreeNetId(NetId netId);

private:
    net::NetServer m_netServer;
    UniquePtr<GameServerThread> m_thread;
    UniquePtr<ConsoleInputThread> m_consoleInputThread;

    AtomicIndexAllocator m_netIdAllocator;
};

} // namespace Hyperion
