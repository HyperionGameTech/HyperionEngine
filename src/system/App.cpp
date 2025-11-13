#include <HyperionPch.hpp>

#include <system/App.hpp>
#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/logging/Logger.hpp>

#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderBackend.hpp>

#include <game/Game.hpp>
#include <game/GameThread.hpp>

#include <engine/EngineDriver.hpp>

#include <engine/EngineGlobals.hpp>
#include <HyperionEngine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

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

    Assert(game.IsValid());

    m_gameThread = MakeUnique<GameThread>();
    m_gameThread->SetGame(game);
    m_gameThread->Start();

    // Loop blocks the main thread until the game is done.
    Assert(g_engineDriver->StartRenderLoop());
}

} // namespace sys
} // namespace hyperion
