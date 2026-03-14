/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <SystemPch.hpp>

#include <system/AppContext.hpp>

#include <input/InputManager.hpp>
#include <input/Event.hpp>

#include <Core/threading/Threads.hpp>

#include <Core/debug/Debug.hpp>

#include <android/looper.h>

#include <fcntl.h>
#include <unistd.h>

#if HYP_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>

#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#endif

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

static constexpr int LOOPER_ID_MAIN = 1;

AndroidAppContext::AndroidAppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments)
{
    InitializeLooper();
}

AndroidAppContext::~AndroidAppContext()
{
    ShutdownLooper();
}

void AndroidAppContext::InitializeLooper()
{
    m_looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    ALooper_acquire(m_looper);

    if (pipe(m_pipeFd) != 0)
    {
        HYP_FAIL("Failed to create pipe for AndroidAppContext");
    }

    // Non-blocking read so the drain loop in PollEvents never stalls
    fcntl(m_pipeFd[0], F_SETFL, fcntl(m_pipeFd[0], F_GETFL) | O_NONBLOCK);

    ALooper_addFd(
        m_looper,
        m_pipeFd[0],
        LOOPER_ID_MAIN,
        ALOOPER_EVENT_INPUT,
        nullptr,
        nullptr);
}

void AndroidAppContext::ShutdownLooper()
{
    if (m_looper != nullptr)
    {
        ALooper_removeFd(m_looper, m_pipeFd[0]);
        ALooper_release(m_looper);
        m_looper = nullptr;
    }

    if (m_pipeFd[0] != -1) { close(m_pipeFd[0]); m_pipeFd[0] = -1; }
    if (m_pipeFd[1] != -1) { close(m_pipeFd[1]); m_pipeFd[1] = -1; }
}

Handle<ApplicationWindow> AndroidAppContext::CreateSystemWindow(WindowOptions windowOptions)
{
    Handle<AndroidApplicationWindow> window = MakeHandle<AndroidApplicationWindow>(windowOptions.title, windowOptions.dimensions);
    m_windows.PushBack(window);

    window->Initialize(windowOptions);

    return window;
}

void AndroidAppContext::SetNativeWindow(void* nativeWindow)
{
    if (m_mainWindow == nullptr)
    {
        HYP_LOG(Core, Warning, "AndroidAppContext::SetNativeWindow called but no main window is set");
        return;
    }

    AndroidApplicationWindow* androidWindow = ObjCast<AndroidApplicationWindow>(m_mainWindow);
    Assert(androidWindow != nullptr);

    androidWindow->SetNativeWindow(nativeWindow);
}

int AndroidAppContext::PollEvents(Event& event)
{
    AssertOnThread(g_mainThread);

    // wake up looper
    ALooper_pollOnce(0, nullptr, nullptr, nullptr);

    char buf;
    while (read(m_pipeFd[0], &buf, 1) > 0) {}

    Array<Event> pending;

    {
        Mutex::Guard lock(m_eventQueueMtx);
        pending = std::move(m_eventQueue);
    }

    InputManager* inputManager = m_mainWindow != nullptr
        ? m_mainWindow->GetInputManager()
        : nullptr;

    for (Event& pending_event : pending)
    {
        switch (pending_event.GetType())
        {
        case EventType::KEYDOWN:
        case EventType::KEYUP:
        case EventType::MOUSEMOTION:
        case EventType::MOUSEBUTTON_DOWN:
        case EventType::MOUSEBUTTON_UP:
        case EventType::MOUSESCROLL:
            if (inputManager != nullptr)
            {
                inputManager->ProcessEvent(std::move(pending_event));
            }
            break;

        default:
            // return directly to caller for system events.
            event = std::move(pending_event);
            return 1;
        }
    }

    return 0;
}

void AndroidAppContext::EnqueueEvent(Event&& event)
{
    {
        Mutex::Guard lock(m_eventQueueMtx);
        m_eventQueue.PushBack(std::move(event));
    }

    // wake up looper.
    const char signal = 'W';
    write(m_pipeFd[1], &signal, 1);
}

#if HYP_VULKAN

VkSurfaceKHR AndroidAppContext::CreateVulkanSurface(
    AndroidApplicationWindow* window,
    IDummyVulkanSurfaceContext** ppOutDummySurfaceContext)
{
    ANativeWindow* nativeWindow = nullptr;

    // window may be nullptr to allow creation of a dummy surface
    if (window != nullptr)
    {
        if (window->GetVkSurface() != VK_NULL_HANDLE)
        {
            return window->GetVkSurface();
        }

        nativeWindow = static_cast<ANativeWindow*>(window->GetNativeWindow());
        Assert(nativeWindow != nullptr); // if window is provided native window must not be null
    }

    Assert(g_renderInterface->GetInstance()->GetInstance() != VK_NULL_HANDLE);

    VkAndroidSurfaceCreateInfoKHR createInfo { VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR };
    createInfo.window = nativeWindow;

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    const VkResult result = vkCreateAndroidSurfaceKHR(
        g_renderInterface->GetInstance()->GetInstance(),
        &createInfo,
        nullptr,
        &surface);

    Assert(result == VK_SUCCESS, "Failed to create Android Vulkan surface: {}", int(result));

    return surface;
}

#endif // HYP_VULKAN

} // namespace Hyperion
