#pragma once
#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

#include <core/utilities/Variant.hpp>
#include <core/filesystem/FilePath.hpp>

#include <core/math/Vector2.hpp>

#ifdef HYP_SDL
#include <SDL2/SDL.h>
#endif

#include <core/Types.hpp>

namespace hyperion {
namespace sys {

class ApplicationWindow;

#ifdef HYP_WINDOWS
struct Win32Event
{
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
};
#endif

#ifdef HYP_MACOS
struct CocoaEvent
{
    void* nsEvent; // NSEvent* (bridged)
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

#ifdef HYP_MACOS
    CocoaEvent cocoaEvent;
#endif
};

class HYP_API SystemEvent final
{
public:
    using EventData = Variant<EnumFlags<MouseButtonState>, KeyCode, FilePath, Vec2i, void*>;

    enum EventType : uint32
    {
        INVALID = ~0u,

        WINDOW_EVENT = 0x0200,
        SHUTDOWN = 0x0100,

        KEYDOWN = 0x0300,
        KEYUP = 0x0301,

        MOUSEMOTION = 0x0400,
        MOUSEBUTTON_DOWN = 0x0401,
        MOUSEBUTTON_UP = 0x0402,
        MOUSESCROLL = 0x0403,

        FILE_DROP = 0x1000,

        WINDOW_MOVED = 0x0204,
        WINDOW_RESIZED = 0x0205,

        WINDOW_FOCUS_GAINED = 0x020C,
        WINDOW_FOCUS_LOST = 0x020D,

        WINDOW_CLOSE = 0x0203,
        WINDOW_MINIMIZED = 0x0206
    };

    SystemEvent()
        : m_eventType(EventType::INVALID),
          m_window(nullptr),
          m_platformEvent(),
          m_eventData()
    {
        Memory::MemSet(&m_platformEvent, 0x0, sizeof(PlatformEvent));
    }

    SystemEvent(EventType eventType, ApplicationWindow* window, PlatformEvent platformEvent)
        : m_eventType(eventType),
          m_window(window),
          m_platformEvent(platformEvent)
    {
    }

    SystemEvent(const SystemEvent& other) = delete;
    SystemEvent& operator=(const SystemEvent& other) = delete;

    SystemEvent(SystemEvent&& other) noexcept
        : m_eventType(other.m_eventType),
          m_window(other.m_window),
          m_platformEvent(other.m_platformEvent),
          m_eventData(std::move(other.m_eventData))
    {
        other.m_eventType = EventType::INVALID;
        other.m_window = nullptr;

        m_platformEvent = other.m_platformEvent;
        Memory::MemSet(&other.m_platformEvent, 0x0, sizeof(PlatformEvent));
    }

    SystemEvent& operator=(SystemEvent&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_eventType = other.m_eventType;
        m_window = other.m_window;
        m_platformEvent = other.m_platformEvent;

        m_eventData = std::move(other.m_eventData);

        other.m_eventType = EventType::INVALID;
        other.m_window = nullptr;

        Memory::MemSet(&other.m_platformEvent, 0x0, sizeof(PlatformEvent));

        return *this;
    }

    ~SystemEvent();

    HYP_FORCE_INLINE EventType GetType() const
    {
        return m_eventType;
    }

    HYP_FORCE_INLINE ApplicationWindow* GetWindow() const
    {
        return m_window;
    }

    HYP_FORCE_INLINE KeyCode GetKeyCode() const
    {
        const KeyCode* keyCode = m_eventData.TryGet<KeyCode>();
        AssertDebug(keyCode != nullptr);

        if (!keyCode)
        {
            return KeyCode::UNKNOWN;
        }

        return *keyCode;
    }

    HYP_FORCE_INLINE EnumFlags<MouseButtonState> GetMouseButtons() const
    {
        const EnumFlags<MouseButtonState>* mouseButtonState = m_eventData.TryGet<EnumFlags<MouseButtonState>>();
        AssertDebug(mouseButtonState != nullptr);

        if (!mouseButtonState)
        {
            return MouseButtonState::NONE;
        }

        return *mouseButtonState;
    }

    HYP_FORCE_INLINE Vec2i GetWindowResizeDimensions() const
    {
        if (m_eventType != WINDOW_RESIZED)
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

    HYP_FORCE_INLINE Vec2i GetMouseWheel() const
    {
        if (m_eventType != MOUSESCROLL)
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
    EventType m_eventType;
    ApplicationWindow* m_window;
    PlatformEvent m_platformEvent;
    EventData m_eventData;
};

// for backwards compatibility
enum SystemEventType : uint32
{
    SYSTEM_EVENT_INVALID = SystemEvent::INVALID,
    SYSTEM_EVENT_WINDOW_EVENT = SystemEvent::WINDOW_EVENT,
    SYSTEM_EVENT_SHUTDOWN = SystemEvent::SHUTDOWN,
    SYSTEM_EVENT_KEYDOWN = SystemEvent::KEYDOWN,
    SYSTEM_EVENT_KEYUP = SystemEvent::KEYUP,
    SYSTEM_EVENT_MOUSEMOTION = SystemEvent::MOUSEMOTION,
    SYSTEM_EVENT_MOUSEBUTTON_DOWN = SystemEvent::MOUSEBUTTON_DOWN,
    SYSTEM_EVENT_MOUSEBUTTON_UP = SystemEvent::MOUSEBUTTON_UP,
    SYSTEM_EVENT_MOUSESCROLL = SystemEvent::MOUSESCROLL,
    SYSTEM_EVENT_FILE_DROP = SystemEvent::FILE_DROP,
    SYSTEM_EVENT_WINDOW_MOVED = SystemEvent::WINDOW_MOVED,
    SYSTEM_EVENT_WINDOW_RESIZED = SystemEvent::WINDOW_RESIZED,
    SYSTEM_EVENT_WINDOW_FOCUS_GAINED = SystemEvent::WINDOW_FOCUS_GAINED,
    SYSTEM_EVENT_WINDOW_FOCUS_LOST = SystemEvent::WINDOW_FOCUS_LOST,
    SYSTEM_EVENT_WINDOW_CLOSE = SystemEvent::WINDOW_CLOSE,
    SYSTEM_EVENT_WINDOW_MINIMIZED = SystemEvent::WINDOW_MINIMIZED
};

} // namespace sys

using sys::PlatformEvent;
using sys::SystemEvent;
using sys::SystemEventType;

#ifdef HYP_WINDOWS
using sys::Win32Event;
#endif

#ifdef HYP_MACOS
using sys::CocoaEvent;
#endif

} // namespace hyperion
