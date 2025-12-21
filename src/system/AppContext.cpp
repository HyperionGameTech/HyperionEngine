/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <SystemPch.hpp>

#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/debug/Debug.hpp>

#include <core/config/Config.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/Device.hpp>
#include <rendering/RenderInterface.hpp>

#ifdef HYP_VULKAN

#include <vulkan/vulkan.h>

#if defined(HYP_WINDOWS)
#include <vulkan/vulkan_win32.h>
#elif defined(HYP_MACOS)
#include <vulkan/vulkan_metal.h>
#elif defined(HYP_LINUX)
#include <vulkan/vulkan_xlib.h>
#endif

#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanSwapchain.hpp>
#endif

#include <engine/EngineDriver.hpp>

#include <input/InputManager.hpp>

#ifdef HYP_SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#endif

#include <AppContext.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

extern const GlobalConfig& CoreApi_GetGlobalConfig();

namespace sys {

#pragma region ApplicationWindow

ApplicationWindow::ApplicationWindow(ANSIString title, Vec2i size)
    : m_title(std::move(title)),
      m_size(size),
      m_hwnd(nullptr),
      m_inputManager(CreateObject<InputManager>(this))
{
}

ApplicationWindow::~ApplicationWindow()
{
#ifdef HYP_VULKAN
    if (m_vkSurface)
    {
        VulkanInstance* vkInstance = g_renderBackend->GetInstance();
        Assert(vkInstance != nullptr);

        vkDestroySurfaceKHR(vkInstance->GetInstance(), m_vkSurface, nullptr);
        m_vkSurface = VK_NULL_HANDLE;
    }
#endif
}

void ApplicationWindow::HandleResize(Vec2i newSize)
{
    {
        Mutex::Guard guard(m_mtx);

        if (m_size == newSize)
        {
            return;
        }

        m_size = newSize;
    }

    if (Swapchain* swapchain = GetSwapchain())
    {
        if (IsOnThread(g_renderThread))
        {
            swapchain->Resize(Vec2u(newSize));
        }
        else
        {
            // if we have a dedicated rendering thread we need to tell the render thread to resize the swapchain
            GetThreadById(g_renderThread)->GetScheduler().Enqueue([swapchainWeak = MakeWeakRef(swapchain), newSize]()
                {
                    SwapchainRef swapchain = swapchainWeak.Lock();
                    if (!swapchain.IsValid())
                    {
                        HYP_LOG(Core, Warning, "Attempted to resize invalid swapchain on render thread!");
                        return;
                    }

                    swapchain->Resize(Vec2u(newSize));
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }

    OnWindowSizeChanged(newSize);
}

void ApplicationWindow::CreateSwapchain()
{
    if (m_swapchain.IsValid())
    {
        return; // already created
    }

#ifdef HYP_VULKAN
    AssertDebug(GetDimensions() != Vec2i::Zero());

    if (!m_vkSurface)
    {
        VkSurfaceKHR surface = g_renderBackend->CreateSurface(this, nullptr);
        Assert(surface != VK_NULL_HANDLE);

        m_vkSurface = surface;
    }

    VulkanSwapchainRef swapchain = g_renderBackend->CreateSwapchain(this, g_renderBackend->GetInstance(), m_vkSurface);
    Assert(swapchain.IsValid());

    m_swapchain = swapchain;
#else
    HYP_NOT_IMPLEMENTED();
#endif
}

#pragma endregion ApplicationWindow

#pragma region AppContextBase

const Handle<AppContextBase>& AppContextBase::GetInstance()
{
    return g_appContext;
}

AppContextBase::AppContextBase(ANSIString name, const CommandLineArguments& arguments)
    : m_mainWindow(nullptr)
{
    m_name = std::move(name);

    if (m_name.Empty())
    {
        if (json::JSONValue configAppName = CoreApi_GetGlobalConfig().Get("App.Name"))
        {
            m_name = CoreApi_GetGlobalConfig().Get("App.Name").ToString();
        }
    }
}

AppContextBase::~AppContextBase() = default;

void AppContextBase::SetMainWindow(const Handle<ApplicationWindow>& window)
{
    AssertOnThread(g_mainThread);

    if (!m_windows.Contains(window))
    {
        m_windows.PushBack(window);
    }

    m_mainWindow = window;

    if (RenderApi::IsInit())
    {
        if (IsOnThread(g_renderThread))
        {
            m_mainWindow->CreateSwapchain();
        }
        else
        {
            GetThreadById(g_renderThread)->GetScheduler().Enqueue([mainWindowWeak = MakeWeakRef(m_mainWindow)]()
                {
                    Handle<ApplicationWindow> mainWindow = mainWindowWeak.Lock();
                    if (mainWindow.IsValid())
                    {
                        mainWindow->CreateSwapchain();
                    }
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }

    OnCurrentWindowChanged(m_mainWindow);
}

void AppContextBase::RemoveWindow(ApplicationWindow* window)
{
    AssertOnThread(g_mainThread);

    auto it = m_windows.FindIf([window](const Handle<ApplicationWindow>& other)
        {
            return other.Get() == window;
        });

    if (it != m_windows.End())
    {
        if (m_mainWindow == window)
        {
            m_mainWindow = nullptr;

            OnCurrentWindowChanged(nullptr);
        }

        SafeDelete(std::move(*it));
        m_windows.Erase(it);
    }
}

#pragma endregion AppContextBase

#pragma region SDLApplicationWindow

#ifdef HYP_SDL

SDLApplicationWindow::SDLApplicationWindow(ANSIString title, Vec2i size)
    : ApplicationWindow(std::move(title), size),
      m_windowHandle(nullptr)
{
}

SDLApplicationWindow::~SDLApplicationWindow()
{
    SDL_DestroyWindow(static_cast<SDL_Window*>(m_windowHandle));
}

void SDLApplicationWindow::Initialize(WindowOptions windowOptions)
{
    uint32 sdlFlags = 0;

    if (!(windowOptions.flags & uint32(WindowFlags::NO_GFX)))
    {
#if HYP_VULKAN
        sdlFlags |= SDL_WINDOW_VULKAN;
#endif
    }

    if (windowOptions.flags & uint32(WindowFlags::HIGH_DPI))
    {
        sdlFlags |= SDL_WINDOW_ALLOW_HIGHDPI;
    }

    if (windowOptions.flags & uint32(WindowFlags::HEADLESS))
    {
        sdlFlags |= SDL_WINDOW_HIDDEN;
    }
    else
    {
        sdlFlags |= SDL_WINDOW_SHOWN;
        sdlFlags |= SDL_WINDOW_RESIZABLE;

        // make sure to use SDL_free on file name strings for these events
        SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    }

    m_hwnd = SDL_CreateWindow(
        m_title.Data(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        int(m_size.x),
        int(m_size.y),
        sdlFlags);

    Assert(m_hwnd != nullptr, "Failed to initialize window: %s", SDL_GetError());
}

void SDLApplicationWindow::SetMousePosition(Vec2i position)
{
    SDL_WarpMouseInWindow(static_cast<SDL_Window*>(m_hwnd), position.x, position.y);
}

Vec2i SDLApplicationWindow::GetMousePosition() const
{
    Vec2i position;
    SDL_GetMouseState(&position.x, &position.y);

    return position;
}

Vec2i SDLApplicationWindow::GetDimensions() const
{
    int width, height;
    SDL_GetWindowSize(static_cast<SDL_Window*>(m_hwnd), &width, &height);

    return Vec2i { width, height };
}

void SDLApplicationWindow::SetIsMouseLocked(bool locked)
{
    if (locked)
    {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
    else
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
}

bool SDLApplicationWindow::IsMouseLocked() const
{
    return false; /// \todo
}

bool SDLApplicationWindow::HasMouseFocus() const
{
    const SDL_Window* focusWindow = SDL_GetMouseFocus();

    return focusWindow == static_cast<SDL_Window*>(m_hwnd);
}

bool SDLApplicationWindow::IsHighDPI() const
{
    const int displayIndex = SDL_GetWindowDisplayIndex(static_cast<SDL_Window*>(m_hwnd));

    if (displayIndex < 0)
    {
        return false;
    }

    float ddpi, hdpi, vdpi;

    if (SDL_GetDisplayDPI(displayIndex, &ddpi, &hdpi, &vdpi) == 0)
    {
        return hdpi > 96.0f;
    }

    return false;
}

#else

SDLApplicationWindow::SDLApplicationWindow(ANSIString title, Vec2i size)
    : ApplicationWindow(std::move(title), size)
{
}

SDLApplicationWindow::~SDLApplicationWindow() = default;

void SDLApplicationWindow::Initialize(WindowOptions windowOptions)
{
    HYP_NOT_IMPLEMENTED();
}

void SDLApplicationWindow::SetMousePosition(Vec2i position)
{
    HYP_NOT_IMPLEMENTED();
}

Vec2i SDLApplicationWindow::GetMousePosition() const
{
    HYP_NOT_IMPLEMENTED();
}

Vec2i SDLApplicationWindow::GetDimensions() const
{
    HYP_NOT_IMPLEMENTED();
}

void SDLApplicationWindow::SetIsMouseLocked(bool locked)
{
    HYP_NOT_IMPLEMENTED();
}

bool SDLApplicationWindow::IsMouseLocked() const
{
    HYP_NOT_IMPLEMENTED();
}

bool SDLApplicationWindow::HasMouseFocus() const
{
    HYP_NOT_IMPLEMENTED();
}

bool SDLApplicationWindow::IsHighDPI() const
{
    HYP_NOT_IMPLEMENTED();
}

#endif

#pragma endregion SDLApplicationWindow

#pragma region SDLAppContext

#ifdef HYP_SDL
#ifdef HYP_IOS
static struct IOSSDLInitializer
{
    IOSSDLInitializer()
    {
        SDL_SetMainReady();
    }
} s_iosSdlInitializer = {};
#endif

SDLAppContext::SDLAppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments)
{
    const int sdlInitResult = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    if (sdlInitResult < 0)
    {
        HYP_FAIL("Failed to initialize SDL: %s (%d)", SDL_GetError(), sdlInitResult);
    }
}

SDLAppContext::~SDLAppContext()
{
    SDL_Quit();
}

Handle<ApplicationWindow> SDLAppContext::CreateSystemWindow(WindowOptions windowOptions)
{
    Handle<SDLApplicationWindow> window = CreateObject<SDLApplicationWindow>(windowOptions.title, windowOptions.size);
    m_windows.PushBack(window);

    window->Initialize(windowOptions);

    return window;
}

int SDLAppContext::PollEvents(SystemEvent& event)
{
    event = SystemEvent();

    SDL_Event& sdlEvent = event.GetPlatformEvent().sdlEvent;

    const int result = SDL_PollEvent(&sdlEvent);

    if (result)
    {
        switch (sdlEvent.type)
        {
        case SDL_DROPFILE:
        {
            event = SystemEvent(SystemEvent::FILE_DROP, PlatformEvent(sdlEvent));
            // set event data variant to the file path
            event.GetEventData().Set(FilePath(sdlEvent.drop.file));

            // need to free or else the mem will leak
            SDL_free(sdlEvent.drop.file);
            sdlEvent.drop.file = nullptr;

            break;
        }
        case SDL_KEYDOWN: // fallthrough
        case SDL_KEYUP:
        {
            switch (sdlEvent.type)
            {
            case SDL_KEYDOWN:
                event = SystemEvent(SystemEvent::KEYDOWN, PlatformEvent(sdlEvent));
                break;
            case SDL_KEYUP:
                event = SystemEvent(SystemEvent::KEYUP, PlatformEvent(sdlEvent));
                break;
            default:
                HYP_UNREACHABLE();
            }

            event.GetEventData().Set(KeyCode(sdlEvent.key.keysym.sym));

            break;
        }
        case SDL_MOUSEMOTION:
        {
            event = SystemEvent(SystemEvent::MOUSEMOTION, PlatformEvent(sdlEvent));
            event.GetEventData().Set(Vec2i(sdlEvent.motion.x, sdlEvent.motion.y));
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: // fallthrough
        {
            switch (sdlEvent.type)
            {
            case SDL_MOUSEBUTTONDOWN:
                event = SystemEvent(SystemEvent::MOUSEBUTTON_DOWN, PlatformEvent(sdlEvent));
                break;
            case SDL_MOUSEBUTTONUP:
                event = SystemEvent(SystemEvent::MOUSEBUTTON_UP, PlatformEvent(sdlEvent));
                break;
            default:
                HYP_UNREACHABLE();
            }

            EnumFlags<MouseButtonState> mouseButtonState = MouseButtonState::NONE;

            switch (sdlEvent.button.button)
            {
            case SDL_BUTTON_LEFT:
                mouseButtonState |= MouseButtonState::LEFT;
                break;
            case SDL_BUTTON_MIDDLE:
                mouseButtonState |= MouseButtonState::MIDDLE;
                break;
            case SDL_BUTTON_RIGHT:
                mouseButtonState |= MouseButtonState::RIGHT;
                break;
            default:
                break;
            }

            event.GetEventData().Set(mouseButtonState);

            break;
        }
        case SDL_MOUSEWHEEL:
        {
            event = SystemEvent(SystemEvent::MOUSESCROLL, PlatformEvent(sdlEvent));
            event.GetEventData().Set(Vec2i(sdlEvent.wheel.x, sdlEvent.wheel.y));
            break;
        }
        case SDL_WINDOWEVENT:
        {
            switch (sdlEvent.window.event)
            {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
            {
                int width = sdlEvent.window.data1;
                int height = sdlEvent.window.data2;

                event = SystemEvent(SystemEvent::WINDOW_RESIZED, PlatformEvent(sdlEvent));
                event.GetEventData().Set(Vec2i(width, height));

                break;
            }
            default:
                break;
            }

            break;
        }
        default:
            break;
        }
    }

    return result;
}

VkSurfaceKHR SDLAppContext::CreateVulkanSurface(
    SDLApplicationWindow* window,
    IDummyVulkanSurfaceContext** ppOutDummySurfaceContext)
{
    HYP_ASSERT(window != nullptr);

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    SDLApplicationWindow* sdlWindow = ObjCast<SDLApplicationWindow>(window);
    Assert(sdlWindow != nullptr);

    SDL_bool result = SDL_Vulkan_CreateSurface(
        static_cast<SDL_Window*>(sdlWindow->GetHWND()),
        m_instance->GetInstance(),
        &surface);

    Assert(result, "Failed to create Vulkan surface: {}", SDL_GetError());

    return surface;
}

#else

SDLAppContext::SDLAppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments)
{
}

SDLAppContext::~SDLAppContext() = default;

Handle<ApplicationWindow> SDLAppContext::CreateSystemWindow(WindowOptions windowOptions)
{
    HYP_NOT_IMPLEMENTED();
}

int SDLAppContext::PollEvents(SystemEvent& event)
{
    HYP_NOT_IMPLEMENTED();
}

#endif

#pragma endregion SDLAppContext

#pragma region Win32ApplicationWindow

#ifdef HYP_WINDOWS

namespace {

struct Win32WindowRegistry
{
    HashSet<WideString> registeredClasses;
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

static constexpr const wchar_t* WindowClassName = L"HyperionRenderWindow";

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

void Win32ApplicationWindow::Initialize(WindowOptions windowOptions)
{
    m_title = windowOptions.title;
    m_size = windowOptions.dimensions;
    m_useWndProc = (windowOptions.flags & uint32(WindowFlags::EVENTS_POLLING)) == 0;

    WideString wTitle = m_title.ToWide();

    WNDCLASSEXW wc {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = &Win32ApplicationWindow::StaticWndProc;
    wc.hInstance = m_hinst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = wTitle.Data();

    ATOM classAtom = RegisterClassExW(&wc);
    Assert(classAtom != 0, "Failed to register Win32 window class! Win32 Error: {}", GetLastError());

    Win32WindowRegistry::GetInstance().Register(wTitle);

    int x = 0, y = 0;

    DWORD style = WS_VISIBLE;

    if (!(windowOptions.flags & uint32(WindowFlags::HEADLESS)))
    {
        style |= WS_OVERLAPPEDWINDOW;

        x = CW_USEDEFAULT;
        y = CW_USEDEFAULT;
    }

    if (windowOptions.parentHwnd != nullptr)
    {
        style |= WS_CHILD;
        style &= ~WS_OVERLAPPEDWINDOW;
    }

    RECT r { 0, 0, (LONG)m_size.x, (LONG)m_size.y };
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
}

static KeyCode MapWin32VirtualKeyToKeyCode(LPARAM lParam, WPARAM wParam)
{
    // Most VK_* keys are mapped directly to KeyCode, but some need special handling
    switch (wParam)
    {
    case VK_TAB:
        return KeyCode::KEY_TAB;
    case VK_SHIFT:
    {
        // Distinguish between left and right shift
        const bool isRight = (lParam & (1 << 24)) != 0;
        return isRight ? KeyCode::KEY_RSHIFT : KeyCode::KEY_LSHIFT;
    }
    case VK_CONTROL:
    {
        // Distinguish between left and right control
        const bool isRight = (lParam & (1 << 24)) != 0;
        return isRight ? KeyCode::KEY_RCTRL : KeyCode::KEY_LCTRL;
    }
    case VK_MENU:
    {
        // Distinguish between left and right alt (menu)
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

static bool HandleWindowEvent(
    Win32ApplicationWindow* window, SystemEvent& event,
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
    case WM_KEYDOWN:
        event = SystemEvent(SystemEvent::KEYDOWN, window, platformEvent);
        event.GetEventData().Set(MapWin32VirtualKeyToKeyCode(lParam, wParam));

        return true;
    case WM_KEYUP:
        event = SystemEvent(SystemEvent::KEYUP, window, platformEvent);
        event.GetEventData().Set(MapWin32VirtualKeyToKeyCode(lParam, wParam));

        return true;
    case WM_MOUSEMOVE:
    {
        event = SystemEvent(SystemEvent::MOUSEMOTION, window, platformEvent);

        POINT pt;
        pt.x = LOWORD(lParam);
        pt.y = HIWORD(lParam);

        event.GetEventData().Set(Vec2i(pt.x, pt.y));

        return true;
    }
    case WM_LBUTTONDOWN:
        event = SystemEvent(SystemEvent::MOUSEBUTTON_DOWN, window, platformEvent);
        event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));

        return true;
    case WM_LBUTTONUP:
        event = SystemEvent(SystemEvent::MOUSEBUTTON_UP, window, platformEvent);
        event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));

        return true;
    case WM_MBUTTONDOWN:
        event = SystemEvent(SystemEvent::MOUSEBUTTON_DOWN, window, platformEvent);
        event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));

        return true;
    case WM_MBUTTONUP:
        event = SystemEvent(SystemEvent::MOUSEBUTTON_UP, window, platformEvent);
        event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));

        return true;
    case WM_RBUTTONDOWN:
        event = SystemEvent(SystemEvent::MOUSEBUTTON_DOWN, window, platformEvent);
        event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));

        return true;
    case WM_RBUTTONUP:
        event = SystemEvent(SystemEvent::MOUSEBUTTON_UP, window, platformEvent);
        event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));

        return true;
    case WM_MOUSEWHEEL:
    {
        event = SystemEvent(SystemEvent::MOUSESCROLL, window, platformEvent);

        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        event.GetEventData().Set(Vec2i(0, delta));

        return true;
    }
    case WM_MOUSEHWHEEL:
    {
        event = SystemEvent(SystemEvent::MOUSESCROLL, window, platformEvent);

        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        event.GetEventData().Set(Vec2i(delta, 0));

        return true;
    }
    case WM_DROPFILES:
    {
        /*HDROP hDrop = (HDROP)msg.wParam;
        UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < fileCount; ++i)
        {
            WCHAR filePath[MAX_PATH];
            DragQueryFileW(hDrop, i, filePath, MAX_PATH);
            event = SystemEvent(SystemEvent::FILE_DROP, window, platformEvent);
            event.GetEventData().Set(FilePath(filePath));
        }
        DragFinish(hDrop);*/

        break;
    }
    case WM_CLOSE:
    case WM_DESTROY:
    {
        event = SystemEvent(SystemEvent::WINDOW_CLOSE, window, platformEvent);

        return true;
    }

    default:
        break;
    }

    return false;
}

static LRESULT CALLBACK EngineWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Win32ApplicationWindow* window = reinterpret_cast<Win32ApplicationWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    AssertDebug(window != nullptr);

