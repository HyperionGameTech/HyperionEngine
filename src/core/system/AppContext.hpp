/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_APP_CONTEXT_HPP
#define HYPERION_APP_CONTEXT_HPP

#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL.h>

#include <core/Containers.hpp>
#include <core/functional/Delegate.hpp>
#include <core/filesystem/FilePath.hpp>
#include <core/system/ArgParse.hpp>
#include <core/Defines.hpp>

#include <rendering/backend/Platform.hpp>

#include <Types.hpp>


namespace hyperion {

namespace renderer {
namespace platform {

// forward declare Instance
template <PlatformType PLATFORM>
class Instance;

} // namespace platform

using Instance = platform::Instance<Platform::CURRENT>;

} // namespace renderer

namespace sys {

using WindowFlags = uint32;

enum WindowFlagBits : WindowFlags
{
    WINDOW_FLAGS_NONE       = 0x0,
    WINDOW_FLAGS_HEADLESS   = 0x1,
    WINDOW_FLAGS_NO_GFX     = 0x2,
    WINDOW_FLAGS_HIGH_DPI   = 0x4
};

struct WindowOptions
{
    ANSIString  title;
    Vec2u       size;
    WindowFlags flags = WINDOW_FLAGS_NONE;
};

enum SystemEventType
{
    EVENT_WINDOW_EVENT      = SDL_WINDOWEVENT,
    EVENT_KEYDOWN           = SDL_KEYDOWN,
    EVENT_SHUTDOWN          = SDL_QUIT,
    EVENT_KEYUP             = SDL_KEYUP,

    EVENT_MOUSEMOTION       = SDL_MOUSEMOTION,
    EVENT_MOUSEBUTTON_DOWN  = SDL_MOUSEBUTTONDOWN,
    EVENT_MOUSEBUTTON_UP    = SDL_MOUSEBUTTONUP,
    EVENT_MOUSESCROLL       = SDL_MOUSEWHEEL,

    EVENT_FILE_DROP         = SDL_DROPFILE
};

enum SystemWindowEventType
{
    EVENT_WINDOW_MOVED          = SDL_WINDOWEVENT_MOVED,
    EVENT_WINDOW_RESIZED        = SDL_WINDOWEVENT_RESIZED,

    EVENT_WINDOW_FOCUS_GAINED   = SDL_WINDOWEVENT_FOCUS_GAINED,
    EVENT_WINDOW_FOCUS_LOST     = SDL_WINDOWEVENT_FOCUS_LOST,

    EVENT_WINDOW_CLOSE          = SDL_WINDOWEVENT_CLOSE,
    EVENT_WINDOW_MINIMIZED      = SDL_WINDOWEVENT_MINIMIZED,
};

enum KeyCode : uint16
{
    KEY_UNKNOWN = uint16(-1),

    KEY_A = 'A',
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,

    KEY_0 = '0',
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,

    KEY_F1 = 58, // SDL_SCANCODE_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,

    KEY_LEFT_SHIFT = 225, // SDL_SCANCODE_LSHIFT,
    KEY_LEFT_CTRL = 224, // SDL_SCANCODE_LCTRL,
    KEY_LEFT_ALT = 226, // SDL_SCANCODE_LALT
    KEY_RIGHT_SHIFT = 229, // SDL_SCANCODE_RSHIFT,
    KEY_RIGHT_CTRL = 228, // SDL_SCANCODE_RCTRL,
    KEY_RIGHT_ALT = 230, // SDL_SCANCODE_RALT

    KEY_SPACE = 44, // SDL_SCANCODE_SPACE,
    KEY_PERIOD = 46,
    KEY_RETURN = 257,
    KEY_TAB = 258,
    KEY_BACKSPACE = 259,
    KEY_CAPSLOCK = 280,

    KEY_ARROW_RIGHT = 79,
    KEY_ARROW_LEFT = 80,
    KEY_ARROW_DOWN = 81,
    KEY_ARROW_UP = 82,
};

enum MouseButtonName
{
    MBN_LEFT = SDL_BUTTON_LEFT,
    MBN_MIDDLE,
    MBN_RIGHT,
};

using MouseButtonMask = uint32;

class HYP_API SystemEvent
{
public:
    using EventData = Variant<FilePath, void *>;

    SystemEvent() = default;
    SystemEvent(const SystemEvent &other) = delete;
    SystemEvent &operator=(const SystemEvent &other) = delete;

    SystemEvent(SystemEvent &&other) noexcept
        : sdl_event(other.sdl_event),
          m_event_data(std::move(other.m_event_data))
    {
        std::memset(&other.sdl_event, 0x00, sizeof(SDL_Event));
    }

    SystemEvent &operator=(SystemEvent &&other) noexcept
    {
        // sdl_event better not have any leakable memory!
        // we free filedrop memory as soon as we receive the event...

        sdl_event = other.sdl_event;
        std::memset(&other.sdl_event, 0x00, sizeof(SDL_Event));

        m_event_data = std::move(other.m_event_data);

        return *this;
    }

    ~SystemEvent() = default;

