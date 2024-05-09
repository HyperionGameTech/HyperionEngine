#ifndef HYPERION_SYSTEM_EVENT_HPP
#define HYPERION_SYSTEM_EVENT_HPP

#include <input/Mouse.hpp>

#include <core/utilities/Variant.hpp>
#include <core/filesystem/FilePath.hpp>

#include <SDL2/SDL.h>

#include <Types.hpp>

namespace hyperion {
namespace sys {

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
        Memory::MemSet(&other.sdl_event, 0x00, sizeof(SDL_Event));
    }

    SystemEvent &operator=(SystemEvent &&other) noexcept
    {
        // sdl_event better not have any leakable memory!
        // we free filedrop memory as soon as we receive the event...

        sdl_event = other.sdl_event;
        Memory::MemSet(&other.sdl_event, 0x00, sizeof(SDL_Event));

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

    KeyCode GetKeyCode() const;

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

    MouseButton GetMouseButton() const;

    void GetMouseWheel(int *x, int *y) const
        { *x = sdl_event.wheel.x; *y = sdl_event.wheel.y; }

    uint32 GetWindowId() const
        { return sdl_event.window.windowID; }

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

} // namespace sys

using sys::KeyCode;
using sys::SystemEvent;
using sys::SystemEventType;
using sys::SystemWindowEventType;

} // namespace hyperion

#endif