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

#include <semaphore>

#include <AppContext.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

namespace CoreApi {
CORE_API extern const GlobalConfig& GetGlobalConfig();
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
            if (g_renderInitSignal.IsSignalled())
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
static constexpr float SwapchainScale = 1.0f;
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

    m_vkSurface = RI.CreateSurface(this, nullptr);
    Assert(m_vkSurface != VK_NULL_HANDLE);
#endif

    if (IsOnThread(g_renderThread)) // if -RenderOnMainThread is set this will be the case
    {
        if (m_swapchain.IsValid())
            EnqueueDeletion(std::move(m_swapchain));

        const Vec2u swapchainSize = Vec2u(Vec2f(m_size) * SwapchainScale);

        // we need to temporarily release the lock here to avoid deadlocking the render thread
        lock.Reset();

        SwapchainRef swapchain = RI.CreateSwapchain(this, swapchainSize);
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

                SwapchainRef swapchain = RI.CreateSwapchain(this, swapchainSize);
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

    const auto DoCheck = [&]() -> bool
    {
        return commandletClass
            && commandletClass->IsDerivedFrom(CommandletBase::StaticClass())
            && !commandletClass->IsAbstract();
    };

    if (!DoCheck())
    {
        ANSIString str = commandletName;
        if (!str.EndsWith("Commandlet"))
        {
            // Try again with "Commandlet" appended to the name.
            commandletClass = ClassRegistry::GetInstance().GetClass(str + "Commandlet", /* ignoreCase */ true);
            return DoCheck() ? commandletClass : nullptr;
        }

        return nullptr;
    }

    return commandletClass;
}

#pragma endregion AppContextBase

} // namespace Hyperion
