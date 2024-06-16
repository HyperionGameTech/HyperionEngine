#include <core/system/App.hpp>
#include <core/system/AppContext.hpp>
#include <core/system/SystemEvent.hpp>

#include <core/logging/Logger.hpp>

#include <Game.hpp>

#include <HyperionEngine.hpp>

#define HYP_LOG_FRAMES_PER_SECOND

namespace hyperion {
namespace sys {

App::App()
{
}

App::~App()
{
}

void App::Launch(Game *game, const CommandLineArguments &arguments)
{
    m_app_context = InitAppContext(arguments);

    game->SetAppContext(m_app_context);

    Engine::GetInstance()->InitializeGame(game);

    RunMainLoop(game);

    hyperion::ShutdownApplication();
}

void App::RunMainLoop(Game *game)
{
    SystemEvent event;

#ifdef HYP_LOG_FRAMES_PER_SECOND
    uint32 num_frames = 0;
    float delta_time_accum = 0.0f;

    GameCounter counter;
#endif

    while (Engine::GetInstance()->IsRenderLoopActive()) {
        // input manager stuff
        while (m_app_context->PollEvent(event)) {
            game->PushEvent(std::move(event));
        }

#ifdef HYP_LOG_FRAMES_PER_SECOND
        counter.NextTick();
        delta_time_accum += counter.delta;
        num_frames++;

        if (delta_time_accum >= 1.0f) {
            DebugLog(
                LogType::Debug,
                "Render FPS: %f\n",
                1.0f / (delta_time_accum / float(num_frames))
            );

            delta_time_accum = 0.0f;
            num_frames = 0;
        }
#endif

        Engine::GetInstance()->RenderNextFrame(game);
    }
}

RC<AppContext> App::InitAppContext(const CommandLineArguments &arguments)
{
    RC<AppContext> app_context;

#ifdef HYP_SDL
    app_context.Reset(new SDLAppContext("Hyperion", arguments));
#else
#error "No AppContext implementation for this platform!"
#endif

    Vec2u resolution = { 1280, 720 };

    EnumFlags<WindowFlags> window_flags = WindowFlags::NONE;//WindowFlags::HIGH_DPI;

    if (arguments["Headless"].Is<bool>()) {
        if (arguments["Headless"].Get<bool>()) {
            window_flags |= WindowFlags::HEADLESS;
        }
    }

    if (arguments["ResX"].Is<uint32>()) {
        resolution.x = arguments["ResX"].Get<uint32>();
    }

    if (arguments["ResY"].Is<uint32>()) {
        resolution.y = arguments["ResY"].Get<uint32>();
    }

    if (!(window_flags & WindowFlags::HEADLESS)) {
        app_context->SetCurrentWindow(app_context->CreateSystemWindow({
            "Hyperion Engine",
            resolution,
            window_flags
        }));
    }

    hyperion::InitializeAppContext(app_context);

    return app_context;
}

} // namespace sys
} // namespace hyperion