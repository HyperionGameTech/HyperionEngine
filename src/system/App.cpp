#include <HyperionPch.hpp>

#include <system/App.hpp>
#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/logging/Logger.hpp>

#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderBackend.hpp>

#include <game/Game.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineGlobals.hpp>

#include <HyperionEngine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

extern const CommandLineArguments& CoreApi_GetCommandLineArguments();

namespace sys {

App& App::GetInstance()
{
    static App instance;
    return instance;
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

    g_engineDriver->StartThreadsForGame(game);

    const CommandLineArguments& cmdArgs = CoreApi_GetCommandLineArguments();

    // TEMP
    if (!cmdArgs["Detached"].ToBool() && g_mainThread != g_renderThread)
    {
        // headless mode creates a separate thread for rendering, so we need to wait for it to finish
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
