/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <system/AppContext.hpp>
#include <core/debug/Debug.hpp>
#include <system/SystemEvent.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/logging/Logger.hpp>

#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <Engine.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

HYP_API const CommandLineArgumentDefinitions &DefaultCommandLineArgumentDefinitions()
{
    static const struct DefaultCommandLineArgumentDefinitionsInitializer
    {
        CommandLineArgumentDefinitions definitions;

        DefaultCommandLineArgumentDefinitionsInitializer()
        {
            definitions.Add("Profile", {}, "Enable collection of profiling data for functions that opt in using HYP_SCOPE.", CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);
            definitions.Add("TraceURL", {}, "The endpoint url that profiling data will be submitted to (this url will have /start appended to it to start the session and /results to add results)", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING);
            definitions.Add("ResX", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::INTEGER);
            definitions.Add("ResY", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::INTEGER);
            definitions.Add("Headless", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);
            definitions.Add("Mode", "m", {}, CommandLineArgumentFlags::NONE, Array<String> { "precompile_shaders", "editor" }, String("editor"));
        }
    } initializer;

    return initializer.definitions;
}

namespace sys {

#pragma region ApplicationWindow

ApplicationWindow::ApplicationWindow(ANSIString title, Vec2i size)
    : m_title(std::move(title)),
      m_size(size)
{
}

void ApplicationWindow::HandleResize(Vec2i new_size)
{
    m_size = new_size;

    OnWindowSizeChanged(new_size);
}

#pragma endregion ApplicationWindow

#pragma region SDLApplicationWindow

SDLApplicationWindow::SDLApplicationWindow(ANSIString title, Vec2i size)
    : ApplicationWindow(std::move(title), size),
      window(nullptr)
{
}

SDLApplicationWindow::~SDLApplicationWindow()
{
    SDL_DestroyWindow(this->window);
}

void SDLApplicationWindow::Initialize(WindowOptions window_options)
{
    uint32 sdl_flags = 0;

    if (!(window_options.flags & WindowFlags::NO_GFX)) {
#if HYP_VULKAN
        sdl_flags |= SDL_WINDOW_VULKAN;
#endif
    }

    if (window_options.flags & WindowFlags::HIGH_DPI) {
        sdl_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
    }

    if (window_options.flags & WindowFlags::HEADLESS) {
        sdl_flags |= SDL_WINDOW_HIDDEN;
    } else {
        sdl_flags |= SDL_WINDOW_SHOWN;
        sdl_flags |= SDL_WINDOW_RESIZABLE;

        // make sure to use SDL_free on file name strings for these events
        SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    }

    this->window = SDL_CreateWindow(
        m_title.Data(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        int(m_size.x),
        int(m_size.y),
        sdl_flags
    );

    AssertThrowMsg(this->window != nullptr, "Failed to initialize window: %s", SDL_GetError());
}

#ifdef HYP_VULKAN
VkSurfaceKHR SDLApplicationWindow::CreateVkSurface(renderer::Instance *instance)
{
    VkSurfaceKHR surface;
    SDL_bool result = SDL_Vulkan_CreateSurface(window, instance->GetInstance(), &surface);

    AssertThrowMsg(result == SDL_TRUE, "Failed to create Vulkan surface: %s", SDL_GetError());

    return surface;
}
#endif

void SDLApplicationWindow::SetMousePosition(Vec2i position)
{
    SDL_WarpMouseInWindow(window, position.x, position.y);
}

Vec2i SDLApplicationWindow::GetMousePosition() const
{
    Vec2i position;
    SDL_GetMouseState(&position.x, &position.y);

    return position;
}

Vec2i SDLApplicationWindow::GetDimensions() const
{
    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    return Vec2i { width, height };
}

void SDLApplicationWindow::SetIsMouseLocked(bool locked)
{
    if (locked) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    } else {
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
}

bool SDLApplicationWindow::HasMouseFocus() const
{
    const auto *focus_window = SDL_GetMouseFocus();

    return focus_window == window;
}

bool SDLApplicationWindow::IsHighDPI() const
{
    const int display_index = SDL_GetWindowDisplayIndex(window);

    if (display_index < 0) {
        return false;
    }

    float ddpi, hdpi, vdpi;

    if (SDL_GetDisplayDPI(display_index, &ddpi, &hdpi, &vdpi) == 0) {
        return hdpi > 96.0f;
    }

    return false;
}

#pragma endregion SDLApplicationWindow

#pragma region SDLAppContext

SDLAppContext::SDLAppContext(ANSIString name, const CommandLineArguments &arguments)
    : AppContext(std::move(name), arguments)
{
    const int sdl_init_result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    AssertThrowMsg(sdl_init_result == 0, "Failed to initialize SDL: %s", SDL_GetError());
}

SDLAppContext::~SDLAppContext()
{
    SDL_Quit();
}

RC<ApplicationWindow> SDLAppContext::CreateSystemWindow(WindowOptions window_options)
{
    RC<SDLApplicationWindow> window = MakeRefCountedPtr<SDLApplicationWindow>(window_options.title, window_options.size);
    window->Initialize(window_options);

    return window;
}

int SDLAppContext::PollEvent(SystemEvent &event)
{
    const int result = SDL_PollEvent(event.GetInternalEvent());

    if (result) {
        if (event.GetType() == SystemEventType::EVENT_FILE_DROP) {
            // set event data variant to the file path
            event.GetEventData().Set(FilePath(event.GetInternalEvent()->drop.file));

            // need to free or else the mem will leak
            SDL_free(event.GetInternalEvent()->drop.file);
            event.GetInternalEvent()->drop.file = nullptr;
        }
    }

    return result;
}

#ifdef HYP_VULKAN
bool SDLAppContext::GetVkExtensions(Array<const char *> &out_extensions) const
{
    uint32 num_extensions = 0;
    SDL_Window *window = static_cast<SDLApplicationWindow *>(m_main_window.Get())->GetInternalWindow();

    if (!SDL_Vulkan_GetInstanceExtensions(window, &num_extensions, nullptr)) {
        return false;
    }
    
    out_extensions.Resize(num_extensions);

    if (!SDL_Vulkan_GetInstanceExtensions(window, &num_extensions, out_extensions.Data())) {
        return false;
    }

    return true;
}
#endif

#pragma endregion SDLAppContext

#pragma region AppContext

AppContext::AppContext(ANSIString name, const CommandLineArguments &arguments)
    : m_configuration("app"),
      m_game(nullptr)
{
    m_input_manager = CreateObject<InputManager>();

    m_name = std::move(name);

    if (m_name.Empty()) {
        if (json::JSONValue config_app_name = m_configuration.Get("app.name")) {
            m_name = m_configuration.Get("app.name").ToString();
        }
    }

    UniquePtr<CommandLineArguments> new_arguments;
    
    if (json::JSONValue config_args = m_configuration.Get("app.args")) {
        json::JSONString config_args_string = config_args.ToString();
        Array<String> config_args_string_split = config_args_string.Split(' ');

        CommandLineParser arg_parse { &DefaultCommandLineArgumentDefinitions() };

        TResult<CommandLineArguments> parse_result = arg_parse.Parse(arguments.GetCommand(), config_args_string_split);

        if (parse_result.HasValue()) { 
            new_arguments = MakeUnique<CommandLineArguments>(CommandLineArguments::Merge(*arg_parse.GetDefinitions(), *parse_result, arguments));
        } else {
            HYP_LOG(Core, Error, "Failed to parse config command line value \"{}\":\n\t{}", config_args_string, parse_result.GetError().GetMessage());
        }
    }

    if (!new_arguments) {
        new_arguments = MakeUnique<CommandLineArguments>(arguments);
    }

    m_arguments = std::move(new_arguments);
}

AppContext::~AppContext() = default;

Game *AppContext::GetGame() const
{
    return m_game;
}

void AppContext::SetGame(Game *game)
{
    m_game = game;
}

const CommandLineArguments &AppContext::GetArguments() const
{
    AssertThrow(m_arguments != nullptr);

    return *m_arguments;
}

void AppContext::SetMainWindow(const RC<ApplicationWindow> &window)
{
    m_main_window = window;
    m_input_manager->SetWindow(m_main_window.Get());

    OnCurrentWindowChanged(m_main_window.Get());
}

void AppContext::UpdateConfigurationOverrides()
{
    // if ray tracing is not supported, we need to update the configuration
    if (!g_engine->GetGPUDevice()->GetFeatures().IsRaytracingSupported()) {
        m_configuration.Set("rendering.rt.enabled", false);
        m_configuration.Set("rendering.rt.reflections.enabled", false);
        m_configuration.Set("rendering.rt.gi.enabled", false);
        m_configuration.Set("rendering.rt.path_tracer.enabled", false);

        // Save new configuration to disk
        if (m_configuration.IsChanged()) {
            m_configuration.Save();
        }
    }
}

#pragma endregion AppContext

} // namespace sys
} // namespace hyperion