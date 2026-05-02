/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <SystemPch.hpp>

#include <system/AppContext.hpp>

#include <input/Event.hpp>

#include <Core/cli/CommandLine.hpp>

#include <Core/debug/Debug.hpp>

#include <Core/threading/ThreadSignal.hpp>

#include <Core/reflection/ClassRegistry.hpp>

#include <Core/config/Config.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/Device.hpp>

#include <rendering/util/DeletionQueue.hpp>

#if HYP_VULKAN
#include <vulkan/vulkan.h>
#if HYP_WINDOWS
#include <vulkan/vulkan_win32.h>
#elif HYP_APPLE
#include <vulkan/vulkan_metal.h>
#elif HYP_ANDROID
#include <vulkan/vulkan_android.h>
#elif HYP_LINUX
#include <vulkan/vulkan_xlib.h>
#endif

#include <rendering/vulkan/VulkanInstance.hpp>
#endif // HYP_VULKAN

#include <rendering/Swapchain.hpp>

#include <engine/EngineDriver.hpp>

#include <engine/commandlet/Commandlet.hpp>

#include <engine/threads/MainThread.hpp>
#include <engine/threads/RenderThread.hpp>

#include <input/InputManager.hpp>

#if HYP_SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#endif // HYP_SDL

#include <semaphore>

#include <AppContext.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

namespace CoreApi {
extern const GlobalConfig& GetGlobalConfig();
} // namespace CoreApi

extern ThreadSignal g_renderInitSignal;

/*! \brief Async task object to create a window swapchain once the render API is initialized.
 *  Since some platforms require us to create the surface on the main thread (ahem, macOS), we need to defer swapchain
 *  creation until the render API is ready.
 */
struct SetupWindowSwapchainAsync
{
    WeakHandle<ApplicationWindow> windowWeak;
    bool success;

    explicit SetupWindowSwapchainAsync(const WeakHandle<ApplicationWindow>& windowWeak)
        : windowWeak(windowWeak),
          success(false)
    {
        Assert(windowWeak.IsValid());
    }

    void operator()()
    {
        // ensure window is still valid, otherwise, cancel the task
        if (Handle<ApplicationWindow> window = windowWeak.Lock(); window.IsValid())
        {
            if (g_renderInterface != nullptr && g_renderInitSignal.IsSignalled())
            {
                window->CreateSwapchain();
                success = true;
            }
            else
            {
                g_mainThreadInstance->GetScheduler().Enqueue(*this, TaskEnqueueFlags::FIRE_AND_FORGET);
            }
        }
    }
};

#pragma region ApplicationWindow

#if HYP_ANDROID || HYP_IOS
static constexpr float SwapchainScale = 0.6f;
#else
static constexpr float SwapchainScale = 1.0f;
#endif

ApplicationWindow::ApplicationWindow(ANSIString title, Vec2i size)
    : m_title(std::move(title)),
      m_size(size),
      m_hwnd(nullptr),
      m_inputManager(MakeHandle<InputManager>(this))
{
}

ApplicationWindow::~ApplicationWindow() = default;

