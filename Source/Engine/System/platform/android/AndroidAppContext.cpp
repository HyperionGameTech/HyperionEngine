/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <SystemPch.hpp>

#include <System/AppContext.hpp>

#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <Core/threading/Threads.hpp>

#include <Core/debug/Debug.hpp>

#include <android/looper.h>

#include <fcntl.h>
#include <unistd.h>

#if HYP_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>

#include <Rendering/vulkan/VulkanInstance.hpp>
#include <Rendering/vulkan/VulkanRenderInterface.hpp>
#endif

namespace Hyperion {

CORE_API HYP_DECLARE_LOG_CHANNEL(Core);

#pragma region AndroidLooperThread

class AndroidLooperThread final : public Thread<Scheduler>
{
public:
    static constexpr int LooperId = 1;

    AndroidLooperThread()
        : Thread(ThreadId(NAME("AndroidLooperThread")), ThreadPriorityValue::NORMAL),
          m_looper(nullptr),
          m_pipeFd { -1, -1 }
    {
    }

    /*! \brief Called from main thread to pull events from the looper thread! Call until returns 0 to drain all events from the looper thread. */
    int PullNextEvent(Event& event);

    void EnqueueEvent(Event&& event);

    void Stop() override
    {
        Thread<Scheduler>::Stop();
        Wake(); // ensure poll loop unblocks promptly
    }

private:
    virtual void operator()() override
    {
        InitializeLooper();
        while (!m_stopRequested.Load())
        {
            DoWork();
        }
        ShutdownLooper();
    }

    void InitializeLooper();
    void ShutdownLooper();

    void DoWork();

    void Wake();
    void DrainWakePipe();
    void FlushPendingEvents();

    static int PipeFdCallback(int fd, int events, void* data);

    ALooper* m_looper;
    int m_pipeFd[2];

    Mutex m_pendingEventQueueMutex;
    Array<Event, DynamicAllocator> m_pendingEventQueue;

    Mutex m_readyEventQueueMutex;
    Array<Event, DynamicAllocator> m_readyEventQueue;
};

int AndroidLooperThread::PullNextEvent(Event& event)
{
    // pull events that the looper thread has marked ready for the main thread
    {
        Mutex::Guard lock(m_readyEventQueueMutex);

        if (!m_readyEventQueue.Empty())
        {
            event = std::move(m_readyEventQueue.Front());
            m_readyEventQueue.PopFront();

            return 1;
        }
    }

    return 0;
}

void AndroidLooperThread::EnqueueEvent(Event&& event)
{
    {
        Mutex::Guard lock(m_pendingEventQueueMutex);
        m_pendingEventQueue.PushBack(std::move(event));
    }

    Wake();
}

void AndroidLooperThread::InitializeLooper()
{
    m_looper = ALooper_forThread();

    if (m_looper == nullptr)
    {
        m_looper = ALooper_prepare(0);
    }

    Assert(m_looper != nullptr);
    ALooper_acquire(m_looper);

    if (pipe(m_pipeFd) != 0)
    {
        HYP_FAIL("Failed to create pipe for AndroidLooperThread");
    }

    // Non-blocking read so DrainWakePipe() can clear all signals safely.
    fcntl(m_pipeFd[0], F_SETFL, fcntl(m_pipeFd[0], F_GETFL, 0) | O_NONBLOCK);

    const int addFdResult = ALooper_addFd(
        m_looper,
        m_pipeFd[0],
        LooperId,
        ALOOPER_EVENT_INPUT,
        &AndroidLooperThread::PipeFdCallback,
        this);

    Assert(addFdResult == 1, "Failed to add wake pipe to Android looper (result={})", addFdResult);

}

void AndroidLooperThread::ShutdownLooper()
{
    if (m_looper != nullptr)
    {
        if (m_pipeFd[0] != -1)
        {
            ALooper_removeFd(m_looper, m_pipeFd[0]);
        }

        ALooper_release(m_looper);
        m_looper = nullptr;
    }

    if (m_pipeFd[0] != -1)
    {
        close(m_pipeFd[0]);
        m_pipeFd[0] = -1;
    }

    if (m_pipeFd[1] != -1)
    {
        close(m_pipeFd[1]);
        m_pipeFd[1] = -1;
    }
}

void AndroidLooperThread::DoWork()
{
    // Block until the wake pipe is signaled, then callback flushes pending events.
    ALooper_pollOnce(-1, nullptr, nullptr, nullptr);
}

void AndroidLooperThread::Wake()
{
    if (m_pipeFd[1] == -1)
    {
        return;
    }

    const char signal = 'W';
    write(m_pipeFd[1], &signal, 1);
}

void AndroidLooperThread::DrainWakePipe()
{
    char buf[64];

    while (read(m_pipeFd[0], buf, sizeof(buf)) > 0)
    {
    }
}

void AndroidLooperThread::FlushPendingEvents()
{
    Array<Event, DynamicAllocator> pending;

    {
        Mutex::Guard lock(m_pendingEventQueueMutex);

        if (m_pendingEventQueue.Empty())
        {
            return;
        }

        pending = std::move(m_pendingEventQueue);
    }

    {
        Mutex::Guard lock(m_readyEventQueueMutex);

        for (Event& evt : pending)
        {
            m_readyEventQueue.PushBack(std::move(evt));
        }
    }
}

int AndroidLooperThread::PipeFdCallback(int fd, int events, void* data)
{
    if ((events & ALOOPER_EVENT_INPUT) == 0 || data == nullptr)
    {
        return 1;
    }

    AndroidLooperThread* looperThread = static_cast<AndroidLooperThread*>(data);

    looperThread->DrainWakePipe();
    looperThread->FlushPendingEvents();

    return 1;
}

#pragma endregion AndroidLooperThread

AndroidAppContext::AndroidAppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments),
      m_looperThread(new AndroidLooperThread)
{
    const bool wasStarted = m_looperThread->Start();
    Assert(wasStarted, "Failed to start Android looper thread!");
}

AndroidAppContext::~AndroidAppContext()
{
    if (m_looperThread != nullptr)
    {
        if (m_looperThread->IsRunning())
        {
            m_looperThread->Stop();

            Assert(m_looperThread->Join());
        }

        delete m_looperThread;
        m_looperThread = nullptr;
    }
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

    AndroidApplicationWindow* androidWindow = DynamicCast<AndroidApplicationWindow>(m_mainWindow);
    Assert(androidWindow != nullptr);

    androidWindow->SetNativeWindow(nativeWindow);
}

int AndroidAppContext::PollEvents(Event& event)
{
    // drain events from looper thread!

    Assert(m_looperThread != nullptr);

    return m_looperThread->PullNextEvent(event);
}

void AndroidAppContext::EnqueueEvent(Event&& event)
{
    if (m_looperThread != nullptr)
    {
        m_looperThread->EnqueueEvent(std::move(event));
    }
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

    Assert(RI.GetInstance()->GetInstance() != VK_NULL_HANDLE);

    VkAndroidSurfaceCreateInfoKHR createInfo { VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR };
    createInfo.window = nativeWindow;

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    const VkResult result = vkCreateAndroidSurfaceKHR(
        RI.GetInstance()->GetInstance(),
        &createInfo,
        nullptr,
        &surface);

    Assert(result == VK_SUCCESS, "Failed to create Android Vulkan surface: {}", int(result));

    return surface;
}

#endif // HYP_VULKAN

} // namespace Hyperion
