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
            if (g_appContext->GetMainWindow() != nullptr)
            {
                SystemEvent event;
                while (g_appContext->PollEvent(event))
                {
                    g_appContext->GetMainWindow()->GetInputEventSink().Push(std::move(event));
                }

                if (g_appContext->GetMainWindow()->GetDimensions() != g_appContext->GetMainWindow()->GetSize())
                {
                    HYP_LOG(Core, Debug, "WINDOW RESIZED! New dimensions: {}, Old size: {}",
                        g_appContext->GetMainWindow()->GetDimensions(),
                        g_appContext->GetMainWindow()->GetSize());
                    // g_appContext->GetMainWindow()->SetSize(g_appContext->GetMainWindow()->GetDimensions());
                }

                /// @TODO: Make this less terrible; we should have a proper event loop instead of busy-waiting
                /// (i.e using condition variables or similar)
            }
        }
    }
}

} // namespace sys
} // namespace hyperion
