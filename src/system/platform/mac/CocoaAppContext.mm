/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/CAMetalLayer.h>

#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <input/Keyboard.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/debug/Debug.hpp>
#include <core/logging/Logger.hpp>

#include <engine/EngineGlobals.hpp>

#include <rendering/RenderBackend.hpp>

#ifdef HYP_VULKAN
#include <rendering/vulkan/VulkanInstance.hpp>
#endif

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

void DestroyCocoaEvent(CocoaEvent& cocoaEvent)
{
    if (cocoaEvent.nsEvent != nullptr)
    {
        if ([NSThread isMainThread])
        {
            [(NSEvent*)cocoaEvent.nsEvent release];
        }
        else
        {
            // Can't use dispatch_sync here because it may lead to deadlocks
            // if called from the game thread and we are using RenderOnMainThread.
            dispatch_async(dispatch_get_main_queue(), ^{
                [(NSEvent*)cocoaEvent.nsEvent release];
            });
        }
        
        cocoaEvent.nsEvent = nullptr;
    }
}

namespace sys {

CocoaAppContext::CocoaAppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments)
{
    if (![NSThread isMainThread])
    {
        dispatch_async(dispatch_get_main_queue(), ^{
            [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            [NSApp finishLaunching];
        });
    }
    else
    {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];
    }
}

CocoaAppContext::~CocoaAppContext()
{
}

Handle<ApplicationWindow> CocoaAppContext::CreateSystemWindow(WindowOptions windowOptions, HWND parentHwnd)
{
    Handle<CocoaApplicationWindow> window = CreateObject<CocoaApplicationWindow>(windowOptions.title, windowOptions.dimensions);
    window->Initialize(windowOptions, parentHwnd);
    
    return window;
}

