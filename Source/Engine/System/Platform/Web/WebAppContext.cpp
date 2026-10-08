/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <SystemPch.hpp>

#include <System/AppContext.hpp>

#include <Input/Event.hpp>

#include <Core/Threading/Mutex.hpp>

#include <emscripten/html5.h>

#include <cstring>

namespace Hyperion {

#pragma region Browser events

static constexpr const char* g_canvasSelector = "#canvas";

struct WebKeyMapping
{
    const char* code;
    KeyCode keyCode;
};

// KeyboardEvent.code values, which name the physical key whatever the layout
static const WebKeyMapping g_keyMappings[] = {
    { "Space", KeyCode::KEY_SPACE }, { "Enter", KeyCode::KEY_RETURN }, { "Tab", KeyCode::KEY_TAB },
    { "Backspace", KeyCode::KEY_BACKSPACE }, { "Escape", KeyCode::KEY_ESCAPE }, { "CapsLock", KeyCode::KEY_CAPSLOCK },
    { "ShiftLeft", KeyCode::KEY_LSHIFT }, { "ShiftRight", KeyCode::KEY_RSHIFT },
    { "ControlLeft", KeyCode::KEY_LCTRL }, { "ControlRight", KeyCode::KEY_RCTRL },
    { "AltLeft", KeyCode::KEY_LALT }, { "AltRight", KeyCode::KEY_RALT },
    { "ArrowLeft", KeyCode::KEY_LEFT }, { "ArrowRight", KeyCode::KEY_RIGHT }, { "ArrowUp", KeyCode::KEY_UP }, { "ArrowDown", KeyCode::KEY_DOWN },
    { "Quote", KeyCode::KEY_APOSTROPHE }, { "Comma", KeyCode::KEY_COMMA }, { "Minus", KeyCode::KEY_DASH }, { "Period", KeyCode::KEY_PERIOD },
    { "Slash", KeyCode::KEY_SLASH }, { "Semicolon", KeyCode::KEY_SEMICOLON }, { "Equal", KeyCode::KEY_EQUALS }, { "Backquote", KeyCode::KEY_TILDE }
};

static KeyCode MapWebKeyCode(const char* code)
{
    // "KeyA".."KeyZ", "Digit0".."Digit9", "F1".."F12"
    if (std::strncmp(code, "Key", 3) == 0 && code[3] >= 'A' && code[3] <= 'Z' && code[4] == 0)
    {
        return KeyCode(uint16(KeyCode::KEY_A) + uint16(code[3] - 'A'));
    }

    if (std::strncmp(code, "Digit", 5) == 0 && code[5] >= '0' && code[5] <= '9' && code[6] == 0)
    {
        return KeyCode(uint16(KeyCode::KEY_0) + uint16(code[5] - '0'));
    }

    if (code[0] == 'F' && code[1] >= '1' && code[1] <= '9')
    {
        const int functionKeyIndex = std::atoi(code + 1);

        if (functionKeyIndex >= 1 && functionKeyIndex <= 12)
        {
            return KeyCode(uint16(KeyCode::KEY_F1) + uint16(functionKeyIndex - 1));
        }
    }

    for (const WebKeyMapping& mapping : g_keyMappings)
    {
        if (std::strcmp(mapping.code, code) == 0)
        {
            return mapping.keyCode;
        }
    }

    return KeyCode::KEY_UNKNOWN;
}

static EnumFlags<MouseButtonState> MapWebMouseButton(unsigned short button)
{
    switch (button)
    {
    case 0:
        return MouseButtonState::LEFT;
    case 1:
        return MouseButtonState::MIDDLE;
    case 2:
        return MouseButtonState::RIGHT;
    default:
        return MouseButtonState::NONE;
    }
}

static EM_BOOL OnWebKeyEvent(int eventType, const EmscriptenKeyboardEvent* keyEvent, void* userData)
{
    WebAppContext* appContext = static_cast<WebAppContext*>(userData);
    ApplicationWindow* window = appContext->GetMainWindow();

    const KeyCode keyCode = MapWebKeyCode(keyEvent->code);

    if (window == nullptr || keyCode == KeyCode::KEY_UNKNOWN || (eventType == EMSCRIPTEN_EVENT_KEYDOWN && keyEvent->repeat))
    {
        return EM_FALSE;
    }

    Event event(eventType == EMSCRIPTEN_EVENT_KEYDOWN ? EventType::KEYDOWN : EventType::KEYUP, window, PlatformEvent {});
    event.GetEventData().Set(keyCode);

    appContext->EnqueueEvent(std::move(event));

    // the browser keeps its own shortcuts for function keys and anything with a modifier held
    const bool isBrowserShortcut = keyEvent->ctrlKey || keyEvent->metaKey || (keyCode >= KeyCode::KEY_F1 && keyCode <= KeyCode::KEY_F12);

    return isBrowserShortcut ? EM_FALSE : EM_TRUE;
}

static EM_BOOL OnWebMouseEvent(int eventType, const EmscriptenMouseEvent* mouseEvent, void* userData)
{
    WebAppContext* appContext = static_cast<WebAppContext*>(userData);
    WebApplicationWindow* window = static_cast<WebApplicationWindow*>(appContext->GetMainWindow());

    if (window == nullptr)
    {
        return EM_FALSE;
    }

    switch (eventType)
    {
    case EMSCRIPTEN_EVENT_MOUSEMOVE:
    {
        Event event(EventType::MOUSEMOTION, window, PlatformEvent {});

        if (window->HasPointerLock())
        {
            // a locked pointer has no position, only movement
            event.GetEventData().Set(MotionData { Vec2f::Zero(), Vec2f(float(mouseEvent->movementX), float(mouseEvent->movementY)), /* isAbsolute */ false });
        }
        else
        {
            const Vec2i position { int(mouseEvent->targetX), int(mouseEvent->targetY) };

            window->SetMousePosition(position);
            event.GetEventData().Set(MotionData { Vec2f(position), Vec2f::Zero(), /* isAbsolute */ true });
        }

        appContext->EnqueueEvent(std::move(event));

        return EM_TRUE;
    }
    case EMSCRIPTEN_EVENT_MOUSEDOWN: // fallthrough
    case EMSCRIPTEN_EVENT_MOUSEUP:
    {
        const EnumFlags<MouseButtonState> button = MapWebMouseButton(mouseEvent->button);

        if (!button)
        {
            return EM_FALSE;
        }

        Event event(eventType == EMSCRIPTEN_EVENT_MOUSEDOWN ? EventType::MOUSEBUTTON_DOWN : EventType::MOUSEBUTTON_UP, window, PlatformEvent {});
        event.GetEventData().Set(button);

        appContext->EnqueueEvent(std::move(event));

        return EM_TRUE;
    }
    default:
        return EM_FALSE;
    }
}

static EM_BOOL OnWebWheelEvent(int eventType, const EmscriptenWheelEvent* wheelEvent, void* userData)
{
    WebAppContext* appContext = static_cast<WebAppContext*>(userData);
    ApplicationWindow* window = appContext->GetMainWindow();

    if (window == nullptr)
    {
        return EM_FALSE;
    }

    // one notch either way, as the other platforms report it; browsers scroll down with a positive delta
    const auto toNotches = [](double delta)
    {
        return delta > 0.0 ? -1 : (delta < 0.0 ? 1 : 0);
    };

    Event event(EventType::MOUSESCROLL, window, PlatformEvent {});
    event.GetEventData().Set(Vec2i(toNotches(wheelEvent->deltaX), toNotches(wheelEvent->deltaY)));

    appContext->EnqueueEvent(std::move(event));

    return EM_TRUE;
}

static EM_BOOL OnWebFocusEvent(int eventType, const EmscriptenFocusEvent* focusEvent, void* userData)
{
    WebAppContext* appContext = static_cast<WebAppContext*>(userData);
    ApplicationWindow* window = appContext->GetMainWindow();

    if (window == nullptr)
    {
        return EM_FALSE;
    }

    appContext->EnqueueEvent(Event(eventType == EMSCRIPTEN_EVENT_FOCUS ? EventType::WINDOW_FOCUS_GAINED : EventType::WINDOW_FOCUS_LOST, window, PlatformEvent {}));

    return EM_FALSE;
}

static EM_BOOL OnWebPointerLockChange(int eventType, const EmscriptenPointerlockChangeEvent* pointerLockEvent, void* userData)
{
    WebAppContext* appContext = static_cast<WebAppContext*>(userData);
    WebApplicationWindow* window = static_cast<WebApplicationWindow*>(appContext->GetMainWindow());

    if (window != nullptr && window->OnPointerLockChanged(pointerLockEvent->isActive))
    {
        // The user left pointer lock, which the browser does on Escape without ever delivering the key.
        // Hand the game the Escape it would have seen, so it lets go of the mouse the way it does on desktop.
        for (const EventType eventType : { EventType::KEYDOWN, EventType::KEYUP })
        {
            Event event(eventType, window, PlatformEvent {});
            event.GetEventData().Set(KeyCode::KEY_ESCAPE);

            appContext->EnqueueEvent(std::move(event));
        }
    }

    return EM_FALSE;
}

static void RegisterBrowserEvents(WebAppContext* appContext)
{
    // callbacks are delivered to the thread that registers them, which is the one that polls the queue
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, appContext, true, &OnWebKeyEvent);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, appContext, true, &OnWebKeyEvent);

    emscripten_set_mousemove_callback(g_canvasSelector, appContext, true, &OnWebMouseEvent);
    emscripten_set_mousedown_callback(g_canvasSelector, appContext, true, &OnWebMouseEvent);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, appContext, true, &OnWebMouseEvent);
    emscripten_set_wheel_callback(g_canvasSelector, appContext, true, &OnWebWheelEvent);

    emscripten_set_focus_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, appContext, true, &OnWebFocusEvent);
    emscripten_set_blur_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, appContext, true, &OnWebFocusEvent);

    emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, appContext, true, &OnWebPointerLockChange);
}

