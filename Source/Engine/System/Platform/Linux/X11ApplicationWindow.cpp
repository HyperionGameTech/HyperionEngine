/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <SystemPch.hpp>

#include <System/AppContext.hpp>

#include <Input/Event.hpp>
#include <Input/InputManager.hpp>
#include <Input/Keyboard.hpp>
#include <Input/Mouse.hpp>

#include <Core/Logging/LogChannels.hpp>
#include <Core/Logging/Logger.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Rendering/Swapchain.hpp>
#include <Rendering/RenderInterface.hpp>

#if HYP_VULKAN
#include <vulkan/vulkan.h>

#include <Rendering/Vulkan/VulkanInstance.hpp>
#endif

#include <Rendering/Util/DeletionQueue.hpp>

#include <Framework/Threads/MainThread.hpp>

// X11 defines macros like None, Bool and Status, so it has to come after all engine headers
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <System/Platform/Linux/X11Helpers.hpp>

namespace Hyperion {

// Same units as Win32's WHEEL_DELTA so scroll speeds match across platforms
static constexpr int ScrollDeltaPerNotch = 120;

static KeyCode MapX11KeySymToKeyCode(::KeySym keySym)
{
    switch (keySym)
    {
    case XK_Tab:
    case XK_ISO_Left_Tab:
        return KeyCode::KEY_TAB;
    case XK_Shift_L:
        return KeyCode::KEY_LSHIFT;
    case XK_Shift_R:
        return KeyCode::KEY_RSHIFT;
    case XK_Control_L:
        return KeyCode::KEY_LCTRL;
    case XK_Control_R:
        return KeyCode::KEY_RCTRL;
    case XK_Alt_L:
    case XK_Meta_L:
        return KeyCode::KEY_LALT;
    case XK_Alt_R:
    case XK_Meta_R:
    case XK_ISO_Level3_Shift: // AltGr
        return KeyCode::KEY_RALT;
    case XK_Caps_Lock:
        return KeyCode::KEY_CAPSLOCK;
    case XK_Return:
    case XK_KP_Enter:
        return KeyCode::KEY_RETURN;
    case XK_BackSpace:
        return KeyCode::KEY_BACKSPACE;
    case XK_Escape:
        return KeyCode::KEY_ESCAPE;
    case XK_space:
        return KeyCode::KEY_SPACE;
    case XK_Left:
        return KeyCode::KEY_LEFT;
    case XK_Up:
        return KeyCode::KEY_UP;
    case XK_Right:
        return KeyCode::KEY_RIGHT;
    case XK_Down:
        return KeyCode::KEY_DOWN;
    case XK_period:
        return KeyCode::KEY_PERIOD;
    case XK_comma:
        return KeyCode::KEY_COMMA;
    case XK_minus:
        return KeyCode::KEY_DASH;
    case XK_equal:
        return KeyCode::KEY_EQUALS;
    case XK_semicolon:
        return KeyCode::KEY_SEMICOLON;
    case XK_slash:
        return KeyCode::KEY_SLASH;
    case XK_apostrophe:
        return KeyCode::KEY_APOSTROPHE;
    case XK_grave:
        return KeyCode::KEY_TILDE;
    default:
        break;
    }

    if (keySym >= XK_a && keySym <= XK_z)
    {
        return KeyCode(uint16(KeyCode::KEY_A) + uint16(keySym - XK_a));
    }

    if (keySym >= XK_A && keySym <= XK_Z)
    {
        return KeyCode(uint16(KeyCode::KEY_A) + uint16(keySym - XK_A));
    }

    if (keySym >= XK_0 && keySym <= XK_9)
    {
        return KeyCode(uint16(KeyCode::KEY_0) + uint16(keySym - XK_0));
    }

    if (keySym >= XK_F1 && keySym <= XK_F12)
    {
        return KeyCode(uint16(KeyCode::KEY_F1) + uint16(keySym - XK_F1));
    }

    return KeyCode::KEY_UNKNOWN;
}

X11ApplicationWindow::X11ApplicationWindow(ANSIString title, Vec2i size)
    : ApplicationWindow(std::move(title), size)
{
}

X11ApplicationWindow::~X11ApplicationWindow()
{
    ::Display* display = static_cast<::Display*>(m_display);

    if (display == nullptr)
    {
        return;
    }

    if (m_pointerGrabbed)
    {
        XUngrabPointer(display, CurrentTime);
        m_pointerGrabbed = false;
    }

    if (m_x11Window != 0)
    {
        XDestroyWindow(display, ::Window(m_x11Window));
        m_x11Window = 0;
        m_hwnd = nullptr;
    }

    if (m_hiddenCursor != 0)
    {
        XFreeCursor(display, ::Cursor(m_hiddenCursor));
        m_hiddenCursor = 0;
    }

    if (m_colormap != 0)
    {
        XFreeColormap(display, ::Colormap(m_colormap));
        m_colormap = 0;
    }

    XFlush(display);

    m_display = nullptr;

    X11_ReleaseDisplay();
}

void X11ApplicationWindow::Initialize(WindowOptions windowOptions)
{
    AssertOnThread(g_mainThread);

    TUniqueLock lock(m_mtx);

    m_title = windowOptions.title;
    m_size = windowOptions.dimensions;

    lock.Reset();

    ::Display* display = X11_AcquireDisplay();

    if (display == nullptr)
    {
        HYP_FAIL("Failed to create X11 window: could not open X11 display. Make sure an X server is running and DISPLAY is set.");
    }

    m_display = display;

    const int screen = DefaultScreen(display);
    const ::Window rootWindow = RootWindow(display, screen);
    ::Visual* visual = DefaultVisual(display, screen);

    m_x11ParentWindow = uint64(reinterpret_cast<uintptr_t>(windowOptions.parentHwnd));

    const ::Window parentWindow = m_x11ParentWindow != 0
        ? ::Window(m_x11ParentWindow)
        : rootWindow;

    // Use our own colormap + border pixel so the window can be created with the default visual
    // even when embedded in a parent that uses a different one (BadMatch otherwise)
    m_colormap = uint64(XCreateColormap(display, rootWindow, visual, AllocNone));

    XSetWindowAttributes attributes {};
    attributes.colormap = ::Colormap(m_colormap);
    attributes.background_pixel = 0;
    attributes.border_pixel = 0;
    attributes.event_mask = KeyPressMask
        | KeyReleaseMask
        | ButtonPressMask
        | ButtonReleaseMask
        | PointerMotionMask
        | FocusChangeMask
        | StructureNotifyMask;

    const unsigned int width = unsigned(MathUtil::Max(windowOptions.dimensions.x, 1));
    const unsigned int height = unsigned(MathUtil::Max(windowOptions.dimensions.y, 1));

    const ::Window window = XCreateWindow(
        display, parentWindow,
        0, 0, width, height, 0,
        DefaultDepth(display, screen), InputOutput, visual,
        CWBackPixel | CWBorderPixel | CWColormap | CWEventMask, &attributes);

    if (window == 0)
    {
        HYP_FAIL("Failed to create X11 window!");
    }

    m_x11Window = uint64(window);
    m_hwnd = reinterpret_cast<void*>(uintptr_t(window));

    if (m_x11ParentWindow == 0)
    {
        const X11Atoms& atoms = X11_GetAtoms();

        XStoreName(display, window, m_title.Data());

        XChangeProperty(
            display, window,
            atoms.netWmName, atoms.utf8String, 8, PropModeReplace,
            reinterpret_cast<const unsigned char*>(m_title.Data()), int(m_title.Size()));

        if (XClassHint* classHint = XAllocClassHint())
        {
            classHint->res_name = const_cast<char*>("hyperion");
            classHint->res_class = const_cast<char*>("Hyperion");

            XSetClassHint(display, window, classHint);
            XFree(classHint);
        }

        // Ask the window manager for a ClientMessage instead of killing the connection when the close button is pressed
        ::Atom wmDeleteWindow = atoms.wmDeleteWindow;
        XSetWMProtocols(display, window, &wmDeleteWindow, 1);
    }

    if (!(windowOptions.flags & uint32(WindowFlags::HEADLESS)))
    {
        XMapWindow(display, window);
    }

    XFlush(display);

    m_isOpen = true;
}

bool X11ApplicationWindow::HandleX11Event(void* xEventPtr, Event& outEvent)
{
    AssertOnThread(g_mainThread);

    XEvent& xEvent = *static_cast<XEvent*>(xEventPtr);
    ::Display* display = static_cast<::Display*>(m_display);

    PlatformEvent platformEvent {};
    platformEvent.x11Event.window = uint64(xEvent.xany.window);
    platformEvent.x11Event.type = int32(xEvent.type);

    switch (xEvent.type)
    {
    case KeyPress:
    case KeyRelease:
    {
        // Index 0 gives the unshifted keysym, so e.g Shift+A still maps to KEY_A
        const ::KeySym keySym = XLookupKeysym(&xEvent.xkey, 0);

        outEvent = Event(xEvent.type == KeyPress ? EventType::KEYDOWN : EventType::KEYUP, this, platformEvent);
        outEvent.GetEventData().Set(MapX11KeySymToKeyCode(keySym));

        return true;
    }
    case ButtonPress:
    {
        // Embedded windows don't get keyboard focus from the window manager
        if (m_x11ParentWindow != 0 && m_isOpen)
        {
            XSetInputFocus(display, ::Window(m_x11Window), RevertToParent, xEvent.xbutton.time);
        }

        if (m_mouseLocked && !m_pointerGrabbed)
        {
            TryGrabPointer();
        }

        switch (xEvent.xbutton.button)
        {
        case Button1:
            outEvent = Event(EventType::MOUSEBUTTON_DOWN, this, platformEvent);
            outEvent.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));
            return true;
        case Button2:
            outEvent = Event(EventType::MOUSEBUTTON_DOWN, this, platformEvent);
            outEvent.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));
            return true;
        case Button3:
            outEvent = Event(EventType::MOUSEBUTTON_DOWN, this, platformEvent);
            outEvent.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));
            return true;
        // X11 reports each wheel notch as a press/release of buttons 4-7
        case Button4:
            outEvent = Event(EventType::MOUSESCROLL, this, platformEvent);
            outEvent.GetEventData().Set(Vec2i(0, ScrollDeltaPerNotch));
            return true;
        case Button5:
            outEvent = Event(EventType::MOUSESCROLL, this, platformEvent);
            outEvent.GetEventData().Set(Vec2i(0, -ScrollDeltaPerNotch));
            return true;
        case 6:
            outEvent = Event(EventType::MOUSESCROLL, this, platformEvent);
            outEvent.GetEventData().Set(Vec2i(-ScrollDeltaPerNotch, 0));
            return true;
        case 7:
            outEvent = Event(EventType::MOUSESCROLL, this, platformEvent);
            outEvent.GetEventData().Set(Vec2i(ScrollDeltaPerNotch, 0));
            return true;
        default:
            return false;
        }
    }
    case ButtonRelease:
    {
        switch (xEvent.xbutton.button)
        {
        case Button1:
            outEvent = Event(EventType::MOUSEBUTTON_UP, this, platformEvent);
            outEvent.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));
            return true;
        case Button2:
            outEvent = Event(EventType::MOUSEBUTTON_UP, this, platformEvent);
            outEvent.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));
            return true;
        case Button3:
            outEvent = Event(EventType::MOUSEBUTTON_UP, this, platformEvent);
            outEvent.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));
            return true;
        default:
            return false;
        }
    }
    case MotionNotify:
    {
        const Vec2i position(xEvent.xmotion.x, xEvent.xmotion.y);

        if (!m_mouseLocked)
        {
            m_lastMousePosition = position;

            outEvent = Event(EventType::MOUSEMOTION, this, platformEvent);
            outEvent.GetEventData().Set(MotionData { Vec2f(position), Vec2f::Zero(), /* isAbsolute */ true });

            return true;
        }

        // While locked the pointer is kept near the center with XWarpPointer and deltas are reported instead.
        // Events generated before the server processed our last warp are still relative to the pre-warp position.
        const bool isBeforeLastWarp = uint64(xEvent.xmotion.serial) < m_warpSerial;

        Vec2i delta;

        if (isBeforeLastWarp)
        {
            delta = position - m_preWarpMousePosition;
            m_preWarpMousePosition = position;
        }
        else
        {
            delta = position - m_lastMousePosition;
            m_lastMousePosition = position;

            const Vec2i size = GetSize();
            const Vec2i center(size.x / 2, size.y / 2);

            if (MathUtil::Abs(position.x - center.x) > size.x / 4 || MathUtil::Abs(position.y - center.y) > size.y / 4)
            {
                WarpPointer(center);
            }
        }

        if (delta == Vec2i::Zero())
        {
            return false;
        }

        outEvent = Event(EventType::MOUSEMOTION, this, platformEvent);
        outEvent.GetEventData().Set(MotionData { Vec2f::Zero(), Vec2f(delta), /* isAbsolute */ false });

        return true;
    }
    case FocusIn:
    case FocusOut:
    {
        // Grab/ungrab notifications come from global hotkeys and keyboard grabs, not real focus changes
        if (xEvent.xfocus.mode == NotifyGrab || xEvent.xfocus.mode == NotifyUngrab)
        {
            return false;
        }

        if (xEvent.xfocus.detail == NotifyInferior || xEvent.xfocus.detail == NotifyPointer)
        {
            return false;
        }

        const bool hasFocus = xEvent.type == FocusIn;

        if (m_hasKeyboardFocus == hasFocus)
        {
            return false;
        }

        m_hasKeyboardFocus = hasFocus;

        if (hasFocus && m_mouseLocked && !m_pointerGrabbed)
        {
            TryGrabPointer();
        }

        outEvent = Event(hasFocus ? EventType::WINDOW_FOCUS_GAINED : EventType::WINDOW_FOCUS_LOST, this, platformEvent);

        return true;
    }
    case ConfigureNotify:
    {
        const Vec2i newSize(xEvent.xconfigure.width, xEvent.xconfigure.height);

        if (newSize.x > 0 && newSize.y > 0)
        {
            HandleResize(newSize);
        }

        return false;
    }
    case MapNotify:
    {
        // XGrabPointer fails until the window is viewable
        if (m_mouseLocked && !m_pointerGrabbed)
        {
            TryGrabPointer();
        }

        return false;
    }
    case ClientMessage:
    {
        const X11Atoms& atoms = X11_GetAtoms();

        if (xEvent.xclient.message_type == atoms.wmProtocols
            && ::Atom(xEvent.xclient.data.l[0]) == atoms.wmDeleteWindow)
        {
            outEvent = Event(EventType::WINDOW_CLOSE, this, platformEvent);

            Close();

            return true;
        }

        return false;
    }
    case DestroyNotify:
    {
        if (xEvent.xdestroywindow.window != ::Window(m_x11Window))
        {
            return false;
        }

        // Destroyed from outside (e.g the embedding parent went away)
        m_x11Window = 0;
        m_hwnd = nullptr;
        m_pointerGrabbed = false;

        outEvent = Event(EventType::WINDOW_CLOSE, this, platformEvent);

        Close();

        return true;
    }
    default:
        break;
    }

    return false;
}

