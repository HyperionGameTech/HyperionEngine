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

#include <iostream>
#include <limits>

namespace Hyperion {

HYP_DEFINE_LOG_CHANNEL(GameServer);

using namespace net;

extern "C" int Hyp_ExecuteConsoleCommand(int argc, const char** argv);

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
}

} // namespace Hyperion
