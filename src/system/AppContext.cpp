/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <system/AppContext.hpp>
#include <core/debug/Debug.hpp>
#include <system/SystemEvent.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/logging/Logger.hpp>

#include <core/config/Config.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderDevice.hpp>

#ifdef HYP_VULKAN
#include <rendering/vulkan/VulkanInstance.hpp>
#endif

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#ifdef HYP_SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#endif

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

HYP_API extern const GlobalConfig& GetGlobalConfig();

HYP_API const CommandLineArgumentDefinitions& DefaultCommandLineArgumentDefinitions()
{
    static const struct DefaultCommandLineArgumentDefinitionsInitializer
    {
        CommandLineArgumentDefinitions definitions;

        DefaultCommandLineArgumentDefinitionsInitializer()
        {
            definitions.Add("Profile", {}, "Enable collection of profiling data for functions that opt in using HYP_SCOPE.", CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);
            definitions.Add("TraceURL", {}, "The endpoint url that profiling data will be submitted to (this url will have /start appended to it to start the session and /results to add results)", CommandLineArgumentFlags::NONE, CommandLineArgumentType::STRING);
            definitions.Add("ResX", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::INTEGER);
            definitions.Add("ResY", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::INTEGER);
            definitions.Add("Headless", {}, {}, CommandLineArgumentFlags::NONE, CommandLineArgumentType::BOOLEAN, false);
            definitions.Add("Mode", "m", {}, CommandLineArgumentFlags::NONE, Array<String> { "precompile_shaders", "editor" }, String("editor"));
        }
    } initializer;

    return initializer.definitions;
}

namespace sys {

#pragma region ApplicationWindow

ApplicationWindow::ApplicationWindow(ANSIString title, Vec2i size)
    : m_title(std::move(title)),
      m_size(size)
{
}

void ApplicationWindow::HandleResize(Vec2i newSize)
{
    m_size = newSize;

    OnWindowSizeChanged(newSize);
}

#pragma endregion ApplicationWindow

#pragma region AppContextBase

AppContextBase::AppContextBase(ANSIString name, const CommandLineArguments& arguments)
{
    m_inputManager = CreateObject<InputManager>();

    m_name = std::move(name);

    if (m_name.Empty())
    {
        if (json::JSONValue configAppName = GetGlobalConfig().Get("app.name"))
        {
            m_name = GetGlobalConfig().Get("app.name").ToString();
        }
    }
}

AppContextBase::~AppContextBase() = default;

void AppContextBase::SetMainWindow(const Handle<ApplicationWindow>& window)
{
    m_mainWindow = window;
    m_inputManager->SetWindow(m_mainWindow.Get());

    OnCurrentWindowChanged(m_mainWindow.Get());
}

void AppContextBase::UpdateConfigurationOverrides()
{
}

#pragma endregion AppContextBase

#pragma region SDLApplicationWindow

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

    if (!(windowOptions.flags & WindowFlags::NO_GFX))
    {
#if HYP_VULKAN
        sdlFlags |= SDL_WINDOW_VULKAN;
#endif
    }

    if (windowOptions.flags & WindowFlags::HIGH_DPI)
    {
        sdlFlags |= SDL_WINDOW_ALLOW_HIGHDPI;
    }

    if (windowOptions.flags & WindowFlags::HEADLESS)
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

