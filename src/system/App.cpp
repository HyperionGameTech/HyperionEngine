#include <system/App.hpp>
#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/logging/Logger.hpp>

#include <rendering/RenderGlobalState.hpp>
#include <rendering/backend/RenderBackend.hpp>

#include <Game.hpp>
#include <GameThread.hpp>

#include <Engine.hpp>

#include <EngineGlobals.hpp>
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
    Threads::AssertOnThread(g_main_thread);

    AssertThrow(game.IsValid());

    Handle<AppContextBase> app_context;

#ifdef HYP_SDL
    app_context = CreateObject<SDLAppContext>("Hyperion", GetCommandLineArguments());
#else
    HYP_FAIL("AppContext not implemented for this platform");
#endif

    app_context->SetGame(game);

    Vec2i resolution = { 1280, 720 };

    EnumFlags<WindowFlags> window_flags = WindowFlags::HIGH_DPI;

    if (GetCommandLineArguments()["Headless"].ToBool())
    {
        window_flags |= WindowFlags::HEADLESS;
    }

    if (GetCommandLineArguments()["ResX"].IsNumber())
    {
        resolution.x = GetCommandLineArguments()["ResX"].ToInt32();
    }

    if (GetCommandLineArguments()["ResY"].IsNumber())
    {
        resolution.y = GetCommandLineArguments()["ResY"].ToInt32();
    }

    if (!(window_flags & WindowFlags::HEADLESS))
    {
        HYP_LOG(Core, Info, "Running in windowed mode: {}x{}", resolution.x, resolution.y);

        app_context->SetMainWindow(app_context->CreateSystemWindow({ "Hyperion Engine", resolution, window_flags }));
    }
    else
    {
        HYP_LOG(Core, Info, "Running in headless mode");
    }

    AssertThrow(g_render_backend != nullptr);
    HYPERION_ASSERT_RESULT(g_render_backend->Initialize(*app_context));

    RenderObjectDeleter<Platform::current>::Initialize();

    g_render_global_state = new RenderGlobalState();

    g_engine->SetAppContext(app_context);
    InitObject(g_engine);

    m_game_thread = MakeUnique<GameThread>(app_context);
    m_game_thread->SetGame(game);
    m_game_thread->Start();

    // Loop blocks the main thread until the game is done.
    AssertThrow(g_engine->StartRenderLoop());

    delete g_render_global_state;
    g_render_global_state = nullptr;

    RenderObjectDeleter<Platform::current>::RemoveAllNow(/* force */ true);

    HYPERION_ASSERT_RESULT(g_render_backend->Destroy());
}

} // namespace sys
} // namespace hyperion