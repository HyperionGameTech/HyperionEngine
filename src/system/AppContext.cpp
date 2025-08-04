/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <system/AppContext.hpp>
#include <core/debug/Debug.hpp>
#include <system/SystemEvent.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/logging/Logger.hpp>

#include <core/config/Config.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderDevice.hpp>

#ifdef HYP_VULKAN
#include <rendering/vulkan/VulkanInstance.hpp>
#endif

#include <EngineGlobals.hpp>
#include <Engine.hpp>

#ifdef HYP_SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#endif

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

HYP_API extern const GlobalConfig& GetGlobalConfig();

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

void ApplicationWindow::HandleResize(Vec2i newSize)
{
    m_size = newSize;

    OnWindowSizeChanged(newSize);
}

#pragma endregion ApplicationWindow

#pragma region SDLApplicationWindow

SDLApplicationWindow::SDLApplicationWindow(ANSIString title, Vec2i size)
    : ApplicationWindow(std::move(title), size),
      m_windowHandle(nullptr)
{
}

SDLApplicationWindow::~SDLApplicationWindow()
{
    SDL_DestroyWindow(static_cast<SDL_Window*>(m_windowHandle));
}

void SDLApplicationWindow::Initialize(WindowOptions windowOptions)
{
    uint32 sdlFlags = 0;

    if (!(windowOptions.flags & WindowFlags::NO_GFX))
    {
#if HYP_VULKAN
        sdlFlags |= SDL_WINDOW_VULKAN;
#endif
    }

    if (windowOptions.flags & WindowFlags::HIGH_DPI)
    {
        sdlFlags |= SDL_WINDOW_ALLOW_HIGHDPI;
    }

    if (windowOptions.flags & WindowFlags::HEADLESS)
    {
        sdlFlags |= SDL_WINDOW_HIDDEN;
    }
    else
    {
        sdlFlags |= SDL_WINDOW_SHOWN;
        sdlFlags |= SDL_WINDOW_RESIZABLE;

        // make sure to use SDL_free on file name strings for these events
        SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    }

    m_windowHandle = SDL_CreateWindow(
        m_title.Data(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        int(m_size.x),
        int(m_size.y),
        sdlFlags);

    Assert(m_windowHandle != nullptr, "Failed to initialize window: %s", SDL_GetError());
}

#ifdef HYP_VULKAN
VkSurfaceKHR SDLApplicationWindow::CreateVkSurface(VulkanInstance* instance)
{
    VkSurfaceKHR surface;
    SDL_bool result = SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(m_windowHandle), instance->GetInstance(), &surface);

    Assert(result == SDL_TRUE, "Failed to create Vulkan surface: %s", SDL_GetError());

    return surface;
}
#endif

void SDLApplicationWindow::SetMousePosition(Vec2i position)
{
    SDL_WarpMouseInWindow(static_cast<SDL_Window*>(m_windowHandle), position.x, position.y);
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
    SDL_GetWindowSize(static_cast<SDL_Window*>(m_windowHandle), &width, &height);

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
    const SDL_Window* focusWindow = SDL_GetMouseFocus();

    return focusWindow == static_cast<SDL_Window*>(m_windowHandle);
}

bool SDLApplicationWindow::IsHighDPI() const
{
    const int displayIndex = SDL_GetWindowDisplayIndex(static_cast<SDL_Window*>(m_windowHandle));

    if (displayIndex < 0)
    {
        return false;
    }

    float ddpi, hdpi, vdpi;

    if (SDL_GetDisplayDPI(displayIndex, &ddpi, &hdpi, &vdpi) == 0)
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
} g_iosSdlInitializer = {};
#endif

SDLAppContext::SDLAppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments)
{
    const int sdlInitResult = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    if (sdlInitResult < 0)
    {
        HYP_FAIL("Failed to initialize SDL: %s (%d)", SDL_GetError(), sdlInitResult);
    }
}

SDLAppContext::~SDLAppContext()
{
    SDL_Quit();
}

Handle<ApplicationWindow> SDLAppContext::CreateSystemWindow(WindowOptions windowOptions)
{
    Handle<SDLApplicationWindow> window = CreateObject<SDLApplicationWindow>(windowOptions.title, windowOptions.size);
    window->Initialize(windowOptions);

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
bool SDLAppContext::GetVkExtensions(Array<const char*>& outExtensions) const
{
    uint32 numExtensions = 0;
    SDL_Window* window = static_cast<SDL_Window*>(static_cast<SDLApplicationWindow*>(m_mainWindow.Get())->GetInternalWindowHandle());

    if (!SDL_Vulkan_GetInstanceExtensions(window, &numExtensions, nullptr))
    {
        return false;
    }

    outExtensions.Resize(numExtensions);

    if (!SDL_Vulkan_GetInstanceExtensions(window, &numExtensions, outExtensions.Data()))
    {
        return false;
    }

    return true;
}
#endif

#pragma endregion SDLAppContext

#pragma region AppContextBase

AppContextBase::AppContextBase(ANSIString name, const CommandLineArguments& arguments)
{
    m_inputManager = CreateObject<InputManager>();

    m_name = std::move(name);

    if (m_name.Empty())
    {
        if (json::JSONValue configAppName = GetGlobalConfig().Get("app.name"))
        {
            m_name =  GetGlobalConfig().Get("app.name").ToString();
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
    m_mainWindow = window;
    m_inputManager->SetWindow(m_mainWindow.Get());

    OnCurrentWindowChanged(m_mainWindow.Get());
}

void AppContextBase::UpdateConfigurationOverrides()
{
}

#pragma endregion AppContextBase

} // namespace sys
} // namespace hyperion
