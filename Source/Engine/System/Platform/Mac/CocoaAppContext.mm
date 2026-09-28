/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#import <AppKit/AppKit.h>

#include <dispatch/dispatch.h>

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/CAMetalLayer.h>

#include <SystemPch.hpp>

#include <System/AppContext.hpp>

#include <Input/Keyboard.hpp>
#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Scheduler.hpp>

#include <Core/Debug/Debug.hpp>

#include <HyperionEngine.hpp>

#if HYP_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>

#include <Rendering/Vulkan/VulkanInstance.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#endif

@interface HyperionAppDelegate : NSObject<NSApplicationDelegate>
@end

@implementation HyperionAppDelegate

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    using namespace Hyperion;

    if (!g_appContext.IsValid())
    {
        return NSTerminateNow;
    }

    // need to copy so we don't invalidate the list
    auto windows = g_appContext->GetWindows();

    for (const auto& window : windows)
    {
        window->Close();
    }

    if (windows.Empty())
    {
        Hyp_RequestQuit();
    }

    return NSTerminateCancel;
}

@end

namespace Hyperion {

CORE_API HYP_DECLARE_LOG_CHANNEL(Core);

static void InitCocoaApplication()
{
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    if (!EngineGlobals::IsEditor() && [NSApp delegate] == nil)
    {
        [NSApp setDelegate:[[HyperionAppDelegate alloc] init]];
    }

    [NSApp finishLaunching];
}

void DestroyCocoaEvent(CocoaEvent& cocoaEvent)
{
    if (cocoaEvent.nsEvent != nullptr)
    {
        Assert([NSThread isMainThread]);
        [(NSEvent*)cocoaEvent.nsEvent release];
        cocoaEvent.nsEvent = nullptr;
    }
}

CocoaAppContext::CocoaAppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments)
{
    if (![NSThread isMainThread])
    {
        dispatch_async(dispatch_get_main_queue(), ^{
            InitCocoaApplication();
        });
    }
    else
    {
        InitCocoaApplication();
    }
}

CocoaAppContext::~CocoaAppContext()
{
}

Handle<ApplicationWindow> CocoaAppContext::CreateSystemWindow(WindowOptions windowOptions)
{
    Handle<CocoaApplicationWindow> window = MakeHandle<CocoaApplicationWindow>(windowOptions.title, windowOptions.dimensions);
    m_windows.PushBack(window);

    window->Initialize(windowOptions);

    return window;
}

int CocoaAppContext::PollEvents(Event& event)
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    event = Event();

    @autoreleasepool
    {
        NSEvent* nsEvent = [NSApp nextEventMatchingMask:NSEventMaskAny
                                              untilDate:nil
                                                 inMode:NSDefaultRunLoopMode
                                                 dequeue:YES];

        if (!nsEvent)
        {
            return 0;
        }

        NSWindow* nsWindow = [nsEvent window];
        CocoaApplicationWindow* cocoaWindow = nullptr;

        // Find the Cocoa window that corresponds to this event
        if (nsWindow)
        {
            auto cocoaWindowIt = m_windows.FindIf(
                [nsWindow](const Handle<ApplicationWindow>& window)
                {
                    AssertDebug(window->IsA(CocoaApplicationWindow::StaticClass()));

                    CocoaApplicationWindow* cocoaWindow = static_cast<CocoaApplicationWindow*>(window.Get());
                    return (NSWindow*)cocoaWindow->GetNSWindow() == nsWindow;
                });

            cocoaWindow = (cocoaWindowIt != m_windows.End())
                ? static_cast<CocoaApplicationWindow*>(cocoaWindowIt->Get())
                : nullptr;
        }

        const NSEventType eventType = [nsEvent type];
        const bool isKeyUpDownEvent = (eventType == NSEventTypeKeyDown || eventType == NSEventTypeKeyUp);

        // When Cocoa events are enabled, the view's keyDown:/keyUp: handlers process
        // key events, so we must route them via sendEvent:. When Cocoa events are
        // disabled, we handle key events ourselves via HandleNSEvent and must NOT
        // route them via sendEvent: to prevent them from falling through the
        // responder chain and causing NSBeep.
        const bool shouldSendEvent = !isKeyUpDownEvent || (cocoaWindow && cocoaWindow->UseCocoaEvents());

        if (shouldSendEvent)
        {
            [NSApp sendEvent:nsEvent];
        }

        [NSApp updateWindows];

        if (!nsWindow)
        {
            return 0;
        }

        if (cocoaWindow
            && !cocoaWindow->UseCocoaEvents()
            && cocoaWindow->HandleNSEvent(nsEvent, event))
        {
            return 1;
        }
    }

    return 0;
}

VkSurfaceKHR CocoaAppContext::CreateVulkanSurface(
    CocoaApplicationWindow *window,
    IDummyVulkanSurfaceContext **ppOutDummySurfaceContext)
{
    // Cocoa objects (NSWindow/NSView/CAMetalLayer) must be created on the
    // main thread. If this method is invoked on another thread (for example
    // when using dedicated render trhead), dispatch the Cocoa-specific parts to the
    // main queue and block until they complete.

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkMetalSurfaceCreateInfoEXT createInfo { VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT };

    if (window)
    {
        CocoaApplicationWindow* cocoaWindow = DynamicCast<CocoaApplicationWindow>(window);
        Assert(cocoaWindow != nullptr);
        __block CAMetalLayer* layer = (CAMetalLayer*)cocoaWindow->GetCAMetalLayer();

        createInfo.pLayer = layer;
    }
    else
    {
        // do same thing as Win32 dummy surface creation
        if (!ppOutDummySurfaceContext)
        {
            // can't do much with this, we need dummy context in order to destruct dummy window properly
            return VK_NULL_HANDLE;
        }

        __block NSWindow* nsWindow = nullptr;
        __block CAMetalLayer* layer = nullptr;

        void (^createDummyWindow)(void) = ^{
            NSRect frame = NSMakeRect(0, 0, 800, 600);
            NSWindow* w = [[NSWindow alloc] initWithContentRect:frame
                                                     styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
                                                       backing:NSBackingStoreBuffered
                                                         defer:NO];
            [w setTitle:@"Hyperion Vulkan Dummy Window"];

            [w.contentView setLayer:[CAMetalLayer layer]];
            [w.contentView setWantsLayer:YES];

            nsWindow = w;
            layer = (CAMetalLayer*)w.contentView.layer;
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

        class CocoaDummyVulkanSurfaceContext : public IDummyVulkanSurfaceContext
        {
        public:
            CocoaDummyVulkanSurfaceContext(NSWindow* window)
                : m_window(window)
            {
            }

            virtual ~CocoaDummyVulkanSurfaceContext() override
            {
                if (m_window)
                {
                    if ([NSThread isMainThread])
                    {
                        [m_window close];
                        //[m_window release];
                    }
                    else
                    {
                        // will only occur if RenderOnMainThread is false
                        AssertOnThread(g_renderThread);

                        __block NSWindow* nsWindow = m_window;
                        dispatch_async(dispatch_get_main_queue(), ^{
                            [nsWindow close];
                            //[nsWindow release];
                        });
                    }

                    m_window = nullptr;
                }
            }

        private:
            NSWindow* m_window;
        };

        *ppOutDummySurfaceContext = new CocoaDummyVulkanSurfaceContext(nsWindow);

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

} // namespace Hyperion