    SystemEvent event;
    if (HandleWindowEvent(window, event, hWnd, msg, wParam, lParam))
    {
        const SystemEvent::EventType eventType = event.GetType();

        if (eventType != SystemEvent::INVALID)
        {
            window->GetInputManager()->ProcessEvent(&event);

            return 0;
        }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK Win32ApplicationWindow::StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = static_cast<Win32ApplicationWindow*>(cs->lpCreateParams);

        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));

        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    Win32ApplicationWindow* window = reinterpret_cast<Win32ApplicationWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    if (window)
    {
        return window->WndProc(hWnd, msg, wParam, lParam);
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT Win32ApplicationWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE: // fallthrough
    // case WM_SIZING: // fallthrough
    case WM_EXITSIZEMOVE:
    {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);

        Win32ApplicationWindow* window = reinterpret_cast<Win32ApplicationWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        AssertDebug(window != nullptr);

        window->HandleResize(Vec2i(width, height));

        // event = SystemEvent(SystemEvent::WINDOW_RESIZED, platformEvent);
        // event.GetEventData().Set(Vec2i(width, height));

        break;
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
    POINT pt {};
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
        return;
    m_mouseLocked = locked;

    if (locked)
    {
        RECT rc {};
        GetClientRect(m_hwnd, &rc);
        POINT tl { rc.left, rc.top }, br { rc.right, rc.bottom };
        ClientToScreen(m_hwnd, &tl);
        ClientToScreen(m_hwnd, &br);
        RECT clip { tl.x, tl.y, br.x, br.y };
        ClipCursor(&clip);
        SetCapture(m_hwnd);
        ShowCursor(FALSE);
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

#else // Stub impls for non-Windows platforms

Win32ApplicationWindow::Win32ApplicationWindow(ANSIString title, Vec2i size)
    : ApplicationWindow(std::move(title), size)
{
    HYP_NOT_IMPLEMENTED();
}

Win32ApplicationWindow::~Win32ApplicationWindow()
{
    HYP_NOT_IMPLEMENTED();
}

void Win32ApplicationWindow::Initialize(WindowOptions windowOptions)
{
    HYP_NOT_IMPLEMENTED();
}

void Win32ApplicationWindow::SetMousePosition(Vec2i position)
{
    HYP_NOT_IMPLEMENTED();
}

Vec2i Win32ApplicationWindow::GetMousePosition() const
{
    HYP_NOT_IMPLEMENTED();
}

Vec2i Win32ApplicationWindow::GetDimensions() const
{
    HYP_NOT_IMPLEMENTED();
}

void Win32ApplicationWindow::SetIsMouseLocked(bool locked)
{
    HYP_NOT_IMPLEMENTED();
}

bool Win32ApplicationWindow::HasMouseFocus() const
{
    HYP_NOT_IMPLEMENTED();
}

#endif

#pragma endregion Win32ApplicationWindow

#pragma region Win32AppContext

Win32AppContext::Win32AppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments)
{
}

Win32AppContext::~Win32AppContext() = default;

Handle<ApplicationWindow> Win32AppContext::CreateSystemWindow(WindowOptions windowOptions)
{
    Handle<Win32ApplicationWindow> window = CreateObject<Win32ApplicationWindow>(windowOptions.title, windowOptions.dimensions);
    m_windows.PushBack(window);

    window->Initialize(windowOptions);

    return window;
}

//// \todo : Move Windows implementation to sys/platform/win32 file to reduce code bloat.
#ifdef HYP_WINDOWS

int Win32AppContext::PollEvents(SystemEvent& event)
{
    AssertOnThread(g_mainThread);

    event = SystemEvent();

    MSG msg {};

    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        Win32ApplicationWindow* window = reinterpret_cast<Win32ApplicationWindow*>(GetWindowLongPtrW(msg.hwnd, GWLP_USERDATA));

        TranslateMessage(&msg);
        DispatchMessageW(&msg);

        if (window && !window->m_useWndProc)
        {
            if (HandleWindowEvent(window, event,
                    msg.hwnd, msg.message, msg.wParam, msg.lParam))
            {
                const SystemEvent::EventType eventType = event.GetType();

                if (eventType != SystemEvent::INVALID)
                {
                    if (window)
                    {
                        return 1;
                    }
                }

                return 0;
            }
        }
    }

    return 0;
}