/// https://gist.github.com/eegrok/949034
static KeyCode MapCocoaKeyCodeToKeyCode(unsigned short keyCode)
{
    switch (keyCode)
    {
        case 0x00: return KeyCode::KEY_A;
        case 0x01: return KeyCode::KEY_S;
        case 0x02: return KeyCode::KEY_D;
        case 0x03: return KeyCode::KEY_F;
        case 0x04: return KeyCode::KEY_H;
        case 0x05: return KeyCode::KEY_G;
        case 0x06: return KeyCode::KEY_Z;
        case 0x07: return KeyCode::KEY_X;
        case 0x08: return KeyCode::KEY_C;
        case 0x09: return KeyCode::KEY_V;
        // 0x0A: Section (ISO layout) - no mapping
        case 0x0B: return KeyCode::KEY_B;
        case 0x0C: return KeyCode::KEY_Q;
        case 0x0D: return KeyCode::KEY_W;
        case 0x0E: return KeyCode::KEY_E;
        case 0x0F: return KeyCode::KEY_R;
        case 0x10: return KeyCode::KEY_Y;
        case 0x11: return KeyCode::KEY_T;
        case 0x12: return KeyCode::KEY_1;
        case 0x13: return KeyCode::KEY_2;
        case 0x14: return KeyCode::KEY_3;
        case 0x15: return KeyCode::KEY_4;
        case 0x16: return KeyCode::KEY_6;
        case 0x17: return KeyCode::KEY_5;
        // 0x18: = - no mapping
        case 0x19: return KeyCode::KEY_9;
        case 0x1A: return KeyCode::KEY_7;
        case 0x1B: return KeyCode::DASH;
        case 0x1C: return KeyCode::KEY_8;
        case 0x1D: return KeyCode::KEY_0;
        // 0x1E: ] - no mapping
        case 0x1F: return KeyCode::KEY_O;
        case 0x20: return KeyCode::KEY_U;
        // 0x21: [ - no mapping
        case 0x22: return KeyCode::KEY_I;
        case 0x23: return KeyCode::KEY_P;
        case 0x24: return KeyCode::RETURN;
        case 0x25: return KeyCode::KEY_L;
        case 0x26: return KeyCode::KEY_J;
        // 0x27: ' - no mapping
        case 0x28: return KeyCode::KEY_K;
        // 0x29: ; - no mapping
        // 0x2A: \ - no mapping
        case 0x2B: return KeyCode::COMMA;
        // 0x2C: / - no mapping
        case 0x2D: return KeyCode::KEY_N;
        case 0x2E: return KeyCode::KEY_M;
        case 0x2F: return KeyCode::PERIOD;
        case 0x30: return KeyCode::TAB;
        case 0x31: return KeyCode::SPACE;
        case 0x32: return KeyCode::TILDE;
        case 0x33: return KeyCode::BACKSPACE;
        // 0x34: Enter (on Powerbook) - no mapping
        case 0x35: return KeyCode::ESC;
        // 0x36: Right Cmd - no mapping
        // 0x37: Cmd (Apple) - no mapping
        case 0x38: return KeyCode::LEFT_SHIFT;
        case 0x39: return KeyCode::CAPSLOCK;
        case 0x3A: return KeyCode::LEFT_ALT; // Option
        case 0x3B: return KeyCode::LEFT_CTRL;
        case 0x3C: return KeyCode::RIGHT_SHIFT;
        case 0x3D: return KeyCode::RIGHT_ALT; // Right Option
        case 0x3E: return KeyCode::RIGHT_CTRL;
        // 0x3F: Fn/Globe - no mapping
        // 0x40: F17 - no mapping
        // 0x41: Numeric Keypad . - no mapping
        // 0x43: Numeric Keypad * - no mapping
        // 0x45: Numeric Keypad + - no mapping
        // 0x47: Clear (or NumLock) - no mapping
        // 0x48: Volume Up - no mapping
        // 0x49: Volume Down - no mapping
        // 0x4A: Mute - no mapping
        // 0x4B: Numeric Keypad / - no mapping
        // 0x4C: Numeric Keypad Enter - no mapping
        // 0x4E: Numeric Keypad - - no mapping
        // 0x4F: F18 - no mapping
        // 0x50: F19 - no mapping
        // 0x51: Numeric Keypad = - no mapping
        // 0x52: Numeric Keypad 0 - no mapping
        // 0x53: Numeric Keypad 1 - no mapping
        // 0x54: Numeric Keypad 2 - no mapping
        // 0x55: Numeric Keypad 3 - no mapping
        // 0x56: Numeric Keypad 4 - no mapping
        // 0x57: Numeric Keypad 5 - no mapping
        // 0x58: Numeric Keypad 6 - no mapping
        // 0x59: Numeric Keypad 7 - no mapping
        // 0x5A: F20 - no mapping
        // 0x5B: Numeric Keypad 8 - no mapping
        // 0x5C: Numeric Keypad 9 - no mapping
        // 0x5D: Yen (JIS layout) - no mapping
        // 0x5E: Underscore (JIS layout) - no mapping
        // 0x5F: Keypad Comma/Separator (JIS layout) - no mapping
        case 0x60: return KeyCode::KEY_F5;
        case 0x61: return KeyCode::KEY_F6;
        case 0x62: return KeyCode::KEY_F7;
        case 0x63: return KeyCode::KEY_F3;
        case 0x64: return KeyCode::KEY_F8;
        case 0x65: return KeyCode::KEY_F9;
        // 0x66: Eisu (JIS layout) - no mapping
        case 0x67: return KeyCode::KEY_F11;
        // 0x68: Kana (JIS layout) - no mapping
        // 0x69: F13 - no mapping
        // 0x6A: F16 - no mapping
        // 0x6B: F14 - no mapping
        case 0x6D: return KeyCode::KEY_F10;
        // 0x6E: Menu (on PC) - no mapping
        case 0x6F: return KeyCode::KEY_F12;
        // 0x71: F15 - no mapping
        // 0x72: Help - no mapping
        // 0x73: Home - no mapping
        // 0x74: Page Up - no mapping
        // 0x75: Del (Below the Help Key) - no mapping
        case 0x76: return KeyCode::KEY_F4;
        // 0x77: End - no mapping
        case 0x78: return KeyCode::KEY_F2;
        // 0x79: Page Down - no mapping
        case 0x7A: return KeyCode::KEY_F1;
        case 0x7B: return KeyCode::ARROW_LEFT;
        case 0x7C: return KeyCode::ARROW_RIGHT;
        case 0x7D: return KeyCode::ARROW_DOWN;
        case 0x7E: return KeyCode::ARROW_UP;
        // 0x7F: Power (on PC) - no mapping
        // 0xA0: Mission Control - no mapping
        default: return KeyCode::UNKNOWN;
    }
}

