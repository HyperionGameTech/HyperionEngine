//
// Created by ethan on 2/5/22.
//

#include "SdlSystem.hpp"
#include "Debug.hpp"

#include <core/lib/CMemory.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>

#include "rendering/backend/RendererInstance.hpp"

namespace hyperion {

SDL_Event *SystemEvent::GetInternalEvent() {
    return &(this->sdl_event);
}

ApplicationWindow::ApplicationWindow(const ANSIString &title, UInt width, UInt height)
    : m_title(title),
      m_width(width),
      m_height(height)
{
}

SDLApplicationWindow::SDLApplicationWindow(const ANSIString &title, UInt width, UInt height)
    : ApplicationWindow(title, width, height),
      window(nullptr)
{
}

SDLApplicationWindow::~SDLApplicationWindow()
{
    SDL_DestroyWindow(this->window);
}

void SDLApplicationWindow::Initialize(WindowOptions window_options)
{
    this->window = SDL_CreateWindow(
        m_title.Data(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        Int(m_width), Int(m_height),
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | (window_options.enable_high_dpi ? SDL_WINDOW_ALLOW_HIGHDPI : 0)
    );

    // make sure to use SDL_free on file name strings for these events
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

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

void SDLApplicationWindow::SetMousePosition(Int x, Int y)
{
    SDL_WarpMouseInWindow(window, x, y);
}

MouseState SDLApplicationWindow::GetMouseState()
{
    MouseState mouse_state { };
    mouse_state.mask = SDL_GetMouseState(&mouse_state.x, &mouse_state.y);

    return mouse_state;
}

Extent2D SDLApplicationWindow::GetExtent() const
{
    Int width, height;
    SDL_GetWindowSize(window, &width, &height);

    return Extent2D { UInt32(width), UInt32(height) };
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

SDLApplication::SDLApplication(const char *name)
    : Application(name)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
}

SDLApplication::~SDLApplication()
{
    SDL_Quit();
}

UniquePtr<ApplicationWindow> SDLApplication::CreateSystemWindow(WindowOptions window_options)
{
    UniquePtr<SDLApplicationWindow> window;
    window.Reset(new SDLApplicationWindow(window_options.title.Data(), window_options.width, window_options.height));
    window->Initialize(window_options);

    return window;
}

Int SDLApplication::PollEvent(SystemEvent &event)
{
    const Int result = SDL_PollEvent(event.GetInternalEvent());

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
bool SDLApplication::GetVkExtensions(Array<const char *> &out_extensions) const
{
    UInt32 num_extensions = 0;
    SDL_Window *window = static_cast<SDLApplicationWindow *>(m_current_window.Get())->GetInternalWindow();

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

Application::Application(const char *name)
{
    if (name == nullptr) {
        name = "HyperionApp";
    }

    const SizeType len = Memory::StrLen(name);

    m_name = new char[len + 1];
    Memory::MemCpy(m_name, name, len);
    m_name[len] = '\0';
}

Application::~Application()
{
    delete[] m_name;
}

} // namespace hyperion