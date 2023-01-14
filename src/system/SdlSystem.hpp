//
// Created by ethan on 2/5/22.
//

#ifndef HYPERION_SDL_SYSTEM_H
#define HYPERION_SDL_SYSTEM_H

#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL.h>

#include <core/Containers.hpp>
#include <util/fs/FsUtil.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>
#include <core/lib/FixedArray.hpp>

#include <rendering/backend/RendererStructs.hpp>

#include <vector>
#include <string>

namespace hyperion {

namespace renderer {
class Instance;
} // namespace renderer

using renderer::Extent2D;
using v2::FilePath;

enum SystemEventType
{
    EVENT_WINDOW_EVENT  = SDL_WINDOWEVENT,
    EVENT_KEYDOWN = SDL_KEYDOWN,
    EVENT_SHUTDOWN = SDL_QUIT,
    EVENT_KEYUP = SDL_KEYUP,

    EVENT_MOUSEMOTION = SDL_MOUSEMOTION,
    EVENT_MOUSEBUTTON_DOWN = SDL_MOUSEBUTTONDOWN,
    EVENT_MOUSEBUTTON_UP = SDL_MOUSEBUTTONUP,
    EVENT_MOUSESCROLL = SDL_MOUSEWHEEL,

    EVENT_FILE_DROP = SDL_DROPFILE
};

enum SystemWindowEventType
{
    EVENT_WINDOW_MOVED = SDL_WINDOWEVENT_MOVED,
    EVENT_WINDOW_RESIZED = SDL_WINDOWEVENT_RESIZED,

    EVENT_WINDOW_FOCUS_GAINED = SDL_WINDOWEVENT_FOCUS_GAINED,
    EVENT_WINDOW_FOCUS_LOST = SDL_WINDOWEVENT_FOCUS_LOST,

    EVENT_WINDOW_CLOSE = SDL_WINDOWEVENT_CLOSE,
    EVENT_WINDOW_MINIMIZED = SDL_WINDOWEVENT_MINIMIZED,
};

enum SystemCursorType
{
    SYSTEM_CURSOR_DEFAULT = SDL_SYSTEM_CURSOR_ARROW,
    SYSTEM_CURSOR_IBEAM,
    SYSTEM_CURSOR_WAIT,
    SYSTEM_CURSOR_CROSSHAIR,
    SYSTEM_CURSOR_WAIT_ARROW,
    SYSTEM_CURSOR_SIZE_NWSE,
    SYSTEM_CURSOR_SIZE_NESW,
    SYSTEM_CURSOR_SIZE_HORIZONTAL,
    SYSTEM_CURSOR_SIZE_VERTICAL,
    SYSTEM_CURSOR_SIZE_ALL,
    SYSTEM_CURSOR_NO,
    SYSTEM_CURSOR_HAND,

    SYSTEM_CURSOR_MAX,
};

enum SystemKey
{
    KEY_UNKNOWN = -1,

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

    KEY_F1 = SDL_SCANCODE_F1,
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

    KEY_LEFT_SHIFT = SDL_SCANCODE_LSHIFT,
    KEY_LEFT_CTRL = SDL_SCANCODE_LCTRL,
    KEY_LEFT_ALT = SDL_SCANCODE_LALT,
    KEY_RIGHT_SHIFT = SDL_SCANCODE_RSHIFT,
    KEY_RIGHT_CTRL = SDL_SCANCODE_RCTRL,
    KEY_RIGHT_ALT = SDL_SCANCODE_RALT,

    KEY_SPACE = SDL_SCANCODE_SPACE,
    KEY_PERIOD = 46,
    KEY_RETURN = 257,
    KEY_TAB = 258,
    KEY_BACKSPACE = 259,
    KEY_CAPSLOCK = 280,

    KEY_ARROW_RIGHT = SDL_SCANCODE_RIGHT,
    KEY_ARROW_LEFT = SDL_SCANCODE_LEFT,
    KEY_ARROW_DOWN = SDL_SCANCODE_DOWN,
    KEY_ARROW_UP = SDL_SCANCODE_UP,
};

enum MouseButton
{
    MOUSE_BUTTON_LEFT = SDL_BUTTON_LEFT,
    MOUSE_BUTTON_MIDDLE,
    MOUSE_BUTTON_RIGHT,
};

using MouseButtonMask = UInt32;

class SystemEvent
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

