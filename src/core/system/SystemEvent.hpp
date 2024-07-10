#ifndef HYPERION_SYSTEM_EVENT_HPP
#define HYPERION_SYSTEM_EVENT_HPP

#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

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

class HYP_API SystemEvent
{
public:
    using EventData = Variant<FilePath, void *>;

    SystemEvent() = default;
    SystemEvent(const SystemEvent &other)               = delete;
    SystemEvent &operator=(const SystemEvent &other)    = delete;

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
        if (uint32(key) >= 'a' && uint32(key) <= 'z') {
            key = KeyCode('A' + (uint32(key) - 'a'));
        }

        return key;
    }

    EnumFlags<MouseButtonState> GetMouseButtons() const;

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

using sys::SystemEvent;
using sys::SystemEventType;
using sys::SystemWindowEventType;

} // namespace hyperion

#endif