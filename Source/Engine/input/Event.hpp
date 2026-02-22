#pragma once
#include <input/Mouse.hpp>
#include <input/Keyboard.hpp>

#include <Core/utilities/Variant.hpp>
#include <Core/filesystem/FilePath.hpp>

#include <Core/math/Vector2.hpp>

#ifdef HYP_SDL
#include <SDL2/SDL.h>
#endif

#include <Core/Types.hpp>

namespace Hyperion {

HYP_ENUM()
enum class EventFlags : uint8
{
    NONE = 0x0,
    RELATIVE_MOUSE = 0x1
};

HYP_MAKE_ENUM_FLAGS(EventFlags);

HYP_ENUM()
enum class EventType : uint32
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

class HYP_API Event final
{
public:
    using EventData = Variant<EnumFlags<MouseButtonState>, KeyCode, FilePath, Vec2i, Vec2f, void*>;

    Event()
        : m_eventType(EventType::INVALID),
          m_flags(EventFlags::NONE),
          m_window(nullptr),
          m_platformEvent(),
          m_eventData(),
          m_timestamp(Time(0))
    {
        Memory::Fill(&m_platformEvent, 0x0, sizeof(PlatformEvent));
    }

    Event(EventType eventType, ApplicationWindow* window, PlatformEvent platformEvent)
        : m_eventType(eventType),
          m_flags(EventFlags::NONE),
          m_window(window),
          m_platformEvent(platformEvent),
          m_timestamp(Time::Now())
    {
    }

    Event(const Event& other) = delete;
    Event& operator=(const Event& other) = delete;

    Event(Event&& other) noexcept
        : m_eventType(other.m_eventType),
          m_flags(other.m_flags),
          m_window(other.m_window),
          m_platformEvent(other.m_platformEvent),
          m_eventData(std::move(other.m_eventData)),
          m_timestamp(other.m_timestamp)
    {
        other.m_eventType = EventType::INVALID;
        other.m_flags = EventFlags::NONE;
        other.m_window = nullptr;
        other.m_timestamp = Time(0);

        m_platformEvent = other.m_platformEvent;
        Memory::Fill(&other.m_platformEvent, 0x0, sizeof(PlatformEvent));
    }

    Event& operator=(Event&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_eventType = other.m_eventType;
        m_flags = other.m_flags;
        m_window = other.m_window;
        m_platformEvent = other.m_platformEvent;
        m_timestamp = other.m_timestamp;

        m_eventData = std::move(other.m_eventData);

        other.m_eventType = EventType::INVALID;
        other.m_flags = EventFlags::NONE;
        other.m_window = nullptr;
        other.m_timestamp = Time(0);

        Memory::Fill(&other.m_platformEvent, 0x0, sizeof(PlatformEvent));

        return *this;
    }

    ~Event();

    HYP_FORCE_INLINE EventType GetType() const
    {
        return m_eventType;
    }

    HYP_FORCE_INLINE EnumFlags<EventFlags> GetFlags() const
    {
        return m_flags;
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
            return KeyCode::KEY_UNKNOWN;
        }

        return *keyCode;
    }

    HYP_FORCE_INLINE EnumFlags<MouseButtonState> GetMouseButtons() const
    {
        const EnumFlags<MouseButtonState>* mouseButtonState = m_eventData.TryGet<EnumFlags<MouseButtonState>>();
        // AssertDebug(mouseButtonState != nullptr);

        if (!mouseButtonState)
        {
            return MouseButtonState::NONE;
        }

        return *mouseButtonState;
    }

    HYP_FORCE_INLINE const Time& GetTimestamp() const
    {
        return m_timestamp;
    }

    MouseEvent ToMouseEvent() const;
    MouseEvent ToMouseEvent(const Vec2f& offsetMousePos, const Vec2f& surfaceSize) const;

    KeyboardEvent ToKeyboardEvent() const;

    Vec2i GetWindowResizeDimensions() const
    {
        if (m_eventType != EventType::WINDOW_RESIZED)
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
        if (m_eventType != EventType::MOUSESCROLL)
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

    HYP_FORCE_INLINE bool IsAbsoluteMousePosition() const
    {
        return m_eventType == EventType::MOUSEMOTION && m_eventData.Is<Vec2i>();
    }

    HYP_FORCE_INLINE Vec2f GetMousePositionDeltas() const
    {
        if (m_eventType != EventType::MOUSEMOTION)
        {
            return Vec2f::Zero();
        }

        const Vec2f* deltas = m_eventData.TryGet<Vec2f>();
        AssertDebug(deltas != nullptr);

        if (!deltas)
        {
            return Vec2f::Zero();
        }

        return *deltas;
    }

    HYP_FORCE_INLINE Vec2i GetMousePosition() const
    {
        if (m_eventType != EventType::MOUSEMOTION)
        {
            return Vec2i::Zero();
        }

        const Vec2i* pos = m_eventData.TryGet<Vec2i>();
        AssertDebug(pos != nullptr);

        if (!pos)
        {
            return Vec2i::Zero();
        }

        return *pos;
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
    EnumFlags<EventFlags> m_flags;
    ApplicationWindow* m_window;
    PlatformEvent m_platformEvent;
    EventData m_eventData;
    Time m_timestamp;
};

} // namespace Hyperion