    m_windowHandle = SDL_CreateWindow(
        m_title.Data(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        int(m_size.x),
        int(m_size.y),
        sdlFlags);

    Assert(m_windowHandle != nullptr, "Failed to initialize window: %s", SDL_GetError());
}

void SDLApplicationWindow::SetMousePosition(Vec2i position)
{
    SDL_WarpMouseInWindow(static_cast<SDL_Window*>(m_windowHandle), position.x, position.y);
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
    SDL_GetWindowSize(static_cast<SDL_Window*>(m_windowHandle), &width, &height);

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

bool SDLApplicationWindow::HasMouseFocus() const
{
    const SDL_Window* focusWindow = SDL_GetMouseFocus();

    return focusWindow == static_cast<SDL_Window*>(m_windowHandle);
}

bool SDLApplicationWindow::IsHighDPI() const
{
    const int displayIndex = SDL_GetWindowDisplayIndex(static_cast<SDL_Window*>(m_windowHandle));

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

#pragma endregion SDLApplicationWindow

#pragma region SDLAppContext

#if defined(HYP_SDL) && defined(HYP_IOS)
static struct IOSSDLInitializer
{
    IOSSDLInitializer()
    {
        SDL_SetMainReady();
    }
} g_iosSdlInitializer = {};
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
    window->Initialize(windowOptions);

    return window;
}

int SDLAppContext::PollEvent(SystemEvent& event)
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
            event = SystemEvent(SystemEventType::EVENT_FILE_DROP, PlatformEvent(sdlEvent));
            // set event data variant to the file path
            event.GetEventData().Set(FilePath(sdlEvent.drop.file));

            // need to free or else the mem will leak
            SDL_free(sdlEvent.drop.file);
            sdlEvent.drop.file = nullptr;

            break;
        }
        case SDL_KEYDOWN:  // fallthrough
        case SDL_KEYUP:
        {
            switch (sdlEvent.type)
            {
            case SDL_KEYDOWN:
                event = SystemEvent(SystemEventType::EVENT_KEYDOWN, PlatformEvent(sdlEvent));
                break;
            case SDL_KEYUP:
                event = SystemEvent(SystemEventType::EVENT_KEYUP, PlatformEvent(sdlEvent));
                break;
            default:
                HYP_UNREACHABLE();
            }

            event.GetEventData().Set(KeyCode(sdlEvent.key.keysym.sym));

            break;
        }
        case SDL_MOUSEMOTION:
        {
            event = SystemEvent(SystemEventType::EVENT_MOUSEMOTION, PlatformEvent(sdlEvent));
            event.GetEventData().Set(Vec2i(sdlEvent.motion.x, sdlEvent.motion.y));
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: // fallthrough
        {
            switch (sdlEvent.type)
            {
            case SDL_MOUSEBUTTONDOWN:
                event = SystemEvent(SystemEventType::EVENT_MOUSEBUTTON_DOWN, PlatformEvent(sdlEvent));
                break;
            case SDL_MOUSEBUTTONUP:
                event = SystemEvent(SystemEventType::EVENT_MOUSEBUTTON_UP, PlatformEvent(sdlEvent));
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
            event = SystemEvent(SystemEventType::EVENT_MOUSESCROLL, PlatformEvent(sdlEvent));
            event.GetEventData().Set(Vec2i(sdlEvent.wheel.x, sdlEvent.wheel.y));
            break;
        }
        default:
            break;
        }
    }

    return result;
}

#pragma endregion SDLAppContext

#pragma region Win32ApplicationWindow

#ifdef HYP_WINDOWS

Win32ApplicationWindow::Win32ApplicationWindow(ANSIString title, Vec2i size)
    : ApplicationWindow(std::move(title), size)
{
    m_hinst = GetModuleHandleW(nullptr);
}

Win32ApplicationWindow::~Win32ApplicationWindow()
{
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    UnregisterClassW(m_title.ToWide().Data(), m_hinst);
}

