/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <SystemPch.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <System/AppContext.hpp>
#include <System/Platform/Windows/Win32Helpers.hpp>

#include <Input/Event.hpp>
#include <Input/InputManager.hpp>
#include <Input/Mouse.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Rendering/Swapchain.hpp>
#include <Rendering/RenderInterface.hpp>

#if HYP_VULKAN
#include <Vulkan/vulkan.h>
#include <Vulkan/vulkan_win32.h>

#include <Rendering/Vulkan/VulkanInstance.hpp>
#endif

#include <Rendering/Util/DeletionQueue.hpp>

#include <Framework/threads/MainThread.hpp>
#include <Framework/threads/RenderThread.hpp>

namespace Hyperion {

namespace {

struct Win32WindowRegistry
{
    TSet<WideString> registeredClasses;
    Mutex mutex;

    static Win32WindowRegistry& GetInstance()
    {
        static Win32WindowRegistry s_instance;
        return s_instance;
    }

    void Register(const WideString& className)
    {
        Mutex::Guard guard(mutex);
        registeredClasses.Add(className);
    }

    void Unregister(const WideString& className)
    {
        Mutex::Guard guard(mutex);
        registeredClasses.Erase(className);
    }

    void Cleanup()
    {
        Mutex::Guard guard(mutex);

        HINSTANCE hInst = GetModuleHandleW(nullptr);

        for (const WideString& className : registeredClasses)
        {
            UnregisterClassW(className.Data(), hInst);
        }

        registeredClasses.Clear();
    }
};

} // namespace

void Win32_RegisterWindowClass(const WideString& className)
{
    Win32WindowRegistry::GetInstance().Register(className);
}

void Win32_UnregisterWindowClass(const WideString& className)
{
    Win32WindowRegistry::GetInstance().Unregister(className);
}

void Win32_CleanupWindowClasses()
{
    Win32WindowRegistry::GetInstance().Cleanup();
}

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC ((USHORT)0x01)
#endif
#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE ((USHORT)0x02)
#endif
#ifndef HID_USAGE_GENERIC_KEYBOARD
#define HID_USAGE_GENERIC_KEYBOARD ((USHORT)0x06)
#endif

static KeyCode MapWin32VirtualKeyToKeyCode(LPARAM lParam, WPARAM wParam)
{
    switch (wParam)
    {
    case VK_TAB:
        return KeyCode::KEY_TAB;
    case VK_SHIFT:
    {
        const bool isRight = (lParam & (1 << 24)) != 0;
        return isRight ? KeyCode::KEY_RSHIFT : KeyCode::KEY_LSHIFT;
    }
    case VK_CONTROL:
    {
        const bool isRight = (lParam & (1 << 24)) != 0;
        return isRight ? KeyCode::KEY_RCTRL : KeyCode::KEY_LCTRL;
    }
    case VK_MENU:
    {
        const bool isRight = (lParam & (1 << 24)) != 0;
        return isRight ? KeyCode::KEY_RALT : KeyCode::KEY_LALT;
    }
    case VK_CAPITAL:
        return KeyCode::KEY_CAPSLOCK;
    case VK_SPACE:
        return KeyCode::KEY_SPACE;
    case VK_LEFT:
        return KeyCode::KEY_LEFT;
    case VK_UP:
        return KeyCode::KEY_UP;
    case VK_RIGHT:
        return KeyCode::KEY_RIGHT;
    case VK_DOWN:
        return KeyCode::KEY_DOWN;
    case VK_LMENU:
        return KeyCode::KEY_LALT;
    case VK_RMENU:
        return KeyCode::KEY_RALT;
    case VK_LCONTROL:
        return KeyCode::KEY_LCTRL;
    case VK_RCONTROL:
        return KeyCode::KEY_RCTRL;
    case VK_LSHIFT:
        return KeyCode::KEY_LSHIFT;
    case VK_RSHIFT:
        return KeyCode::KEY_RSHIFT;
    default:
        break;
    }

    if (wParam >= 'A' && wParam <= 'Z')
    {
        return KeyCode(uint16(KeyCode::KEY_A) + (wParam - 'A'));
    }
    else if (wParam >= 'a' && wParam <= 'z')
    {
        return KeyCode(uint16(KeyCode::KEY_A) + (wParam - 'a'));
    }
    else if (wParam >= '0' && wParam <= '9')
    {
        return KeyCode(wParam);
    }
    else if (wParam >= VK_F1 && wParam <= VK_F12)
    {
        return KeyCode(uint32(KeyCode::KEY_F1) + (wParam - VK_F1));
    }

    if (wParam < 256)
    {
        return KeyCode(wParam);
    }

    return KeyCode::KEY_UNKNOWN;
}

bool HandleWindowEvent(
    Win32ApplicationWindow* window, Event& event,
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PlatformEvent platformEvent {};
    platformEvent.win32Event = Win32Event();
    platformEvent.win32Event.hwnd = hWnd;
    platformEvent.win32Event.message = msg;
    platformEvent.win32Event.wParam = wParam;
    platformEvent.win32Event.lParam = lParam;

    switch (msg)
    {
    case WM_INPUT:
        window->ProcessRawInput((void*)lParam);
        return true;
    case WM_KEYDOWN:
        event = Event(EventType::KEYDOWN, window, platformEvent);
        event.GetEventData().Set(MapWin32VirtualKeyToKeyCode(lParam, wParam));

        return true;
    case WM_KEYUP:
        event = Event(EventType::KEYUP, window, platformEvent);
        event.GetEventData().Set(MapWin32VirtualKeyToKeyCode(lParam, wParam));

        return true;
    case WM_MOUSEMOVE:
    {
        event = Event(EventType::MOUSEMOTION, window, platformEvent);

        POINT pt;
        pt.x = LOWORD(lParam);
        pt.y = HIWORD(lParam);

        event.GetEventData().Set(MotionData { Vec2f(float(pt.x), float(pt.y)), Vec2f::Zero(), /* isAbsolute */ true });

        return true;
    }
    case WM_LBUTTONDOWN:
        event = Event(EventType::MOUSEBUTTON_DOWN, window, platformEvent);
        event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));

