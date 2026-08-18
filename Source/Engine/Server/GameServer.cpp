/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Server/GameServer.hpp>

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Threads.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(GameServer);

using namespace net;

class GameServerThread : public Thread<Scheduler, NetServer*>
{
public:
    GameServerThread()
        : Thread(StaticThreadId(NAME("GameServer")), ThreadPriorityValue::HIGHEST)
    {
    }

    virtual void operator()(NetServer* netServer) override
    {
        while (HYP_LIKELY(!m_stopRequested.LoadVolatile()))
        {
            netServer->Update();

            ThreadSleep(10);
        }
    }
};

GameServer::GameServer()
{
    m_netServer.OnClientConnected.Bind([](const NetClientConnectedData& data)
                                      {
                                          HYP_LOG(GameServer, Info, "Client connected: {} (connection id: {})", data.address.ToString(), uint32(data.connectionId));
                                      })
        .Detach();

    m_netServer.OnClientDisconnected.Bind([](const NetClientDisconnectedData& data)
                                         {
                                             HYP_LOG(GameServer, Info, "Client disconnected: {} (connection id: {})", data.address.ToString(), uint32(data.connectionId));
                                         })
        .Detach();
}

GameServer::~GameServer()
{
    Stop();
}

bool GameServer::IsRunning() const
{
    return m_thread != nullptr && m_thread->IsRunning();
}

Result GameServer::Start(uint16 port)
{
    if (IsRunning())
    {
        return HYP_MAKE_ERROR(Error, "Game server is already running");
    }

    if (Result listenResult = m_netServer.Listen(port); listenResult.HasError())
    {
        return listenResult;
    }

    HYP_LOG(GameServer, Info, "Game server listening on port {}", port);

    m_thread = MakeUnique<GameServerThread>();
    m_thread->Start(&m_netServer);

    return {};
}

void GameServer::Stop()
{
    if (m_thread == nullptr)
    {
        return;
    }

    m_thread->Stop();
    m_thread->Join();
    m_thread.Reset();

    m_netServer.StopListening();
}

} // namespace Hyperion