    UInt16 GetKeyCode() const { return sdl_event.key.keysym.sym; }
    UInt8 GetMouseButton() const { return sdl_event.button.button; }
    void GetMouseWheel(int *x, int *y) const { *x = sdl_event.wheel.x; *y = sdl_event.wheel.y; }
    UInt32 GetWindowId() const { return sdl_event.window.windowID; }

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
    Int x;
    Int y;
};

class SystemCursor
{
public:
    SystemCursor(const SystemCursorType cursor_id);
};

class SDLSystemCursor
{
public:
    SDLSystemCursor(SystemCursorType cursor_id)
    {
        m_cursor = SDL_CreateSystemCursor((SDL_SystemCursor)cursor_id);
    }

    SDL_Cursor *GetInternalCursor() const
    {
        return m_cursor;
    }

    void Destroy()
    {
        SDL_FreeCursor(m_cursor);
        m_cursor = nullptr;
    }

private:
    SDL_Cursor *m_cursor = nullptr;
};

class ApplicationWindow
{
public:
    ApplicationWindow(const ANSIString &title, UInt width, UInt height);
    ApplicationWindow(const ApplicationWindow &other) = delete;
    ApplicationWindow &operator=(const ApplicationWindow &other) = delete;
    virtual ~ApplicationWindow() = default;

    virtual void SetCursor(SystemCursorType cursor_id) const = 0;

    virtual void SetMousePosition(Int x, Int y) = 0;
    virtual MouseState GetMouseState() = 0;

    virtual Extent2D GetExtent() const = 0;

    virtual void SetMouseLocked(bool locked) = 0;
    virtual bool HasMouseFocus() const = 0;

#ifdef HYP_VULKAN
    virtual VkSurfaceKHR CreateVkSurface(renderer::Instance *instance) = 0;
#endif

protected:
    ANSIString m_title;
    UInt m_width;
    UInt m_height;
};

class SDLApplicationWindow : public ApplicationWindow
{
public:
    SDLApplicationWindow(const ANSIString &title, UInt width, UInt height);
    virtual ~SDLApplicationWindow() override;

    void SetCursor(SDLSystemCursor &cursor) const;
    virtual void SetCursor(SystemCursorType cursor_id) const override;

    virtual void SetMousePosition(Int x, Int y) override;
    virtual MouseState GetMouseState() override;

    virtual Extent2D GetExtent() const override;

    virtual void SetMouseLocked(bool locked) override;
    virtual bool HasMouseFocus() const override;

    void Initialize();

    SDL_Window *GetInternalWindow()
        { return window; }

    const SDL_Window *GetInternalWindow() const
        { return window; }

#ifdef HYP_VULKAN
    virtual VkSurfaceKHR CreateVkSurface(renderer::Instance *instance) override;
#endif
    
private:
    SDL_Window *window = nullptr;
    SDLSystemCursor *m_current_cursor = nullptr;
    FixedArray<SDLSystemCursor, SYSTEM_CURSOR_MAX> m_system_cursors;
};

class Application
{
public:
    Application(const char *name);
    virtual ~Application();

    const char *GetAppName() const
        { return m_name; }

    ApplicationWindow *GetCurrentWindow()
        { return m_current_window.Get(); }

    const ApplicationWindow *GetCurrentWindow() const
        { return m_current_window.Get(); }

    void SetCurrentWindow(UniquePtr<ApplicationWindow> &&window)
        { m_current_window = std::move(window); }

    virtual UniquePtr<ApplicationWindow> CreateSystemWindow(const ANSIString &title, UInt width, UInt height) = 0;
    virtual Int PollEvent(SystemEvent &event) = 0;

#ifdef HYP_VULKAN
    virtual bool GetVkExtensions(Array<const char *> &out_extensions) const = 0;
#endif
    
protected:
    UniquePtr<ApplicationWindow> m_current_window;
    char *m_name;
};

class SDLApplication : public Application
{
public:
    SDLApplication(const char *name);
    virtual ~SDLApplication() override;

    virtual UniquePtr<ApplicationWindow> CreateSystemWindow(const ANSIString &title, UInt width, UInt height) override;
    virtual Int PollEvent(SystemEvent &event) override;

#ifdef HYP_VULKAN
    virtual bool GetVkExtensions(Array<const char *> &out_extensions) const override;
#endif
};

} // namespace hyperion

#endif //HYPERION_SDL_SYSTEM_H
