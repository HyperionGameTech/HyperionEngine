#include <core/system/App.hpp>
#include <core/system/AppContext.hpp>
#include <core/system/ArgParse.hpp>
#include <core/system/SystemEvent.hpp>

#include <core/logging/Logger.hpp>

#include <Game.hpp>

#include <HyperionEngine.hpp>

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
    hyperion::InitializeEngine(FilePath(arguments.GetCommand()).BasePath());

    m_app_context = InitAppContext(game, arguments);

    hyperion::DestroyEngine();
}

RC<AppContext> App::InitAppContext(Game *game, const CommandLineArguments &arguments)
{
    AssertThrow(game != nullptr);

    RC<AppContext> app_context;

#ifdef HYP_SDL
    app_context = MakeRefCountedPtr<SDLAppContext>("Hyperion", arguments);
#else
    #error "No AppContext implementation for this platform!"
#endif

    app_context->SetGame(game);

    const CommandLineArguments &app_context_arguments = app_context->GetArguments();

    Vec2i resolution = { 1280, 720 };

    EnumFlags<WindowFlags> window_flags = WindowFlags::HIGH_DPI;

    if (app_context_arguments["Headless"].ToBool()) {
        window_flags |= WindowFlags::HEADLESS;
    }

    if (app_context_arguments["ResX"].IsNumber()) {
        resolution.x = app_context_arguments["ResX"].ToInt32();
    }

    if (app_context_arguments["ResY"].IsNumber()) {
        resolution.y = app_context_arguments["ResY"].ToInt32();
    }

    if (!(window_flags & WindowFlags::HEADLESS)) {
        app_context->SetMainWindow(app_context->CreateSystemWindow({
            "Hyperion Engine",
            resolution,
            window_flags
        }));
    }

    hyperion::InitializeAppContext(app_context, game);

    return app_context;
}

} // namespace sys
} // namespace hyperion