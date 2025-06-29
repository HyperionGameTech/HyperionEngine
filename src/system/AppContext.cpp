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

#ifdef HYP_SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#endif

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

HYP_API const CommandLineArgumentDefinitions& DefaultCommandLineArgumentDefinitions()
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
      m_window_handle(nullptr)
{
}

SDLApplicationWindow::~SDLApplicationWindow()
{
    SDL_DestroyWindow(static_cast<SDL_Window*>(m_window_handle));
}

void SDLApplicationWindow::Initialize(WindowOptions window_options)
{
    uint32 sdl_flags = 0;

    if (!(window_options.flags & WindowFlags::NO_GFX))
    {
#if HYP_VULKAN
        sdl_flags |= SDL_WINDOW_VULKAN;
#endif
    }

    if (window_options.flags & WindowFlags::HIGH_DPI)
    {
        sdl_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
    }

    if (window_options.flags & WindowFlags::HEADLESS)
    {
        sdl_flags |= SDL_WINDOW_HIDDEN;
    }
    else
    {
        sdl_flags |= SDL_WINDOW_SHOWN;
        sdl_flags |= SDL_WINDOW_RESIZABLE;

        // make sure to use SDL_free on file name strings for these events
        SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    }

    m_window_handle = SDL_CreateWindow(
        m_title.Data(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        int(m_size.x),
        int(m_size.y),
        sdl_flags);

    AssertThrowMsg(m_window_handle != nullptr, "Failed to initialize window: %s", SDL_GetError());
}

#ifdef HYP_VULKAN
VkSurfaceKHR SDLApplicationWindow::CreateVkSurface(Instance* instance)
{
    VkSurfaceKHR surface;
    SDL_bool result = SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(m_window_handle), instance->GetInstance(), &surface);

    AssertThrowMsg(result == SDL_TRUE, "Failed to create Vulkan surface: %s", SDL_GetError());

    return surface;
}
#endif

void SDLApplicationWindow::SetMousePosition(Vec2i position)
{
    SDL_WarpMouseInWindow(static_cast<SDL_Window*>(m_window_handle), position.x, position.y);
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
    SDL_GetWindowSize(static_cast<SDL_Window*>(m_window_handle), &width, &height);

    return Vec2i { width, height };
}

void SDLApplicationWindow::SetIsMouseLocked(bool locked)
{
    if (locked)
    {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
    else
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
}

bool SDLApplicationWindow::HasMouseFocus() const
{
    const SDL_Window* focus_window = SDL_GetMouseFocus();

    return focus_window == static_cast<SDL_Window*>(m_window_handle);
}

bool SDLApplicationWindow::IsHighDPI() const
{
    const int display_index = SDL_GetWindowDisplayIndex(static_cast<SDL_Window*>(m_window_handle));

    if (display_index < 0)
    {
        return false;
    }

    float ddpi, hdpi, vdpi;

    if (SDL_GetDisplayDPI(display_index, &ddpi, &hdpi, &vdpi) == 0)
    {
        return hdpi > 96.0f;
    }

    return false;
}

#pragma endregion SDLApplicationWindow

#pragma region SDLAppContext

#if defined(HYP_SDL) && defined(HYP_IOS)
static struct IOSSDLInitializer
{
    IOSSDLInitializer()
    {
        SDL_SetMainReady();
    }
} g_ios_sdl_initializer = {};
#endif

SDLAppContext::SDLAppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments)
{
    const int sdl_init_result = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    if (sdl_init_result < 0)
    {
        HYP_FAIL("Failed to initialize SDL: %s (%d)", SDL_GetError(), sdl_init_result);
    }
}

SDLAppContext::~SDLAppContext()
{
    SDL_Quit();
}

Handle<ApplicationWindow> SDLAppContext::CreateSystemWindow(WindowOptions window_options)
{
    Handle<SDLApplicationWindow> window = CreateObject<SDLApplicationWindow>(window_options.title, window_options.size);
    window->Initialize(window_options);

    return window;
}

int SDLAppContext::PollEvent(SystemEvent& event)
{
    const int result = SDL_PollEvent(event.GetInternalEvent());

    if (result)
    {
        if (event.GetType() == SystemEventType::EVENT_FILE_DROP)
        {
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
bool SDLAppContext::GetVkExtensions(Array<const char*>& out_extensions) const
{
    uint32 num_extensions = 0;
    SDL_Window* window = static_cast<SDL_Window*>(static_cast<SDLApplicationWindow*>(m_main_window.Get())->GetInternalWindowHandle());

    if (!SDL_Vulkan_GetInstanceExtensions(window, &num_extensions, nullptr))
    {
        return false;
    }

    out_extensions.Resize(num_extensions);

    if (!SDL_Vulkan_GetInstanceExtensions(window, &num_extensions, out_extensions.Data()))
    {
        return false;
    }

    return true;
}
#endif

#pragma endregion SDLAppContext

#pragma region AppContextBase

AppContextBase::AppContextBase(ANSIString name, const CommandLineArguments& arguments)
    : m_configuration("app")
{
    m_input_manager = CreateObject<InputManager>();

    m_name = std::move(name);

    if (m_name.Empty())
    {
        if (json::JSONValue config_app_name = m_configuration.Get("app.name"))
        {
            m_name = m_configuration.Get("app.name").ToString();
        }
    }
}

AppContextBase::~AppContextBase() = default;

const Handle<Game>& AppContextBase::GetGame() const
{
    return m_game;
}

void AppContextBase::SetGame(const Handle<Game>& game)
{
    m_game = game;
}

void AppContextBase::SetMainWindow(const Handle<ApplicationWindow>& window)
{
    m_main_window = window;
    m_input_manager->SetWindow(m_main_window.Get());

    OnCurrentWindowChanged(m_main_window.Get());
}

void AppContextBase::UpdateConfigurationOverrides()
{
    // if ray tracing is not supported, we need to update the configuration
    if (!g_render_backend->GetRenderConfig().IsRaytracingSupported())
    {
        m_configuration.Set("rendering.rt.enabled", false);
        m_configuration.Set("rendering.rt.reflections.enabled", false);
        m_configuration.Set("rendering.rt.gi.enabled", false);
        m_configuration.Set("rendering.rt.path_tracing.enabled", false);

        // Save new configuration to disk
        if (m_configuration.IsChanged())
        {
            m_configuration.Save();
        }
    }
}

#pragma endregion AppContextBase

} // namespace sys
} // namespace hyperion