void ApplicationWindow::HandleResize(Vec2i newSize)
{
    TUniqueLock lock(m_mtx);

    if (m_size == newSize)
    {
        return;
    }

    m_size = newSize;

    Swapchain* swapchain = m_swapchain;
    const Vec2i swapchainSize = Vec2i(Vec2f(newSize) * SwapchainScale);

    if (swapchain != nullptr)
    {
        if (IsOnThread(g_renderThread))
        {
            swapchain->SetExtent(Vec2u(swapchainSize));
        }
        else
        {
            // if we have a dedicated rendering thread we need to tell the render thread to resize the swapchain
            GetThreadById(g_renderThread)->GetScheduler().Enqueue([this, weakThis = MakeWeakRef(this), weakSwapchain = MakeWeakRef(swapchain), newSize, swapchainSize]()
                {
                    Handle<ApplicationWindow> strongThis = weakThis.Lock();
                    if (strongThis.IsValid())
                    {
                        if (GetSize() != newSize)
                        {
                            HYP_LOG(Core, Verbose,
                                "ApplicationWindow size changed on another thread before swapchain new size could be set - "
                                "aborting swapchain resize for dimensions {}",
                                newSize);

                            return;
                        }
                    }
                    else
                    {
                        HYP_LOG(Core, Warning, "ApplicationWindow expired before swapchain size could be set");
                    }

                    SwapchainRef swapchain = weakSwapchain.Lock();
                    if (!swapchain.IsValid())
                    {
                        HYP_LOG(Core, Warning, "Attempted to resize invalid swapchain on render thread!");
                        return;
                    }

                    swapchain->SetExtent(Vec2u(swapchainSize));
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }
    }

    OnWindowSizeChanged(newSize);
}

void ApplicationWindow::CreateSwapchain()
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    AssertDebug(GetDimensions() != Vec2i::Zero());

    TUniqueLock lock(m_mtx);

#if HYP_VULKAN
    if (m_vkSurface)
    {
        return; // already created. swapchain is set on render thread
    }

    m_vkSurface = g_renderInterface->CreateSurface(this, nullptr);
    Assert(m_vkSurface != VK_NULL_HANDLE);
#endif

    if (IsOnThread(g_renderThread)) // if -RenderOnMainThread is set this will be the case
    {
        if (m_swapchain.IsValid())
            EnqueueDeletion(std::move(m_swapchain));

        const Vec2u swapchainSize = Vec2u(Vec2f(m_size) * SwapchainScale);

        // we need to temporarily release the lock here to avoid deadlocking the render thread
        lock.Reset();

        SwapchainRef swapchain = g_renderInterface->CreateSwapchain(this, swapchainSize);
        Assert(swapchain.IsValid());

        lock.Reset(m_mtx);

        m_swapchain = swapchain;
    }
    else
    {
        g_renderThreadInstance->GetScheduler().Enqueue([this, weakThis = MakeWeakRef(this)]()
            {
                Handle<ApplicationWindow> strongThis = weakThis.Lock();
                if (!strongThis.IsValid())
                {
                    HYP_LOG(Core, Warning, "Attempted to create swapchain for invalid window on render thread!");
                    return;
                }

                TUniqueLock lock(m_mtx);

                const Vec2u swapchainSize = Vec2u(Vec2f(m_size) * SwapchainScale);

                SwapchainRef swapchain = g_renderInterface->CreateSwapchain(this, swapchainSize);
                Assert(swapchain.IsValid());

                if (m_swapchain.IsValid())
                    EnqueueDeletion(std::move(m_swapchain));

                m_swapchain = swapchain;
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

Swapchain* ApplicationWindow::GetSwapchain() const
{
    TSharedLock lock(m_mtx);
    return m_swapchain.Get();
}

void ApplicationWindow::SetSwapchain(const SwapchainRef& swapchain)
{
    TUniqueLock lock(m_mtx);

    if (swapchain == m_swapchain)
        return;

    if (m_swapchain.IsValid())
        EnqueueDeletion(std::move(m_swapchain));

    m_swapchain = swapchain;
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
        if (JSON::Value configAppName = CoreApi::GetGlobalConfig().Get("App.Name"))
        {
            m_name = CoreApi::GetGlobalConfig().Get("App.Name").ToString().ToAnsi();
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

    SetupWindowSwapchainAsync setupSwapchainTask(MakeWeakRef(window));
    setupSwapchainTask(); // will re-enqueue itself if render API is not ready

    OnCurrentWindowChanged(m_mainWindow);
}

void AppContextBase::RemoveWindow(ApplicationWindow* window)
{
    AssertOnThread(g_mainThread);

    auto it = m_windows.FindIf([window](const Handle<ApplicationWindow>& other)
        {
            return other.Get() == window;
        });

    AssertDebug(it != m_windows.End(), "Invalid window specified");

    if (it != m_windows.End())
    {
        m_windows.Erase(it);

        if (m_mainWindow == window)
        {
            m_mainWindow = nullptr;

            OnCurrentWindowChanged(nullptr);
        }
    }
}

Result AppContextBase::RunCommandlet(ANSIStringView commandletName, const CommandLineArguments& args)
{
    AssertOnThread(g_mainThread);

    const Class* commandletClass = ClassRegistry::GetInstance().GetClass(commandletName, /* ignoreCase */ true);

    if (!commandletClass
        || !commandletClass->IsDerivedFrom(CommandletBase::StaticClass())
        || commandletClass->IsAbstract())
    {
        return HYP_MAKE_ERROR(Error, "'{}' is not a valid commandlet class", commandletClass ? commandletClass->GetName() : Name::Invalid());
    }

    BoxedValue boxed;
    if (!commandletClass->CreateInstance(boxed))
    {
        return HYP_MAKE_ERROR(Error, "Failed to create an instance of '{}'", commandletClass->GetName());
    }

    Handle<CommandletBase>& commandlet = boxed.Get<Handle<CommandletBase>>();
    Assert(commandlet.IsValid());

    return commandlet->Run(args);
}

const Class* AppContextBase::FindCommandletClass(ANSIStringView commandletName)
{
    AssertOnThread(g_mainThread);

    const Class* commandletClass = ClassRegistry::GetInstance().GetClass(commandletName, /* ignoreCase */ true);

    if (!commandletClass
        || !commandletClass->IsDerivedFrom(CommandletBase::StaticClass())
        || commandletClass->IsAbstract())
    {
        return nullptr;
    }

    return commandletClass;
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

void SDLApplicationWindow::Close()
{
    // not implemented
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

void SDLApplicationWindow::Close()
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
    Handle<SDLApplicationWindow> window = MakeHandle<SDLApplicationWindow>(windowOptions.title, windowOptions.size);
    m_windows.PushBack(window);

    window->Initialize(windowOptions);

    return window;
}

int SDLAppContext::PollEvents(Event& event)
{
    event = Event();

    SDL_Event& sdlEvent = event.GetPlatformEvent().sdlEvent;

    const int result = SDL_PollEvent(&sdlEvent);

    if (result)
    {
        switch (sdlEvent.type)
        {
        case SDL_DROPFILE:
        {
            event = Event(EventType::FILE_DROP, PlatformEvent(sdlEvent));
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
                event = Event(EventType::KEYDOWN, PlatformEvent(sdlEvent));
                break;
            case SDL_KEYUP:
                event = Event(EventType::KEYUP, PlatformEvent(sdlEvent));
                break;
            default:
                HYP_UNREACHABLE();
            }

            event.GetEventData().Set(KeyCode(sdlEvent.key.keysym.sym));

            break;
        }
        case SDL_MOUSEMOTION:
        {
            event = Event(EventType::MOUSEMOTION, PlatformEvent(sdlEvent));
            event.GetEventData().Set(Vec2i(sdlEvent.motion.x, sdlEvent.motion.y));
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: // fallthrough
        {
            switch (sdlEvent.type)
            {
            case SDL_MOUSEBUTTONDOWN:
                event = Event(EventType::MOUSEBUTTON_DOWN, PlatformEvent(sdlEvent));
                break;
            case SDL_MOUSEBUTTONUP:
                event = Event(EventType::MOUSEBUTTON_UP, PlatformEvent(sdlEvent));
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
            event = Event(EventType::MOUSESCROLL, PlatformEvent(sdlEvent));
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

                event = Event(EventType::WINDOW_RESIZED, PlatformEvent(sdlEvent));
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

    SDLApplicationWindow* sdlWindow = DynamicCast<SDLApplicationWindow>(window);
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

int SDLAppContext::PollEvents(Event& event)
{
    HYP_NOT_IMPLEMENTED();
}

#endif

#pragma endregion SDLAppContext



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

void CocoaApplicationWindow::Close()
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

int CocoaAppContext::PollEvents(Event& event)
{
    HYP_NOT_IMPLEMENTED();
}

#endif

#pragma endregion CocoaAppContext

#pragma region AndroidApplicationWindow

#ifndef HYP_ANDROID

AndroidApplicationWindow::AndroidApplicationWindow(ANSIString title, Vec2i size)
    : ApplicationWindow(std::move(title), size)
{
}

AndroidApplicationWindow::~AndroidApplicationWindow() = default;

void AndroidApplicationWindow::Initialize(WindowOptions windowOptions)
{
    HYP_NOT_IMPLEMENTED();
}

void AndroidApplicationWindow::SetNativeWindow(void* nativeWindow)
{
    HYP_NOT_IMPLEMENTED();
}

void AndroidApplicationWindow::SetMousePosition(Vec2i position)
{
    HYP_NOT_IMPLEMENTED();
}

Vec2i AndroidApplicationWindow::GetMousePosition() const
{
    HYP_NOT_IMPLEMENTED();
}

Vec2i AndroidApplicationWindow::GetDimensions() const
{
    HYP_NOT_IMPLEMENTED();
}

void AndroidApplicationWindow::SetIsMouseLocked(bool locked)
{
    HYP_NOT_IMPLEMENTED();
}

bool AndroidApplicationWindow::HasMouseFocus() const
{
    HYP_NOT_IMPLEMENTED();
}

void AndroidApplicationWindow::Close()
{
    HYP_NOT_IMPLEMENTED();
}

bool AndroidApplicationWindow::HandleInputEvent(int32 type, int32 action, float x, float y, int32 intParam, Event& outEvent)
{
    HYP_NOT_IMPLEMENTED();
}

#endif

#pragma endregion AndroidApplicationWindow

#pragma region AndroidAppContext

#ifndef HYP_ANDROID

AndroidAppContext::AndroidAppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments)
{
}

AndroidAppContext::~AndroidAppContext() = default;

Handle<ApplicationWindow> AndroidAppContext::CreateSystemWindow(WindowOptions windowOptions)
{
    HYP_NOT_IMPLEMENTED();
}

int AndroidAppContext::PollEvents(Event& event)
{
    HYP_NOT_IMPLEMENTED();
}

void AndroidAppContext::SetNativeWindow(void* nativeWindow)
{
    HYP_NOT_IMPLEMENTED();
}

void AndroidAppContext::EnqueueEvent(Event&&)
{
    HYP_NOT_IMPLEMENTED();
}

#endif

#pragma endregion AndroidAppContext

} // namespace Hyperion