        SetFocus(window->GetHWND());

        return true;
    case WM_LBUTTONUP:
        event = Event(EventType::MOUSEBUTTON_UP, window, platformEvent);
        event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));

        return true;
    case WM_MBUTTONDOWN:
        event = Event(EventType::MOUSEBUTTON_DOWN, window, platformEvent);
        event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));

        return true;
    case WM_MBUTTONUP:
        event = Event(EventType::MOUSEBUTTON_UP, window, platformEvent);
        event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));

        return true;
    case WM_RBUTTONDOWN:
        event = Event(EventType::MOUSEBUTTON_DOWN, window, platformEvent);
        event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));

        return true;
    case WM_RBUTTONUP:
        event = Event(EventType::MOUSEBUTTON_UP, window, platformEvent);
        event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));

        return true;
    case WM_MOUSEWHEEL:
    {
        event = Event(EventType::MOUSESCROLL, window, platformEvent);

        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        event.GetEventData().Set(Vec2i(0, delta));

        return true;
    }
    case WM_MOUSEHWHEEL:
    {
        event = Event(EventType::MOUSESCROLL, window, platformEvent);

        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        event.GetEventData().Set(Vec2i(delta, 0));

        return true;
    }
    case WM_ACTIVATE:
    {
        bool isActive = (LOWORD(wParam) != WA_INACTIVE);

        event = Event(isActive ? EventType::WINDOW_FOCUS_GAINED : EventType::WINDOW_FOCUS_LOST, window, platformEvent);

        return true;
    }
    case WM_CLOSE:
    case WM_DESTROY:
    {
        event = Event(EventType::WINDOW_CLOSE, window, platformEvent);

        return true;
    }

    default:
        break;
    }

    return false;
}

