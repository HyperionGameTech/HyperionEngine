/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineDriver.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Core/Net/NetServer.hpp>

#include <Core/Reflection/ClassUtils.hpp>

#include <Core/Logging/Logger.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion
{

HYP_DEFINE_LOG_CHANNEL(GameServer);

using namespace net;

class GameServerThread : public Thread<Scheduler, Handle<Game>, TaskPromise<void>*>
{
public:
    GameServerThread()
        : Thread(StaticThreadId(NAME("GameServer")), ThreadPriorityValue::HIGHEST)
    {
    }

    virtual void operator()(Handle<Game> game, TaskPromise<void>* onDone) override;
};

void GameServerThread::operator()(Handle<Game> game, TaskPromise<void>* onDone)
{
    NetServer server;

    server.OnClientConnected.Bind(
                                [](const NetClientConnectedData& data)
                                {
                                    HYP_LOG(GameServer, Info, "Client connected: {} (connection id: {})", data.address.ToString(), uint32(data.connectionId));
                                })
        .Detach();

    server.OnClientDisconnected.Bind(
                                   [](const NetClientDisconnectedData& data)
                                   {
                                       HYP_LOG(GameServer, Info, "Client disconnected: {} (connection id: {})", data.address.ToString(), uint32(data.connectionId));
                                   })
        .Detach();

    // @TODO Config var for port
    if (Result listenResult = server.Listen(9192); listenResult.HasError())
    {
        HYP_LOG(GameServer, Error, "Failed to start game server: {}", listenResult.GetError().GetMessage());
        return;
    }

    HYP_LOG(GameServer, Info, "Game server listening on port {}", 9192);

    while (HYP_LIKELY(!m_stopRequested.LoadVolatile()))
    {
        server.Update();

        ThreadSleep(10);
    }

    server.StopListening();

    if (onDone != nullptr)
    {
        onDone->Fulfill();
    }
}

class GameServerCommandlet final : public CommandletBase
{
    HYP_OBJECT_BODY(GameServerCommandlet);

public:
    virtual ~GameServerCommandlet() override = default;

    HYP_METHOD()
    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;

        static bool s_initialized = false;
        if (!s_initialized)
        {
            s_initialized = true;

            s_definitions.Add(
                "game",
                "g",
                "Game class name (e.g DefaultGame)",
                CommandLineArgumentFlags::REQUIRED,
                {},
                "");
        }

        return s_definitions;
    }

    virtual Result Run(const CommandLineArguments& args) override
    {
        const bool isCommandlet = EngineGlobals::IsCommandlet();
        if (!isCommandlet)
        {
            return HYP_MAKE_ERROR(Error, "Cannot run GameServer from within game");
        }

        // Create game instance based on the game class name
        const String gameClassName = args["game"].ToString();

        // Get the class
        const Class* gameClass = GetClass(gameClassName);
        if (!gameClass)
        {
            return HYP_MAKE_ERROR(Error, "Game class not found: {}", gameClassName);
        }

        BoxedValue gameInstanceBoxed;
        if (!gameClass->CreateInstance(gameInstanceBoxed))
        {
            return HYP_MAKE_ERROR(Error, "Failed to create game instance");
        }

        if (!gameInstanceBoxed.Is<Handle<Game>>())
        {
            return HYP_MAKE_ERROR(Error, "Game instance is not an instance of type Game");
        }

        Handle<Game> game = gameInstanceBoxed.Get<Handle<Game>>();

        Task<void> doneTask;
        TaskPromise<void>* donePromise = isCommandlet ? doneTask.Promise() : nullptr;

        GameServerThread serverThread;

        if (!serverThread.Start(game, donePromise))
        {
            return HYP_MAKE_ERROR(Error, "Failed to start Thread");
        }

        g_engineDriver->Initialize();
        g_engineDriver->SetGameInstance(game.Get());

        if (!g_engineDriver->StartThreads())
        {
            return HYP_MAKE_ERROR(Error, "Failed to start engine threads");
        }

        serverThread.Stop();
        serverThread.Join();

        doneTask.Await();

        return {};
    }
};

HYP_EXPORT const Class* g_clsGameServerCommandlet = nullptr;

const Class* GameServerCommandlet::StaticClass()
{
    return g_clsGameServerCommandlet;
}

// clang-format off

HYP_BEGIN_CLASS(GameServerCommandlet, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "gameserver"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

    // clang-format on

    HYP_REGISTER_STATIC_CLASS(GameServerCommandlet);

} // namespace Hyperion
