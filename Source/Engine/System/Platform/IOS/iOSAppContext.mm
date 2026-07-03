/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include <dispatch/dispatch.h>

#include <SystemPch.hpp>

#include <System/AppContext.hpp>

#include <Input/Keyboard.hpp>
#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Scheduler.hpp>

#include <Core/Debug/Debug.hpp>

#if HYP_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>

#include <Rendering/Vulkan/VulkanInstance.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#endif

// Minimal UIView subclass that uses CAMetalLayer as its backing layer.
// On iOS, UIView does not allow setting the layer property directly;
// the layer class is determined by +layerClass.
@interface DummyMetalLayerView : UIView
@end
@implementation DummyMetalLayerView
+ (Class)layerClass { return [CAMetalLayer class]; }
@end

namespace Hyperion {

CORE_API HYP_DECLARE_LOG_CHANNEL(Core);

#pragma mark - IOSEventQueue

class IOSEventQueue final
{
public:
    IOSEventQueue() = default;
    ~IOSEventQueue() = default;

    void Enqueue(Event&& event)
    {
        Mutex::Guard lock(m_mutex);
        m_queue.PushBack(std::move(event));
    }

    int Dequeue(Event& outEvent)
    {
        Mutex::Guard lock(m_mutex);

        if (m_queue.Empty())
        {
            return 0;
        }

        outEvent = std::move(m_queue.Front());
        m_queue.PopFront();

        return 1;
    }

private:
    Mutex m_mutex;
    Array<Event> m_queue;
};

void DestroyIOSEvent(IOSEvent& iosEvent)
{
    if (iosEvent.uiEvent != nullptr)
    {
        Assert([NSThread isMainThread]);
        [(UIEvent*)iosEvent.uiEvent release];
        iosEvent.uiEvent = nullptr;
    }
}

#pragma mark - IOSAppContext

IOSAppContext::IOSAppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments),
      m_eventQueue(new IOSEventQueue)
{
    if (![NSThread isMainThread])
    {
        dispatch_async(dispatch_get_main_queue(), ^{
            // Ensure UIKit is set up (typically already done by the app delegate)
            [UIApplication sharedApplication];
        });
    }
    else
    {
        [UIApplication sharedApplication];
    }
}

IOSAppContext::~IOSAppContext()
{
    delete m_eventQueue;
    m_eventQueue = nullptr;
}

Handle<ApplicationWindow> IOSAppContext::CreateSystemWindow(WindowOptions windowOptions)
{
    Handle<IOSApplicationWindow> window = MakeHandle<IOSApplicationWindow>(windowOptions.title, windowOptions.dimensions);
    m_windows.PushBack(window);

    window->Initialize(windowOptions);

    return window;
}

int IOSAppContext::PollEvents(Event& event)
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    event = Event();

    @autoreleasepool
    {
        // Drain events from the queue (populated by ObjC view callbacks)
        return m_eventQueue->Dequeue(event);
    }

    return 0;
}

void IOSAppContext::EnqueueEvent(Event&& event)
{
    if (m_eventQueue != nullptr)
    {
        m_eventQueue->Enqueue(std::move(event));
    }
}

#if HYP_VULKAN

VkSurfaceKHR IOSAppContext::CreateVulkanSurface(
    IOSApplicationWindow* window,
    IDummyVulkanSurfaceContext** ppOutDummySurfaceContext)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkMetalSurfaceCreateInfoEXT createInfo { VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT };

    if (window)
    {
        IOSApplicationWindow* iosWindow = DynamicCast<IOSApplicationWindow>(window);
        Assert(iosWindow != nullptr);
        __block CAMetalLayer* layer = (CAMetalLayer*)iosWindow->GetCAMetalLayer();

        createInfo.pLayer = layer;
    }
    else
    {
        // Create a dummy surface using a hidden UIWindow with CAMetalLayer
        if (!ppOutDummySurfaceContext)
        {
            return VK_NULL_HANDLE;
        }

        __block UIWindow* dummyWindow = nil;
        __block CAMetalLayer* layer = nil;

        void (^createDummyWindow)(void) = ^{
            CGRect frame = CGRectMake(0, 0, 800, 600);
            dummyWindow = [[UIWindow alloc] initWithFrame:frame];
            dummyWindow.hidden = YES;

            DummyMetalLayerView* dummyView = [[DummyMetalLayerView alloc] initWithFrame:frame];

            dummyWindow.rootViewController = [[UIViewController alloc] init];
            dummyWindow.rootViewController.view = dummyView;
            [dummyView release];

            layer = (CAMetalLayer*)dummyView.layer;
        };

        if ([NSThread isMainThread])
        {
            createDummyWindow();
        }
        else
        {
            AssertOnThread(g_renderThread);
            dispatch_sync(dispatch_get_main_queue(), createDummyWindow);
        }

        class IOSDummyVulkanSurfaceContext : public IDummyVulkanSurfaceContext
        {
        public:
            IOSDummyVulkanSurfaceContext(UIWindow* window)
                : m_window(window)
            {
            }

            virtual ~IOSDummyVulkanSurfaceContext() override
            {
                if (m_window)
                {
                    if ([NSThread isMainThread])
                    {
                        m_window.hidden = YES;
                        [m_window release];
                    }
                    else
                    {
                        AssertOnThread(g_renderThread);
                        __block UIWindow* window = m_window;
                        dispatch_async(dispatch_get_main_queue(), ^{
                            window.hidden = YES;
                            [window release];
                        });
                    }

                    m_window = nil;
                }
            }

        private:
            UIWindow* m_window;
        };

        *ppOutDummySurfaceContext = new IOSDummyVulkanSurfaceContext(dummyWindow);

        createInfo.pLayer = layer;
    }

    Assert(RI.GetInstance()->GetInstance() != VK_NULL_HANDLE);

    VkResult vkResult = vkCreateMetalSurfaceEXT(
        RI.GetInstance()->GetInstance(),
        &createInfo,
        nullptr,
        &surface);

    Assert(vkResult == VK_SUCCESS, "Failed to create Metal Vulkan surface: {}", int(vkResult));

    return surface;
}

#endif // HYP_VULKAN

} // namespace Hyperion