static LRESULT CALLBACK EngineWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ObjId<Win32ApplicationWindow> windowId;
    windowId.value = static_cast<decltype(ObjId<Win32ApplicationWindow>::value)>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    Handle<Win32ApplicationWindow> windowHandle { windowId };

    Event event;
    if (HandleWindowEvent(windowHandle.Get(), event, hWnd, msg, wParam, lParam))
    {
        const EventType eventType = event.GetType();

        if (eventType != EventType::INVALID)
        {
            windowHandle->GetInputManager()->ProcessEvent(std::move(event));

            return 0;
        }

        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

Win32ApplicationWindow::Win32ApplicationWindow(ANSIString title, Vec2i size)
    : ApplicationWindow(std::move(title), size)
{
    m_hinst = GetModuleHandleW(nullptr);
}

Win32ApplicationWindow::~Win32ApplicationWindow()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    WideString wTitle = m_title.ToWide();

    UnregisterClassW(wTitle.Data(), m_hinst);
    Win32WindowRegistry::GetInstance().Unregister(wTitle.Data());
}

void Win32ApplicationWindow::ProcessRawInput(void* rawInput)
{
    HRAWINPUT hRawInput = (HRAWINPUT)rawInput;
    UINT size = 0;

    GetRawInputData(hRawInput, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));

    if (size == 0)
    {
        return;
    }

    void* lpb = alloca(size);
    if (GetRawInputData(hRawInput, RID_INPUT, lpb, &size, sizeof(RAWINPUTHEADER)) != size)
    {
        return;
    }

    RAWINPUT* raw = (RAWINPUT*)lpb;

    Event event;

    PlatformEvent platformEvent {};
    platformEvent.win32Event.hwnd = m_hwnd;
    platformEvent.win32Event.message = WM_INPUT;

    if (raw->header.dwType == RIM_TYPEMOUSE)
    {
        event = Event(EventType::MOUSEMOTION, this, platformEvent);

        int x = raw->data.mouse.lLastX;
        int y = raw->data.mouse.lLastY;

        if (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
        {
            event.GetEventData().Set(MotionData { Vec2f(x, y), Vec2f::Zero(), /* isAbsolute */ true });

            m_inputManager->ProcessEvent(std::move(event));
        }
        else
        {
            event.GetEventData().Set(MotionData { Vec2f::Zero(), Vec2f(x, y), /* isAbsolute */ false });

            m_inputManager->ProcessEvent(std::move(event));
        }

        if (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
        {
            event = Event(EventType::MOUSEBUTTON_DOWN, this, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));
            m_inputManager->ProcessEvent(std::move(event));
        }

        if (raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
        {
            event = Event(EventType::MOUSEBUTTON_UP, this, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));
            m_inputManager->ProcessEvent(std::move(event));
        }

        if (raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
        {
            event = Event(EventType::MOUSEBUTTON_DOWN, this, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));
            m_inputManager->ProcessEvent(std::move(event));
        }

        if (raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
        {
            event = Event(EventType::MOUSEBUTTON_UP, this, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));
            m_inputManager->ProcessEvent(std::move(event));
        }

        if (raw->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
        {
            event = Event(EventType::MOUSEBUTTON_DOWN, this, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));
            m_inputManager->ProcessEvent(std::move(event));
        }

        if (raw->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
        {
            event = Event(EventType::MOUSEBUTTON_UP, this, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));
            m_inputManager->ProcessEvent(std::move(event));
        }

        if (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
        {
            event = Event(EventType::MOUSESCROLL, this, platformEvent);
            event.GetEventData().Set(Vec2i(0, (short)raw->data.mouse.usButtonData));
            m_inputManager->ProcessEvent(std::move(event));
        }
    }
    else if (raw->header.dwType == RIM_TYPEKEYBOARD)
    {
        USHORT virtualKey = raw->data.keyboard.VKey;
        UINT makeCode = raw->data.keyboard.MakeCode;
        UINT flags = raw->data.keyboard.Flags;

        if (virtualKey == 255)
        {
            return;
        }

        if (virtualKey == VK_SHIFT || virtualKey == VK_CONTROL || virtualKey == VK_MENU)
        {
            virtualKey = LOWORD(MapVirtualKeyW(makeCode, MAPVK_VSC_TO_VK_EX));
        }

        bool isDown = !(flags & RI_KEY_BREAK);

        LPARAM fakeLParam = 0;
        if (flags & RI_KEY_E0)
        {
            fakeLParam |= (1 << 24);
        }

        KeyCode keyCode = MapWin32VirtualKeyToKeyCode(fakeLParam, virtualKey);

        event = Event(isDown ? EventType::KEYDOWN : EventType::KEYUP, this, platformEvent);
        event.GetEventData().Set(keyCode);

        m_inputManager->ProcessEvent(std::move(event));
    }
}

void Win32ApplicationWindow::Initialize(WindowOptions windowOptions)
{
    AssertOnThread(g_mainThread);

    TUniqueLock lock(m_mtx);

    m_title = windowOptions.title;
    m_size = windowOptions.dimensions;
    WideString wTitle = m_title.ToWide();

    lock.Reset();

    m_useWndProc = !(windowOptions.flags & uint32(WindowFlags::EVENTS_POLLING));

    WNDCLASSEXW wc {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = &Win32ApplicationWindow::StaticWndProc;
    wc.hInstance = m_hinst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(10, 10, 10));
    wc.lpszClassName = wTitle.Data();

    ATOM classAtom = RegisterClassExW(&wc);
    Assert(classAtom != 0, "Failed to register Win32 window class! Win32 Error: {}", GetLastError());

    Win32WindowRegistry::GetInstance().Register(wTitle);

    int x = 0, y = 0;

    DWORD style = WS_VISIBLE;

    x = CW_USEDEFAULT;
    y = CW_USEDEFAULT;

    if (!(windowOptions.flags & uint32(WindowFlags::HEADLESS)))
    {
        style |= WS_OVERLAPPEDWINDOW;
    }

    if (windowOptions.parentHwnd != nullptr)
    {
        style |= WS_CHILD;
        style &= ~WS_OVERLAPPEDWINDOW;
    }

    RECT r { 0, 0, (LONG)windowOptions.dimensions.x, (LONG)windowOptions.dimensions.y };
    AdjustWindowRect(&r, style, FALSE);

    m_hwnd = CreateWindowW(
        wc.lpszClassName, wTitle.Data(), style,
        x, y,
        r.right - r.left, r.bottom - r.top,
        windowOptions.parentHwnd, nullptr, m_hinst, this);

    if (!m_hwnd)
    {
        HYP_FAIL("Failed to create Win32 window! Error code: {}", GetLastError());
    }

    UpdateWindow(m_hwnd);

    m_isOpen = true;
}

LRESULT CALLBACK Win32ApplicationWindow::StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_NCCREATE:
    {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);

        Win32ApplicationWindow* self = static_cast<Win32ApplicationWindow*>(cs->lpCreateParams);
        ObjId<Win32ApplicationWindow> windowId = self->Id();

        SetWindowLongPtrW(hWnd, GWLP_USERDATA, static_cast<LONG_PTR>(windowId.Value()));

        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    case WM_NCDESTROY:
    {
        ObjId<Win32ApplicationWindow> windowId;
        windowId.value = static_cast<decltype(ObjId<Win32ApplicationWindow>::value)>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

        Handle<Win32ApplicationWindow> windowHandle { windowId };

        LRESULT result = windowHandle->WndProc(hWnd, msg, wParam, lParam);

        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);

        return result;
    }
    default:
    {
        ObjId<Win32ApplicationWindow> windowId;
        windowId.value = static_cast<decltype(ObjId<Win32ApplicationWindow>::value)>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

        if (windowId.IsValid())
        {
            Handle<Win32ApplicationWindow> windowHandle { windowId };
            return windowHandle->WndProc(hWnd, msg, wParam, lParam);
        }

        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    }
}