void X11ApplicationWindow::TryGrabPointer()
{
    ::Display* display = static_cast<::Display*>(m_display);

    if (display == nullptr || m_x11Window == 0 || m_pointerGrabbed)
    {
        return;
    }

    const int result = XGrabPointer(
        display, ::Window(m_x11Window),
        True,
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
        GrabModeAsync, GrabModeAsync,
        ::Window(m_x11Window), // confine to the window
        ::Cursor(m_hiddenCursor),
        CurrentTime);

    m_pointerGrabbed = (result == GrabSuccess);

    if (!m_pointerGrabbed)
    {
        HYP_LOG(Core, Debug, "XGrabPointer failed with status {} - will retry", result);
    }
}

void X11ApplicationWindow::WarpPointer(Vec2i position)
{
    ::Display* display = static_cast<::Display*>(m_display);

    if (display == nullptr || m_x11Window == 0)
    {
        return;
    }

    m_preWarpMousePosition = m_lastMousePosition;

    // Holding the display lock keeps other threads (e.g the Vulkan WSI on the render thread) from
    // sending requests between reading the serial and sending the warp
    XLockDisplay(display);

    m_warpSerial = uint64(XNextRequest(display));
    XWarpPointer(display, None, ::Window(m_x11Window), 0, 0, 0, 0, position.x, position.y);

    XUnlockDisplay(display);

    XFlush(display);

    m_lastMousePosition = position;
}