    SystemEventType GetType() const
    {
        return (SystemEventType)sdl_event.type;
    }

    SystemWindowEventType GetWindowEventType() const
    {
        return (SystemWindowEventType)sdl_event.window.event;
    }

    KeyCode GetKeyCode() const
        { return KeyCode(sdl_event.key.keysym.sym); }

    /*! \brief For any characters a-z, returns the uppercase version.
     *  Otherwise, the result from GetKeyCode() is returned.
     */
    KeyCode GetNormalizedKeyCode() const
    {
        KeyCode key = GetKeyCode();
        
        /* Set all letters to uppercase */
        if (key >= 'a' && key <= 'z') {
            key = KeyCode('A' + (key - 'a'));
        }

        return key;
    }


    uint8 GetMouseButton() const { return sdl_event.button.button; }
    void GetMouseWheel(int *x, int *y) const { *x = sdl_event.wheel.x; *y = sdl_event.wheel.y; }
    uint32 GetWindowId() const { return sdl_event.window.windowID; }

    void GetWindowResizeDimensions(int *_width, int *_height)
    {
        SDL_Event *ev = this->GetInternalEvent();
        (*_width) = ev->window.data1;
        (*_height) = ev->window.data2;
    }

    SDL_Event *GetInternalEvent();

    EventData &GetEventData()
        { return m_event_data; }

    const EventData &GetEventData() const
        { return m_event_data; }

private:
    SDL_Event sdl_event;

    EventData m_event_data;
};

struct MouseState
{
    MouseButtonMask mask;
    int             x;
    int             y;
};

class HYP_API ApplicationWindow
{
public:
    ApplicationWindow(ANSIString title, Vec2u size);
    ApplicationWindow(const ApplicationWindow &other) = delete;
    ApplicationWindow &operator=(const ApplicationWindow &other) = delete;
    virtual ~ApplicationWindow() = default;

    virtual void SetMousePosition(int x, int y) = 0;
    virtual MouseState GetMouseState() = 0;

    virtual Vec2u GetDimensions() const = 0;

    virtual void SetMouseLocked(bool locked) = 0;
    virtual bool HasMouseFocus() const = 0;

#ifdef HYP_VULKAN
    virtual VkSurfaceKHR CreateVkSurface(renderer::Instance *instance) = 0;
#endif

protected:
    ANSIString              m_title;
    Vec2u                   m_size;
};

class HYP_API SDLApplicationWindow : public ApplicationWindow
{
public:
    SDLApplicationWindow(ANSIString title, Vec2u size);
    virtual ~SDLApplicationWindow() override;

    virtual void SetMousePosition(int x, int y) override;
    virtual MouseState GetMouseState() override;

    virtual Vec2u GetDimensions() const override;

    virtual void SetMouseLocked(bool locked) override;
    virtual bool HasMouseFocus() const override;
    
    void Initialize(WindowOptions);

    SDL_Window *GetInternalWindow()
        { return window; }

    const SDL_Window *GetInternalWindow() const
        { return window; }

#ifdef HYP_VULKAN
    virtual VkSurfaceKHR CreateVkSurface(renderer::Instance *instance) override;
#endif
    
private:
    SDL_Window *window = nullptr;
};

class HYP_API AppContext
{
public:
    AppContext(ANSIString name, const CommandLineArguments &arguments);
    virtual ~AppContext();

    const ANSIString &GetAppName() const
        { return m_name; }

    const CommandLineArguments &GetArguments() const
        { return m_arguments; }

    ApplicationWindow *GetCurrentWindow() const
        { return m_current_window.Get(); }

    void SetCurrentWindow(UniquePtr<ApplicationWindow> &&window);

    virtual UniquePtr<ApplicationWindow> CreateSystemWindow(WindowOptions) = 0;
    virtual int PollEvent(SystemEvent &event) = 0;

#ifdef HYP_VULKAN
    virtual bool GetVkExtensions(Array<const char *> &out_extensions) const = 0;
#endif

    Delegate<void, ApplicationWindow *> OnCurrentWindowChanged;
    
protected:
    UniquePtr<ApplicationWindow>        m_current_window;
    ANSIString                          m_name;
    CommandLineArguments                m_arguments;
};

class HYP_API SDLAppContext : public AppContext
{
public:
    SDLAppContext(ANSIString name, const CommandLineArguments &arguments);
    virtual ~SDLAppContext() override;

    virtual UniquePtr<ApplicationWindow> CreateSystemWindow(WindowOptions) override;

    virtual int PollEvent(SystemEvent &event) override;

#ifdef HYP_VULKAN
    virtual bool GetVkExtensions(Array<const char *> &out_extensions) const override;
#endif

};

} // namespace sys

using sys::KeyCode;
using sys::MouseButtonName;
using sys::MouseButtonMask;
using sys::MouseState;

using sys::WindowOptions;
using sys::WindowFlags;

using sys::SystemEvent;
using sys::SystemEventType;
using sys::SystemWindowEventType;

using sys::AppContext;
using sys::ApplicationWindow;

using sys::SDLAppContext;
using sys::SDLApplicationWindow;

} // namespace hyperion

#endif
