#include <SystemPch.hpp>

#include <system/App.hpp>
#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <core/cli/CommandLine.hpp>

#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderBackend.hpp>

#include <game/Game.hpp>

#include <engine/EngineDriver.hpp>

#include <engine/threads/GameThread.hpp>

#include <HyperionEngine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

extern const CommandLineArguments& CoreApi_GetCommandLineArguments();

namespace sys {

App& App::GetInstance()
{
    static App s_instance;
    return s_instance;
}

App::App()
{
}

App::~App()
{
}

void App::LaunchGame(const Handle<Game>& game)
{
    AssertOnThread(g_mainThread);

    Assert(game != nullptr);
    Assert(g_engineDriver != nullptr && g_gameThreadInstance->IsRunning());

    g_gameThreadInstance->SetGame(game);

    const CommandLineArguments& cmdArgs = CoreApi_GetCommandLineArguments();

    if (!cmdArgs["Detached"].ToBool() && g_mainThread != g_renderThread)
    {
        while (g_engineDriver->IsRenderLoopActive())
        {
            SystemEvent event;
            while (g_appContext->PollEvents(event))
                ;
        }
    }
}

} // namespace sys
} // namespace hyperion