void Win32ApplicationWindow::Initialize(WindowOptions windowOptions)
{
    m_title = windowOptions.title;
    m_size  = windowOptions.size;

    WideString wTitle = m_title.ToWide();

    WNDCLASSW wc {};
    wc.lpfnWndProc = &Win32ApplicationWindow::StaticWndProc;
    wc.hInstance = m_hinst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = wTitle.Data();

    RegisterClassW(&wc);

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT r{0, 0, (LONG)m_size.x, (LONG)m_size.y};
    AdjustWindowRect(&r, style, FALSE);

    m_hwnd = CreateWindowW(
        wTitle.Data(), wTitle.Data(), style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        nullptr, nullptr, m_hinst, this);

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
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
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void Win32ApplicationWindow::SetMousePosition(Vec2i position)
{
    POINT pt{ position.x, position.y };
    ClientToScreen(m_hwnd, &pt);
    SetCursorPos(pt.x, pt.y);
}

Vec2i Win32ApplicationWindow::GetMousePosition() const
{
    POINT pt{};
    GetCursorPos(&pt);
    ScreenToClient(m_hwnd, &pt);
    return { pt.x, pt.y };
}

Vec2i Win32ApplicationWindow::GetDimensions() const
{
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    return { rc.right - rc.left, rc.bottom - rc.top };
}

void Win32ApplicationWindow::SetIsMouseLocked(bool locked)
{
    if (m_mouseLocked == locked) return;
    m_mouseLocked = locked;

    if (locked)
    {
        RECT rc{};
        GetClientRect(m_hwnd, &rc);
        POINT tl{ rc.left, rc.top }, br{ rc.right, rc.bottom };
        ClientToScreen(m_hwnd, &tl);
        ClientToScreen(m_hwnd, &br);
        RECT clip{ tl.x, tl.y, br.x, br.y };
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

Handle<ApplicationWindow> Win32AppContext::CreateSystemWindow(WindowOptions opts)
{
    Handle<Win32ApplicationWindow> window = CreateObject<Win32ApplicationWindow>(opts.title, opts.size);
    window->Initialize(opts);

    SetMainWindow(window);

    return window;
}

#ifdef HYP_WINDOWS

static KeyCode MapWin32VirtualKeyToKeyCode(LPARAM lParam, WPARAM wParam)
{
    // Most VK_* keys are mapped directly to KeyCode, but some need special handling
    switch (wParam)
    {
    case VK_TAB: return KeyCode::TAB;
    case VK_SHIFT:
    {
        // Distinguish between left and right shift
        const bool isRight = (lParam & (1 << 24)) != 0;
        return isRight ? KeyCode::RIGHT_SHIFT : KeyCode::LEFT_SHIFT;
    }
    case VK_CONTROL:
    {
        // Distinguish between left and right control
        const bool isRight = (lParam & (1 << 24)) != 0;
        return isRight ? KeyCode::RIGHT_CTRL : KeyCode::LEFT_CTRL;
    }
    case VK_MENU:
    {
        // Distinguish between left and right alt (menu)
        const bool isRight = (lParam & (1 << 24)) != 0;
        return isRight ? KeyCode::RIGHT_ALT : KeyCode::LEFT_ALT;
    }
    case VK_CAPITAL: return KeyCode::CAPSLOCK;
    case VK_SPACE: return KeyCode::SPACE;
    case VK_LEFT: return KeyCode::ARROW_LEFT;
    case VK_UP: return KeyCode::ARROW_UP;
    case VK_RIGHT: return KeyCode::ARROW_RIGHT;
    case VK_DOWN: return KeyCode::ARROW_DOWN;
    case VK_LMENU: return KeyCode::LEFT_ALT;
    case VK_RMENU: return KeyCode::RIGHT_ALT;
    case VK_LCONTROL: return KeyCode::LEFT_CTRL;
    case VK_RCONTROL: return KeyCode::RIGHT_CTRL;
    case VK_LSHIFT: return KeyCode::LEFT_SHIFT;
    case VK_RSHIFT: return KeyCode::RIGHT_SHIFT;
    default: break;
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

    return KeyCode::UNKNOWN;
}

int Win32AppContext::PollEvent(SystemEvent& event)
{
    event = SystemEvent();

    MSG msg {};

    if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);

        PlatformEvent platformEvent {};

        platformEvent.win32Event = Win32Event();
        platformEvent.win32Event.hwnd = msg.hwnd;
        platformEvent.win32Event.message = msg.message;
        platformEvent.win32Event.wParam = msg.wParam;
        platformEvent.win32Event.lParam = msg.lParam;

        switch (msg.message)
        {
        case WM_KEYDOWN:
            event = SystemEvent(SystemEventType::EVENT_KEYDOWN, platformEvent);
            event.GetEventData().Set(MapWin32VirtualKeyToKeyCode(msg.lParam, msg.wParam));

            return 1;
        case WM_KEYUP:
            event = SystemEvent(SystemEventType::EVENT_KEYUP, platformEvent);
            event.GetEventData().Set(MapWin32VirtualKeyToKeyCode(msg.lParam, msg.wParam));

            return 1;
        case WM_MOUSEMOVE:
        {
            event = SystemEvent(SystemEventType::EVENT_MOUSEMOTION, platformEvent);

            POINT pt;
            pt.x = LOWORD(msg.lParam);
            pt.y = HIWORD(msg.lParam);

            event.GetEventData().Set(Vec2i(pt.x, pt.y));

            return 1;
        }
        case WM_LBUTTONDOWN:
            event = SystemEvent(SystemEventType::EVENT_MOUSEBUTTON_DOWN, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));

            return 1;
        case WM_LBUTTONUP:
            event = SystemEvent(SystemEventType::EVENT_MOUSEBUTTON_UP, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));

            return 1;
        case WM_MBUTTONDOWN:
            event = SystemEvent(SystemEventType::EVENT_MOUSEBUTTON_DOWN, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));

            return 1;
        case WM_MBUTTONUP:
            event = SystemEvent(SystemEventType::EVENT_MOUSEBUTTON_UP, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));

            return 1;
        case WM_RBUTTONDOWN:
            event = SystemEvent(SystemEventType::EVENT_MOUSEBUTTON_DOWN, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));

            return 1;
        case WM_RBUTTONUP:
            event = SystemEvent(SystemEventType::EVENT_MOUSEBUTTON_UP, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));

            return 1;
        case WM_MOUSEWHEEL:
        {
            event = SystemEvent(SystemEventType::EVENT_MOUSESCROLL, platformEvent);

            int delta = GET_WHEEL_DELTA_WPARAM(msg.wParam);
            event.GetEventData().Set(Vec2i(0, delta));

            return 1;
        }
        case WM_MOUSEHWHEEL:
        {
            event = SystemEvent(SystemEventType::EVENT_MOUSESCROLL, platformEvent);

            int delta = GET_WHEEL_DELTA_WPARAM(msg.wParam);
            event.GetEventData().Set(Vec2i(delta, 0));

            return 1;
        }
        case WM_DROPFILES:
        {
            /*HDROP hDrop = (HDROP)msg.wParam;
            UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < fileCount; ++i)
            {
                WCHAR filePath[MAX_PATH];
                DragQueryFileW(hDrop, i, filePath, MAX_PATH);
                event = SystemEvent(SystemEventType::EVENT_FILE_DROP, platformEvent);
                event.GetEventData().Set(FilePath(filePath));
            }
            DragFinish(hDrop);*/
            
            return 1;
        }
        case WM_CLOSE:
        case WM_DESTROY:
        {
            event = SystemEvent(SystemEventType::EVENT_WINDOW_CLOSE, platformEvent);
            PostQuitMessage(0);
            
            return 1;
        }
        case WM_SIZE:
        {
            int width = LOWORD(msg.lParam);
            int height = HIWORD(msg.lParam);

            event = SystemEvent(SystemEventType::EVENT_WINDOW_RESIZED, platformEvent);
            event.GetEventData().Set(Vec2i(width, height));

            return 1;
        }
        default:
            break;
        }
    }

    return 0;
}

#else

int Win32AppContext::PollEvent(SystemEvent& event)
{
    HYP_NOT_IMPLEMENTED();
}

#endif

#pragma endregion Win32AppContext

} // namespace sys
} // namespace hyperion
