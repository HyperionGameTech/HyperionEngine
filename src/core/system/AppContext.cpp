/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/system/AppContext.hpp>
#include <core/system/Debug.hpp>

#include <core/system/SystemEvent.hpp>

#include <rendering/backend/RendererInstance.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

namespace hyperion {
namespace sys {

#pragma region ApplicationWindow

ApplicationWindow::ApplicationWindow(ANSIString title, Vec2u size)
    : m_title(std::move(title)),
      m_size(size)
{
}

#pragma endregion ApplicationWindow

#pragma region SDLApplicationWindow

SDLApplicationWindow::SDLApplicationWindow(ANSIString title, Vec2u size)
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

Vec2u SDLApplicationWindow::GetDimensions() const
{
    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    return Vec2u { uint32(width), uint32(height) };
}

void SDLApplicationWindow::SetMouseLocked(bool locked)
{
    SDL_SetRelativeMouseMode(locked ? SDL_TRUE : SDL_FALSE);
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

UniquePtr<ApplicationWindow> SDLAppContext::CreateSystemWindow(WindowOptions window_options)
{
    UniquePtr<SDLApplicationWindow> window;
    window.Reset(new SDLApplicationWindow(window_options.title.Data(), window_options.size));
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
    : m_arguments(arguments),
      m_configuration("app")
{
    if (name == nullptr) {
        name = "HyperionApp";
    }

    m_name = std::move(name);
}

AppContext::~AppContext() = default;

void AppContext::SetMainWindow(UniquePtr<ApplicationWindow> &&window)
{
    m_main_window = std::move(window);

    OnCurrentWindowChanged.Broadcast(m_main_window.Get());
}

#pragma endregion AppContext

} // namespace sys
} // namespace hyperion