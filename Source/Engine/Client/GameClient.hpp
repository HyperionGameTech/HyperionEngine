/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Net/NetClient.hpp>
#include <Core/Net/NetAddress.hpp>

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

    bool IsConnected() const;

    Result Connect(const NetAddress& serverAddress);
    void Disconnect();

private:
    net::NetClient m_netClient;
    UniquePtr<GameClientThread> m_thread;
};

} // namespace Hyperion