int CocoaAppContext::PollEvents(SystemEvent& event)
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    event = SystemEvent();
    
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
        
        [NSApp sendEvent:nsEvent];
        [NSApp updateWindows];
        
        PlatformEvent platformEvent {};
        platformEvent.cocoaEvent.nsEvent = (void*)[nsEvent retain]; // keep it around, we release it manually in DestroyCocoaEvent()

        switch ([nsEvent type])
        {
        case NSEventTypeKeyDown:
            event = SystemEvent(SystemEvent::KEYDOWN, platformEvent);
            event.GetEventData().Set(MapCocoaKeyCodeToKeyCode([nsEvent keyCode]));
            break;
            
        case NSEventTypeKeyUp:
            event = SystemEvent(SystemEvent::KEYUP, platformEvent);
            event.GetEventData().Set(MapCocoaKeyCodeToKeyCode([nsEvent keyCode]));
            break;
            
        case NSEventTypeMouseMoved:
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDragged:
        case NSEventTypeOtherMouseDragged:
        {
            event = SystemEvent(SystemEvent::MOUSEMOTION, platformEvent);
            
            // Check if mouse is locked
            bool isMouseLocked = false;
            CocoaApplicationWindow* cocoaWindow = nullptr;
            
            if (m_mainWindow)
            {
                cocoaWindow = ObjCast<CocoaApplicationWindow>(m_mainWindow.Get());
                
                if (cocoaWindow)
                {
                    isMouseLocked = cocoaWindow->IsMouseLocked();
                }
            }
            
            if (isMouseLocked && cocoaWindow)
            {
                CGFloat deltaX = [nsEvent deltaX];
                CGFloat deltaY = [nsEvent deltaY];
                
                Vec2i currentPos = cocoaWindow->GetMousePosition();
                Vec2i newPos = currentPos + Vec2i((int)deltaX, (int)deltaY);
                
                Vec2i windowSize = cocoaWindow->GetDimensions();
                newPos.x = MathUtil::Clamp(newPos.x, 0, windowSize.x - 1);
                newPos.y = MathUtil::Clamp(newPos.y, 0, windowSize.y - 1);

                cocoaWindow->SetMousePosition(newPos);
                
                event.GetEventData().Set(newPos);
            }
            else
            {
                NSWindow* window = [nsEvent window];
                
                if (!window && cocoaWindow)
                {
                    window = cocoaWindow->GetNSWindow();
                }
                
                NSPoint location = [nsEvent locationInWindow];
                
                // Flip Y coordinate (Cocoa has origin at bottom-left)
                if (window)
                {
                    NSRect frame = [window.contentView frame];
                    location.y = frame.size.height - location.y;
                }
                
                event.GetEventData().Set(Vec2i((int)location.x, (int)location.y));
            }
            
            break;
        }
            
        case NSEventTypeLeftMouseDown:
            event = SystemEvent(SystemEvent::MOUSEBUTTON_DOWN, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));
            break;
            
        case NSEventTypeLeftMouseUp:
            event = SystemEvent(SystemEvent::MOUSEBUTTON_UP, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));
            break;
            
        case NSEventTypeRightMouseDown:
            event = SystemEvent(SystemEvent::MOUSEBUTTON_DOWN, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));
            break;
            
        case NSEventTypeRightMouseUp:
            event = SystemEvent(SystemEvent::MOUSEBUTTON_UP, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));
            break;
            
        case NSEventTypeOtherMouseDown:
            event = SystemEvent(SystemEvent::MOUSEBUTTON_DOWN, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));
            break;
            
        case NSEventTypeOtherMouseUp:
            event = SystemEvent(SystemEvent::MOUSEBUTTON_UP, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));
            break;
            
        case NSEventTypeScrollWheel:
        {
            event = SystemEvent(SystemEvent::MOUSESCROLL, platformEvent);
            CGFloat deltaX = [nsEvent scrollingDeltaX];
            CGFloat deltaY = [nsEvent scrollingDeltaY];
            
            // If the scroll event has precise deltas, use them
            if ([nsEvent hasPreciseScrollingDeltas])
            {
                // Scale down precise deltas
                deltaX *= 0.1;
                deltaY *= 0.1;
            }
            
            event.GetEventData().Set(Vec2i((int)deltaX, (int)deltaY));
            break;
        }
            
        default:
            break;
        }

        if (event.GetType() != SystemEvent::INVALID)
        {
            if (m_mainWindow)
            {
                m_mainWindow->GetInputEventSink().Push(std::move(event));

                return 1;
            }
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
        // Acquire the CAMetalLayer from the CocoaApplicationWindow on the main thread
        if ([NSThread isMainThread])
        {
            CocoaApplicationWindow* cocoaWindow = ObjCast<CocoaApplicationWindow>(window);
            Assert(cocoaWindow != nullptr);
            createInfo.pLayer = (CAMetalLayer*)cocoaWindow->GetCAMetalLayer();
        }
        else
        {
            AssertOnThread(g_renderThread);

            __block CAMetalLayer* layer = nullptr;
            dispatch_sync(dispatch_get_main_queue(), ^{
                CocoaApplicationWindow* cocoaWindow = ObjCast<CocoaApplicationWindow>(window);
                Assert(cocoaWindow != nullptr);
                layer = (CAMetalLayer*)cocoaWindow->GetCAMetalLayer();
            });
            createInfo.pLayer = layer;
        }
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
                    // Close the window on the main thread. Avoid explicitly
                    // calling `release` here to prevent potential double-release
                    // if the window was already released elsewhere. This keeps
                    // us safe from crashes; if necessary we can revisit ownership
                    // to avoid leaks.
                    if ([NSThread isMainThread])
                    {
                        [m_window close];
                    }
                    else
                    {
                        // will only occur if RenderOnMainThread is false

                        AssertOnThread(g_renderThread);

                        NSWindow *w = m_window;
                        dispatch_sync(dispatch_get_main_queue(), ^{
                            [w close];
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

    VkResult vkResult = vkCreateMetalSurfaceEXT(
        g_renderBackend->GetInstance()->GetInstance(),
        &createInfo,
        nullptr,
        &surface);

    Assert(vkResult == VK_SUCCESS, "Failed to create Metal Vulkan surface: {}", int(vkResult));

    return surface;
}

} // namespace sys
} // namespace hyperion