#pragma endregion Browser events

#pragma region WebApplicationWindow

WebApplicationWindow::WebApplicationWindow(ANSIString title, Vec2i size)
    : ApplicationWindow(std::move(title), size)
{
}

WebApplicationWindow::~WebApplicationWindow() = default;

void WebApplicationWindow::SetMousePosition(Vec2i position)
{
    m_mousePosition = position;
}

Vec2i WebApplicationWindow::GetMousePosition() const
{
    return m_mousePosition;
}

Vec2i WebApplicationWindow::GetDimensions() const
{
    return m_size;
}

void WebApplicationWindow::SetIsMouseLocked(bool locked)
{
    if (locked == m_isMouseLocked)
    {
        return;
    }

    m_isMouseLocked = locked;

    if (locked)
    {
        // browsers only grant the lock from a user gesture; deferred, the request is made on the next one
        emscripten_request_pointerlock(g_canvasSelector, /* deferUntilInEventHandler */ true);
    }
    else
    {
        emscripten_exit_pointerlock();
    }
}

bool WebApplicationWindow::OnPointerLockChanged(bool isLocked)
{
    m_hasPointerLock = isLocked;

    if (isLocked || !m_isMouseLocked)
    {
        return false;
    }

    m_isMouseLocked = false;

    return true;
}