VkSurfaceKHR Win32AppContext::CreateVulkanSurface(
    Win32ApplicationWindow* window,
    IDummyVulkanSurfaceContext** ppOutDummySurfaceContext)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    static constexpr const wchar_t* DummyClassName = L"DummyWindowClass";

    VkWin32SurfaceCreateInfoKHR createInfo { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };

    if (window != nullptr)
    {
        Win32ApplicationWindow* win32Window = ObjCast<Win32ApplicationWindow>(window);
        Assert(win32Window != nullptr);

        if (win32Window->GetVkSurface() != VK_NULL_HANDLE)
        {
            // already have a surface created for this window
            return win32Window->GetVkSurface();
        }

        createInfo.hinstance = win32Window->GetHINSTANCE();
        createInfo.hwnd = win32Window->GetHWND();
    }
    else
    {
        if (!ppOutDummySurfaceContext)
        {
            // can't do much with this, we need dummy context in order to destruct dummy window properly
            return VK_NULL_HANDLE;
        }

        class Win32DummyVulkanSurfaceContext : public IDummyVulkanSurfaceContext
        {
        public:
            Win32DummyVulkanSurfaceContext(HINSTANCE hInstance, HWND hwnd)
                : m_hInstance(hInstance),
                  m_hwnd(hwnd)
            {
            }

            virtual ~Win32DummyVulkanSurfaceContext() override
            {
                Assert(DestroyWindow(m_hwnd));
                UnregisterClassW(DummyClassName, m_hInstance);

                sys::Win32_UnregisterWindowClass(DummyClassName);
            }

        private:
            HINSTANCE m_hInstance;
            HWND m_hwnd;
        };

        HINSTANCE hInstance = GetModuleHandleW(nullptr);

        WNDCLASSEXW windowClass = {};
        windowClass.cbSize = sizeof(WNDCLASSEXW);
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = hInstance;
        windowClass.lpszClassName = DummyClassName;

        ATOM classAtom = RegisterClassExW(&windowClass);
        if (classAtom == 0)
        {
            HYP_FAIL("Failed to register Win32 window class for Vulkan dummy window! Win32 Error: {}", GetLastError());
        }

        sys::Win32_RegisterWindowClass(DummyClassName);

        HWND hwnd = CreateWindowExW(
            0,
            DummyClassName,
            L"Hyperion Vulkan Dummy Window",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            nullptr,
            nullptr,
            hInstance,
            nullptr);

        Assert(hwnd != nullptr);

        createInfo.hinstance = hInstance;
        createInfo.hwnd = hwnd;

        *ppOutDummySurfaceContext = new Win32DummyVulkanSurfaceContext(hInstance, hwnd);
    }

    VkResult vkResult = vkCreateWin32SurfaceKHR(
        g_renderBackend->GetInstance()->GetInstance(),
        &createInfo,
        nullptr,
        &surface);

    Assert(vkResult == VK_SUCCESS, "Failed to create Win32 Vulkan surface: {}", int(vkResult));

    return surface;
}

