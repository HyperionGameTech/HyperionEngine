/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/Server/GameServer.hpp>

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Threads.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Core/Logging/Logger.hpp>

#include <iostream>
#include <limits>

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(GameServer);

using namespace net;

extern "C" int Hyp_ExecuteConsoleCommand(int argc, const char** argv);

class GameServerThread : public Thread<Scheduler, NetServer*>
{
public:
    GameServerThread(GameServer* ownerServer)
        : Thread(StaticThreadId(NAME("GameServer")), ThreadPriorityValue::HIGHEST),
          m_ownerServer(ownerServer)
    {
    }

    virtual void operator()(NetServer* netServer) override
    {
        InitThreadAllocator();

        while (HYP_LIKELY(!m_stopRequested.LoadVolatile()))
        {
            netServer->Update();

            m_ownerServer->GetRequestManager().PublishBatch();

            if (m_scheduler->NumEnqueued())
            {
                Array<Scheduler::ScheduledTask, ThreadAllocator> tasks;
                m_scheduler->AcceptAll(tasks);

                for (auto& task : tasks)
                {
                    task.Execute();
                }
            }
            
            m_threadAllocator->Reset();

            ThreadSleep(10);
        }
    }

private:
    GameServer* m_ownerServer;
};

class ConsoleInputThread : public Thread<Scheduler>
{
public:
    ConsoleInputThread()
        : Thread(StaticThreadId(NAME("ConsoleInput")), ThreadPriorityValue::LOWEST)
    {
    }

    virtual void operator()() override
    {
        char buffer[1024];

        while (HYP_LIKELY(!m_stopRequested.LoadVolatile()))
        {
            std::cin.getline(buffer, sizeof(buffer));

            if (std::cin.eof())
            {
                break;
            }

            if (std::cin.fail())
            {
                // line was longer than the buffer; discard the remainder and keep reading
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }

            if (buffer[0] == '\0')
            {
                continue;
            }

            const String commandLine { buffer };

            Array<String> args = commandLine.Split(' ');
            Array<const char*> argsCharV = MapToArray(args, [](const String& str) { return str.Data(); });

            const int result = Hyp_ExecuteConsoleCommand(int(args.Size()), argsCharV.Data());

            if (result != 0)
            {
                HYP_LOG(GameServer, Error, "Error executing console command '{}': returned error code {}", commandLine, result);
            }
        }
    }
};

GameServer::GameServer()
{
    m_requestManager.RegisterHandlers(m_netServer);

    m_netServer.OnClientConnected.Bind([this](const NetClientConnectedData& data)
                                      {
                                          HYP_LOG(GameServer, Info, "Client connected: {} (connection id: {})", data.address.ToString(), uint32(data.connectionId));

                                          NotifyClientConnected(data.connectionId);
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

    m_thread = MakeUnique<GameServerThread>(this);
    m_thread->Start(&m_netServer);

    m_consoleInputThread = MakeUnique<ConsoleInputThread>();
    m_consoleInputThread->Start();

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

    if (m_consoleInputThread != nullptr)
    {
        m_consoleInputThread->Stop();
        m_consoleInputThread->Detach();
        m_consoleInputThread.Reset();
    }

    m_netIdAllocator.Reset();
}

NetId GameServer::AllocNetId()
{
    return NetId(m_netIdAllocator.Allocate());
}

void GameServer::FreeNetId(NetId netId)
{
    m_netIdAllocator.Free(uint32(netId));
}

void GameServer::NotifyClientConnected(NetConnectionId connectionId)
{
    Mutex::Guard guard(m_newConnectionsMutex);

    m_newConnections.PushBack(connectionId);
}

} // namespace Hyperion
