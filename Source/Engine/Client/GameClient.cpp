/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Client/GameClient.hpp>

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Threads.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(GameClient);

using namespace net;

class GameClientThread : public Thread<Scheduler, NetClient*>
{
public:
    GameClientThread()
        : Thread(StaticThreadId(NAME("GameClient")), ThreadPriorityValue::HIGHEST)
    {
    }

    virtual void operator()(NetClient* netClient) override
    {
        while (HYP_LIKELY(!m_stopRequested.LoadVolatile()))
        {
            netClient->Update();

            ThreadSleep(10);
        }
    }
};

GameClient::GameClient()
{
    m_netClient.OnDisconnected.Bind([](const NetServerDisconnectedData& data)
                                   {
                                       HYP_LOG(GameClient, Warning, "Disconnected from server: {}", data.serverAddress.ToString());
                                   })
        .Detach();
}

GameClient::~GameClient()
{
    Disconnect();
}

bool GameClient::IsConnected() const
{
    return m_netClient.IsConnected();
}

Result GameClient::Connect(const NetAddress& serverAddress)
{
    if (m_thread != nullptr && m_thread->IsRunning())
    {
        return HYP_MAKE_ERROR(Error, "Game client is already connected");
    }

    if (Result connectResult = m_netClient.Connect(serverAddress); connectResult.HasError())
    {
        return connectResult;
    }

    HYP_LOG(GameClient, Info, "Connected to server: {}", m_netClient.GetServerAddress().ToString());

    m_thread = MakeUnique<GameClientThread>();
    m_thread->Start(&m_netClient);

    return {};
}

void GameClient::Disconnect()
{
    if (m_thread == nullptr)
    {
        return;
    }

    m_thread->Stop();
    m_thread->Join();
    m_thread.Reset();

    m_netClient.Disconnect();
}

} // namespace Hyperion