LRESULT Win32ApplicationWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PlatformEvent platformEvent {};
    platformEvent.win32Event = Win32Event();
    platformEvent.win32Event.hwnd = hWnd;
    platformEvent.win32Event.message = msg;
    platformEvent.win32Event.wParam = wParam;
    platformEvent.win32Event.lParam = lParam;

    switch (msg)
    {
    case WM_SIZE:
    {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);

        HandleResize(Vec2i(width, height));

        break;
    }
    case WM_NCDESTROY:
    {
        m_hwnd = nullptr;

        Close();

        break;
    }
    case WM_ACTIVATE:
    {
        bool isActive = (LOWORD(wParam) != WA_INACTIVE);

        Event event(isActive ? EventType::WINDOW_FOCUS_GAINED : EventType::WINDOW_FOCUS_LOST, this, platformEvent);

        m_inputManager->ProcessEvent(std::move(event));

        return true;
    }
    default:
        break;
    }

    return m_useWndProc
        ? EngineWndProc(hWnd, msg, wParam, lParam)
        : DefWindowProcW(hWnd, msg, wParam, lParam);
}

void Win32ApplicationWindow::SetMousePosition(Vec2i position)
{
    POINT pt { position.x, position.y };
    ClientToScreen(m_hwnd, &pt);
    SetCursorPos(pt.x, pt.y);
}