bool WebApplicationWindow::HasMouseFocus() const
{
    return true;
}

float WebApplicationWindow::GetContentScaleFactor() const
{
    return 1.0f;
}

float WebApplicationWindow::GetRenderTargetScale() const
{
    return 1.0f;
}

void WebApplicationWindow::Close()
{
}

void WebApplicationWindow::ShowVirtualKeyboard()
{
}

void WebApplicationWindow::HideVirtualKeyboard()
{
}

#pragma endregion WebApplicationWindow

#pragma region WebAppContext

WebAppContext::WebAppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments)
{
}

WebAppContext::~WebAppContext() = default;

Handle<ApplicationWindow> WebAppContext::CreateSystemWindow(WindowOptions windowOptions)
{
    Handle<WebApplicationWindow> window = MakeHandle<WebApplicationWindow>(windowOptions.title, windowOptions.dimensions);
    m_windows.PushBack(window);

    if (m_windows.Size() == 1)
    {
        RegisterBrowserEvents(this);
    }

    return window;
}

void WebAppContext::EnqueueEvent(Event&& event)
{
    Mutex::Guard guard(m_pendingEventsMutex);

    m_pendingEvents.PushBack(std::move(event));
}

int WebAppContext::PollEvents(Event& event)
{
    Mutex::Guard guard(m_pendingEventsMutex);

    if (m_pendingEvents.Empty())
    {
        return 0;
    }

    event = std::move(m_pendingEvents.Front());
    m_pendingEvents.PopFront();

    return 1;
}

#pragma endregion WebAppContext

} // namespace Hyperion
