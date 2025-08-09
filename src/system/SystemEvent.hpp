#pragma once
#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

#include <core/utilities/Variant.hpp>
#include <core/filesystem/FilePath.hpp>

#include <core/math/Vector2.hpp>

#include <SDL2/SDL.h>

#include <core/Types.hpp>

namespace hyperion {
namespace sys {

enum SystemEventType : uint32
{
    EVENT_INVALID = ~0u,

    EVENT_WINDOW_EVENT = SDL_WINDOWEVENT,
    EVENT_SHUTDOWN = SDL_QUIT,

    EVENT_KEYDOWN = SDL_KEYDOWN,
    EVENT_KEYUP = SDL_KEYUP,

    EVENT_MOUSEMOTION = SDL_MOUSEMOTION,
    EVENT_MOUSEBUTTON_DOWN = SDL_MOUSEBUTTONDOWN,
    EVENT_MOUSEBUTTON_UP = SDL_MOUSEBUTTONUP,
    EVENT_MOUSESCROLL = SDL_MOUSEWHEEL,

    EVENT_FILE_DROP = SDL_DROPFILE,
    
    EVENT_WINDOW_MOVED = SDL_WINDOWEVENT_MOVED,
    EVENT_WINDOW_RESIZED = SDL_WINDOWEVENT_RESIZED,

    EVENT_WINDOW_FOCUS_GAINED = SDL_WINDOWEVENT_FOCUS_GAINED,
    EVENT_WINDOW_FOCUS_LOST = SDL_WINDOWEVENT_FOCUS_LOST,

    EVENT_WINDOW_CLOSE = SDL_WINDOWEVENT_CLOSE,
    EVENT_WINDOW_MINIMIZED = SDL_WINDOWEVENT_MINIMIZED
};

#ifdef HYP_WINDOWS
struct Win32Event
{
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
};
#endif

union PlatformEvent
{
#ifdef HYP_SDL
    SDL_Event sdlEvent;
#endif

#ifdef HYP_WINDOWS
    Win32Event win32Event;
#endif
};

class HYP_API SystemEvent
{
public:
    using EventData = Variant<EnumFlags<MouseButtonState>, KeyCode, FilePath, Vec2i, void*>;

    SystemEvent()
        : m_eventType(SystemEventType::EVENT_INVALID),
          m_platformEvent(),
          m_eventData()
    {
        Memory::MemSet(&m_platformEvent, 0x0, sizeof(PlatformEvent));
    }

    SystemEvent(SystemEventType eventType, PlatformEvent platformEvent)
        : m_eventType(eventType),
          m_platformEvent(platformEvent)
    {
    }

    SystemEvent(const SystemEvent& other) = delete;
    SystemEvent& operator=(const SystemEvent& other) = delete;

    SystemEvent(SystemEvent&& other) noexcept
        : m_eventType(other.m_eventType),
          m_platformEvent(other.m_platformEvent),
          m_eventData(std::move(other.m_eventData))
    {
        m_platformEvent = other.m_platformEvent;
        Memory::MemSet(&other.m_platformEvent, 0x0, sizeof(PlatformEvent));

        other.m_eventType = SystemEventType::EVENT_INVALID;
    }

    SystemEvent& operator=(SystemEvent&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_eventType = other.m_eventType;
        m_platformEvent = other.m_platformEvent;

        m_eventData = std::move(other.m_eventData);

        Memory::MemSet(&other.m_platformEvent, 0x0, sizeof(PlatformEvent));

        other.m_eventType = SystemEventType::EVENT_INVALID;

        return *this;
    }

    ~SystemEvent() = default;

    SystemEventType GetType() const
    {
        return m_eventType;
    }

    KeyCode GetKeyCode() const
    {
        const KeyCode* keyCode = m_eventData.TryGet<KeyCode>();
        AssertDebug(keyCode != nullptr);

        if (!keyCode)
        {
            return KeyCode::UNKNOWN;
        }

        return *keyCode;
    }

    EnumFlags<MouseButtonState> GetMouseButtons() const
    {
        const EnumFlags<MouseButtonState>* mouseButtonState = m_eventData.TryGet<EnumFlags<MouseButtonState>>();
        AssertDebug(mouseButtonState != nullptr);

        if (!mouseButtonState)
        {
            return MouseButtonState::NONE;
        }

        return *mouseButtonState;
    }

    Vec2i GetWindowResizeDimensions() const
    {
        if (m_eventType != SystemEventType::EVENT_WINDOW_RESIZED)
        {
            return Vec2i::Zero();
        }

        const Vec2i* dimensions = m_eventData.TryGet<Vec2i>();
        AssertDebug(dimensions != nullptr);

        if (!dimensions)
        {
            return Vec2i::Zero();
        }

        return *dimensions;
    }

    Vec2i GetMouseWheel() const
    {
        if (m_eventType != SystemEventType::EVENT_MOUSESCROLL)
        {
            return Vec2i::Zero();
        }

        const Vec2i* mouseWheel = m_eventData.TryGet<Vec2i>();
        AssertDebug(mouseWheel != nullptr);

        if (!mouseWheel)
        {
            return Vec2i::Zero();
        }

        return *mouseWheel;
    }

    HYP_FORCE_INLINE PlatformEvent& GetPlatformEvent()
    {
        return m_platformEvent;
    }

    HYP_FORCE_INLINE const PlatformEvent& GetPlatformEvent() const
    {
        return m_platformEvent;
    }

    HYP_FORCE_INLINE EventData& GetEventData()
    {
        return m_eventData;
    }

    HYP_FORCE_INLINE const EventData& GetEventData() const
    {
        return m_eventData;
    }

private:
    SystemEventType m_eventType;

    PlatformEvent m_platformEvent;

    EventData m_eventData;
};

} // namespace sys

using sys::SystemEvent;
using sys::SystemEventType;

} // namespace hyperion