Vec2i Win32ApplicationWindow::GetMousePosition() const
{
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(m_hwnd, &pt);
    return { pt.x, pt.y };
}

Vec2i Win32ApplicationWindow::GetDimensions() const
{
    RECT rc {};
    GetClientRect(m_hwnd, &rc);
    return { rc.right - rc.left, rc.bottom - rc.top };
}

void Win32ApplicationWindow::SetIsMouseLocked(bool locked)
{
    if (m_mouseLocked == locked)
    {
        return;
    }

    m_mouseLocked = locked;

    if (locked)
    {
        while (::ShowCursor(FALSE) >= 0)
            ;

        SetCapture(m_hwnd);

        RECT rc {};
        GetClientRect(m_hwnd, &rc);

        POINT tl { rc.left, rc.top }, br { rc.right, rc.bottom };
        ClientToScreen(m_hwnd, &tl);
        ClientToScreen(m_hwnd, &br);

        RECT clip { tl.x, tl.y, br.x, br.y };
        ClipCursor(&clip);
    }
    else
    {
        ClipCursor(nullptr);
        ReleaseCapture();
        ShowCursor(TRUE);
    }
}

bool Win32ApplicationWindow::HasMouseFocus() const
{
    return GetFocus() == m_hwnd;
}

void Win32ApplicationWindow::Close()
{
    AssertOnThread(g_mainThread);

    TUniqueLock lock(m_mtx);

    if (!m_isOpen)
    {
        return;
    }

    m_swapchain.Reset();

#if HYP_VULKAN
    if (m_vkSurface)
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([surface = m_vkSurface]()
            {
                VulkanInstance* vulkanInstance = RI.GetInstance();
                Assert(vulkanInstance != nullptr);

                vkDestroySurfaceKHR(vulkanInstance->GetInstance(), surface, nullptr);
            }));

        m_vkSurface = VK_NULL_HANDLE;
    }
#endif

    if (m_hwnd != nullptr)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    auto onClose = std::move(OnClose);

    m_isOpen = false;

    lock.Reset();

    g_appContext->RemoveWindow(this);

    onClose();
}

} // namespace Hyperion