void X11ApplicationWindow::SetMousePosition(Vec2i position)
{
    if (m_mouseLocked)
    {
        // restored when unlocking
        m_lockedMousePosition = position;

        return;
    }

    WarpPointer(position);
}

Vec2i X11ApplicationWindow::GetMousePosition() const
{
    if (m_mouseLocked)
    {
        return m_lockedMousePosition;
    }

    ::Display* display = static_cast<::Display*>(m_display);

    if (display == nullptr || m_x11Window == 0)
    {
        return m_lastMousePosition;
    }

    ::Window rootReturn = 0;
    ::Window childReturn = 0;
    int rootX = 0;
    int rootY = 0;
    int windowX = 0;
    int windowY = 0;
    unsigned int buttonMask = 0;

    // returns False when the pointer is on another screen
    if (!XQueryPointer(display, ::Window(m_x11Window), &rootReturn, &childReturn, &rootX, &rootY, &windowX, &windowY, &buttonMask))
    {
        return m_lastMousePosition;
    }

    return Vec2i(windowX, windowY);
}

Vec2i X11ApplicationWindow::GetDimensions() const
{
    // Kept up to date from ConfigureNotify, avoids a server round trip per call
    return GetSize();
}

void X11ApplicationWindow::SetIsMouseLocked(bool locked)
{
    if (m_mouseLocked == locked)
    {
        return;
    }

    ::Display* display = static_cast<::Display*>(m_display);

    if (display == nullptr || m_x11Window == 0)
    {
        m_mouseLocked = locked;

        return;
    }

    const ::Window window = ::Window(m_x11Window);

    if (locked)
    {
        m_lockedMousePosition = GetMousePosition();
        m_mouseLocked = true;

        if (m_hiddenCursor == 0)
        {
            static const char s_emptyBitmapData[1] = { 0 };

            const ::Pixmap bitmap = XCreateBitmapFromData(display, window, s_emptyBitmapData, 1, 1);

            XColor black {};
            m_hiddenCursor = uint64(XCreatePixmapCursor(display, bitmap, bitmap, &black, &black, 0, 0));

            XFreePixmap(display, bitmap);
        }

        XDefineCursor(display, window, ::Cursor(m_hiddenCursor));

        TryGrabPointer();

        const Vec2i size = GetSize();
        WarpPointer(Vec2i(size.x / 2, size.y / 2));
    }
    else
    {
        m_mouseLocked = false;

        if (m_pointerGrabbed)
        {
            XUngrabPointer(display, CurrentTime);
            m_pointerGrabbed = false;
        }

        XUndefineCursor(display, window);

        // put the cursor back where it was when the lock started
        WarpPointer(m_lockedMousePosition);
    }

    XFlush(display);
}

bool X11ApplicationWindow::HasMouseFocus() const
{
    return m_hasKeyboardFocus;
}

void X11ApplicationWindow::Close()
{
    AssertOnThread(g_mainThread);

    TUniqueLock lock(m_mtx);

    if (!m_isOpen)
    {
        return;
    }

    m_isOpen = false;

#if HYP_VULKAN
    if (m_swapchain.IsValid())
    {
        m_swapchain->TakeOwnershipOfSurface();
        m_vkSurface = VK_NULL_HANDLE;
    }

    if (m_vkSurface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(
            RI.GetInstance()->GetInstance(),
            m_vkSurface,
            nullptr);
        m_vkSurface = VK_NULL_HANDLE;
    }
#endif

    EnqueueDeletion(std::move(m_swapchain));

    lock.Reset();

    SetIsMouseLocked(false);

    // The X window itself is destroyed along with this object, after the swapchain has been queued for deletion
    if (::Display* display = static_cast<::Display*>(m_display); display != nullptr && m_x11Window != 0)
    {
        XUnmapWindow(display, ::Window(m_x11Window));
        XFlush(display);
    }

    g_appContext->RemoveWindow(this);

    OnClose.Fire(this);
}

} // namespace Hyperion
