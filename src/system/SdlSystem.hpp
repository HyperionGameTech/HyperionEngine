//
// Created by ethan on 2/5/22.
//

#ifndef HYPERION_SDL_SYSTEM_H
#define HYPERION_SDL_SYSTEM_H

#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL.h>
#include <vector>
#include <string>

#include "Util.hpp"

namespace hyperion {

using ::std::vector;

enum SystemEventType {
    EVENT_WINDOW_EVENT  = SDL_WINDOWEVENT,
    EVENT_KEYDOWN       = SDL_KEYDOWN,
    EVENT_SHUTDOWN      = SDL_QUIT,
    EVENT_KEYUP         = SDL_KEYUP,

    EVENT_MOUSEMOTION   = SDL_MOUSEMOTION,
    EVENT_MOUSEBUTTON_DOWN = SDL_MOUSEBUTTONDOWN,
    EVENT_MOUSEBUTTON_UP = SDL_MOUSEBUTTONUP,
    EVENT_MOUSESCROLL    = SDL_MOUSEWHEEL
};

enum SystemWindowEventType {
    EVENT_WINDOW_MOVED = SDL_WINDOWEVENT_MOVED,
    EVENT_WINDOW_RESIZED = SDL_WINDOWEVENT_RESIZED,

    EVENT_WINDOW_FOCUS_GAINED = SDL_WINDOWEVENT_FOCUS_GAINED,
    EVENT_WINDOW_FOCUS_LOST = SDL_WINDOWEVENT_FOCUS_LOST,

    EVENT_WINDOW_CLOSE = SDL_WINDOWEVENT_CLOSE,
    EVENT_WINDOW_MINIMIZED = SDL_WINDOWEVENT_MINIMIZED,
};

enum SystemKey {
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
    KEY_ARROW_LEFT  = SDL_SCANCODE_LEFT,
    KEY_ARROW_DOWN  = SDL_SCANCODE_DOWN,
    KEY_ARROW_UP    = SDL_SCANCODE_UP,
};

enum MouseButton {
    MOUSE_BUTTON_LEFT = SDL_BUTTON_LEFT,
    MOUSE_BUTTON_MIDDLE,
    MOUSE_BUTTON_RIGHT,
};

using MouseButtonMask = uint32_t;


class SystemEvent {
public:
    inline SystemEventType GetType() {
        uint32_t event_type = this->GetInternalEvent()->type;
        return (SystemEventType)event_type;
    }
    inline SystemWindowEventType GetWindowEventType() {
        uint32_t event_type = this->GetInternalEvent()->window.event;
        return (SystemWindowEventType)event_type;
    }

    inline uint16_t GetKeyCode()              { return this->GetInternalEvent()->key.keysym.sym; }
    inline uint8_t  GetMouseButton()          { return this->GetInternalEvent()->button.button; }
    inline void GetMouseWheel(int *x, int *y) { *x = this->GetInternalEvent()->wheel.x; *y = this->GetInternalEvent()->wheel.y; }
    inline uint32_t GetWindowId()             { return this->GetInternalEvent()->window.windowID; }

    inline void GetWindowResizeDimensions(int *_width, int *_height) {
        SDL_Event *ev = this->GetInternalEvent();
        (*_width) = ev->window.data1;
        (*_height) = ev->window.data2;
    }

    SDL_Event *GetInternalEvent();

private:
    SDL_Event sdl_event;
};

class SystemWindow {
public:
    SystemWindow(const char *_title, int _width, int _height);
    SystemWindow(const SystemWindow &other) = delete;

    SystemWindow &operator=(const SystemWindow &other) = delete;

    void SetMousePosition(int x, int y);
    MouseButtonMask GetMouseState(int *x, int *y);
    void Initialize();
    inline SDL_Window *GetInternalWindow() {
        AssertThrow(this->window != nullptr);
        return this->window;
    }
    inline void LockMouse(bool lock) {
        SDL_SetRelativeMouseMode(lock ? SDL_TRUE : SDL_FALSE);
    }
    inline void GetSize(int *_width, int *_height) {
        SDL_GetWindowSize(this->GetInternalWindow(), _width, _height);
    }
    VkSurfaceKHR CreateVulkanSurface(VkInstance instance);

    ~SystemWindow();

    const char *title;
    int width, height;
private:
    SDL_Window *window = nullptr;
};

class SystemSDL {
public:
    SystemSDL();

    static SystemWindow *CreateSystemWindow(const char *title, int width, int height);

    static int PollEvent(SystemEvent *result);
    static uint64_t GetTicks();
    void SetCurrentWindow(SystemWindow *window);
    SystemWindow *GetCurrentWindow();
    vector<const char *> GetVulkanExtensionNames();

    ~SystemSDL();

    static void ThrowError();

private:
    SystemWindow *current_window;
};

} // namespace hyperion

#endif //HYPERION_SDL_SYSTEM_H