#else

int Win32AppContext::PollEvents(SystemEvent& event)
{
    HYP_NOT_IMPLEMENTED();
}

#endif

#pragma endregion Win32AppContext

#pragma region CocoaApplicationWindow

#ifndef HYP_MACOS

CocoaApplicationWindow::CocoaApplicationWindow(ANSIString title, Vec2i size)
    : ApplicationWindow(std::move(title), size)
{
}

CocoaApplicationWindow::~CocoaApplicationWindow() = default;

void CocoaApplicationWindow::Initialize(WindowOptions windowOptions)
{
    HYP_NOT_IMPLEMENTED();
}

void CocoaApplicationWindow::SetMousePosition(Vec2i position)
{
    HYP_NOT_IMPLEMENTED();
}

Vec2i CocoaApplicationWindow::GetMousePosition() const
{
    HYP_NOT_IMPLEMENTED();
}

Vec2i CocoaApplicationWindow::GetDimensions() const
{
    HYP_NOT_IMPLEMENTED();
}

void CocoaApplicationWindow::SetIsMouseLocked(bool locked)
{
    HYP_NOT_IMPLEMENTED();
}

bool CocoaApplicationWindow::HasMouseFocus() const
{
    HYP_NOT_IMPLEMENTED();
}

bool CocoaApplicationWindow::IsHighDPI() const
{
    HYP_NOT_IMPLEMENTED();
}

#endif

#pragma endregion CocoaApplicationWindow

#pragma region CocoaAppContext

#ifndef HYP_MACOS

CocoaAppContext::CocoaAppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments)
{
}

CocoaAppContext::~CocoaAppContext() = default;

Handle<ApplicationWindow> CocoaAppContext::CreateSystemWindow(WindowOptions windowOptions)
{
    HYP_NOT_IMPLEMENTED();
}

int CocoaAppContext::PollEvents(SystemEvent& event)
{
    HYP_NOT_IMPLEMENTED();
}

#endif

#pragma endregion CocoaAppContext

} // namespace sys
} // namespace hyperion
