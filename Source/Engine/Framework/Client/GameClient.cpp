/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/Client/GameClient.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Task.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

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
        InitThreadAllocator();

        NetClientConnectionState previousState = netClient->GetConnectionState();

        while (HYP_LIKELY(!m_stopRequested.LoadVolatile()))
        {
            netClient->Update();

            if (g_gameClient != nullptr)
            {
                g_gameClient->GetReplicationManager().PublishBatch();
            }

            if (m_scheduler->NumEnqueued())
            {
                Array<Scheduler::ScheduledTask, ThreadAllocator> tasks;
                m_scheduler->AcceptAll(tasks);

                for (auto& task : tasks)
                {
                    task.Execute();
                }
            }

            const NetClientConnectionState state = netClient->GetConnectionState();

            if (state != previousState)
            {
                if (state == NetClientConnectionState::Connected)
                {
                    HYP_LOG(GameClient, Info, "Connected to server: {}", netClient->GetServerAddress().ToString());
                }
                else if (state == NetClientConnectionState::Disconnected && previousState == NetClientConnectionState::Connecting)
                {
                    HYP_LOG(GameClient, Error, "Failed to connect: {}", netClient->GetLastError().GetError().GetMessage());
                }

                previousState = state;
            }

            ThreadSleep(10);

            m_threadAllocator->Reset();
        }
    }
};

GameClient::GameClient()
{
    m_netClient.OnConnected.Bind(
        [this](const NetClientConnectionStateChangeData& data)
        {
            HYP_LOG(GameClient, Warning, "Connected to server: {}", data.serverAddress.ToString());

            OnConnected(m_netClient.GetConnectionId());
        })
        .Detach();

    m_netClient.OnDisconnected.Bind(
        [this](const NetClientConnectionStateChangeData& data)
        {
            HYP_LOG(GameClient, Warning, "Disconnected from server: {}", data.serverAddress.ToString());

            OnDisconnected(m_netClient.GetConnectionId());
        })
        .Detach();

    m_replicationManager.RegisterHandlers(m_netClient);
}

GameClient::~GameClient()
{
    Disconnect();

    m_netClient.OnConnected.RemoveAllDetached();
}

NetClientConnectionState GameClient::GetConnectionState() const
{
    return m_netClient.GetConnectionState();
}

bool GameClient::IsConnected() const
{
    return m_netClient.IsConnected();
}

Result GameClient::GetLastError() const
{
    return m_netClient.GetLastError();
}

Result GameClient::Connect(const ANSIString& hostname, uint16 defaultPort)
{
    TResult<NetAddress> resolveResult = NetAddress::TryResolve(hostname, defaultPort);

    if (resolveResult.HasError())
    {
        return resolveResult.GetError();
    }

    return Connect(resolveResult.GetValue());
}

Result GameClient::Connect(const NetAddress& serverAddress)
{
    if (m_netClient.GetConnectionState() != NetClientConnectionState::Disconnected)
    {
        return HYP_MAKE_ERROR(Error, "Game client is already connected");
    }

    // Tear down if running
    if (m_thread != nullptr)
    {
        m_thread->Stop();
        m_thread->Join();
        m_thread.Reset();
    }

    if (Result connectResult = m_netClient.Connect(serverAddress); connectResult.HasError())
    {
        return connectResult;
    }

    HYP_LOG(GameClient, Info, "Connecting to server: {}", serverAddress.ToString());

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
