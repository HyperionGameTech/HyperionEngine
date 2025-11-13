/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>

#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <input/Keyboard.hpp>

#include <core/debug/Debug.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

namespace sys {

CocoaAppContext::CocoaAppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments)
{
    @autoreleasepool
    {
        // Initialize the application
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];
        
        HYP_LOG(Core, Debug, "CocoaAppContext initialized");
    }
}

CocoaAppContext::~CocoaAppContext()
{
}

Handle<ApplicationWindow> CocoaAppContext::CreateSystemWindow(WindowOptions windowOptions)
{
    Handle<CocoaApplicationWindow> window = CreateObject<CocoaApplicationWindow>(windowOptions.title, windowOptions.size);
    window->Initialize(windowOptions);
    
    SetMainWindow(window);
    
    return window;
}

// Key code mapping from macOS to engine KeyCode
static KeyCode MapCocoaKeyCodeToKeyCode(unsigned short keyCode)
{
    // macOS virtual key codes
    // Reference: https://eastmanreference.com/complete-list-of-applescript-key-codes
    switch (keyCode)
    {
        case 0x00: return KeyCode::KEY_A;
        case 0x0B: return KeyCode::KEY_B;
        case 0x08: return KeyCode::KEY_C;
        case 0x02: return KeyCode::KEY_D;
        case 0x0E: return KeyCode::KEY_E;
        case 0x03: return KeyCode::KEY_F;
        case 0x05: return KeyCode::KEY_G;
        case 0x04: return KeyCode::KEY_H;
        case 0x22: return KeyCode::KEY_I;
        case 0x26: return KeyCode::KEY_J;
        case 0x28: return KeyCode::KEY_K;
        case 0x25: return KeyCode::KEY_L;
        case 0x2E: return KeyCode::KEY_M;
        case 0x2D: return KeyCode::KEY_N;
        case 0x1F: return KeyCode::KEY_O;
        case 0x23: return KeyCode::KEY_P;
        case 0x0C: return KeyCode::KEY_Q;
        case 0x0F: return KeyCode::KEY_R;
        case 0x01: return KeyCode::KEY_S;
        case 0x11: return KeyCode::KEY_T;
        case 0x20: return KeyCode::KEY_U;
        case 0x09: return KeyCode::KEY_V;
        case 0x0D: return KeyCode::KEY_W;
        case 0x07: return KeyCode::KEY_X;
        case 0x10: return KeyCode::KEY_Y;
        case 0x06: return KeyCode::KEY_Z;
        
        case 0x1D: return KeyCode::KEY_0;
        case 0x12: return KeyCode::KEY_1;
        case 0x13: return KeyCode::KEY_2;
        case 0x14: return KeyCode::KEY_3;
        case 0x15: return KeyCode::KEY_4;
        case 0x17: return KeyCode::KEY_5;
        case 0x16: return KeyCode::KEY_6;
        case 0x1A: return KeyCode::KEY_7;
        case 0x1C: return KeyCode::KEY_8;
        case 0x19: return KeyCode::KEY_9;
        
        case 0x31: return KeyCode::SPACE;
        case 0x30: return KeyCode::TAB;
        case 0x33: return KeyCode::BACKSPACE;
        case 0x24: return KeyCode::RETURN;
        case 0x35: return KeyCode::ESC;
        
        case 0x7B: return KeyCode::ARROW_LEFT;
        case 0x7C: return KeyCode::ARROW_RIGHT;
        case 0x7D: return KeyCode::ARROW_DOWN;
        case 0x7E: return KeyCode::ARROW_UP;
        
        case 0x7A: return KeyCode::KEY_F1;
        case 0x78: return KeyCode::KEY_F2;
        case 0x63: return KeyCode::KEY_F3;
        case 0x76: return KeyCode::KEY_F4;
        case 0x60: return KeyCode::KEY_F5;
        case 0x61: return KeyCode::KEY_F6;
        case 0x62: return KeyCode::KEY_F7;
        case 0x64: return KeyCode::KEY_F8;
        case 0x65: return KeyCode::KEY_F9;
        case 0x6D: return KeyCode::KEY_F10;
        case 0x67: return KeyCode::KEY_F11;
        case 0x6F: return KeyCode::KEY_F12;
        
        case 0x38: return KeyCode::LEFT_SHIFT;
        case 0x3C: return KeyCode::RIGHT_SHIFT;
        case 0x3B: return KeyCode::LEFT_CTRL;
        case 0x3E: return KeyCode::RIGHT_CTRL;
        case 0x3A: return KeyCode::LEFT_ALT;
        case 0x3D: return KeyCode::RIGHT_ALT;
        
        case 0x39: return KeyCode::CAPSLOCK;
        
        default: return KeyCode::UNKNOWN;
    }
}

int CocoaAppContext::PollEvent(SystemEvent& event)
{
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
        platformEvent.cocoaEvent.nsEvent = (__bridge_retained void*)nsEvent;

        HYP_LOG(Core, Debug, "CocoaAppContext received event of type: {}", (int)[nsEvent type]);
        
        switch ([nsEvent type])
        {
        case NSEventTypeKeyDown:
            event = SystemEvent(SystemEventType::EVENT_KEYDOWN, platformEvent);
            event.GetEventData().Set(MapCocoaKeyCodeToKeyCode([nsEvent keyCode]));
            return 1;
            
        case NSEventTypeKeyUp:
            event = SystemEvent(SystemEventType::EVENT_KEYUP, platformEvent);
            event.GetEventData().Set(MapCocoaKeyCodeToKeyCode([nsEvent keyCode]));
            return 1;
            
        case NSEventTypeMouseMoved:
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDragged:
        case NSEventTypeOtherMouseDragged:
        {
            event = SystemEvent(SystemEventType::EVENT_MOUSEMOTION, platformEvent);
            
            // Check if mouse is locked
            bool isMouseLocked = false;
            CocoaApplicationWindow* cocoaWindow = nullptr;
            
            if (m_mainWindow)
            {
                cocoaWindow = dynamic_cast<CocoaApplicationWindow*>(m_mainWindow.Get());
                if (cocoaWindow)
                {
                    isMouseLocked = cocoaWindow->IsMouseLocked();
                }
            }
            
            if (isMouseLocked)
            {
                // When mouse is locked, use delta movement for infinite motion
                CGFloat deltaX = [nsEvent deltaX];
                CGFloat deltaY = [nsEvent deltaY];
                
                HYP_LOG(Core, Debug, "Mouse locked - delta: ({}, {})", deltaX, deltaY);
                
                event.GetEventData().Set(Vec2i((int)deltaX, (int)deltaY));
            }
            else
            {
                // When mouse is free, use absolute position
                NSWindow* window = [nsEvent window];
                
                if (!window && cocoaWindow)
                {
                    window = (__bridge NSWindow*)cocoaWindow->GetNSWindow();
                }
                
                NSPoint location = [nsEvent locationInWindow];
                
                HYP_LOG(Core, Debug, "Mouse free - location in window: ({}, {})", location.x, location.y);
                
                // Flip Y coordinate (Cocoa has origin at bottom-left)
                if (window)
                {
                    NSRect frame = [window.contentView frame];
                    location.y = frame.size.height - location.y;
                }
                
                event.GetEventData().Set(Vec2i((int)location.x, (int)location.y));
            }
            
            return 1;
        }
            
        case NSEventTypeLeftMouseDown:
            event = SystemEvent(SystemEventType::EVENT_MOUSEBUTTON_DOWN, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));
            return 1;
            
        case NSEventTypeLeftMouseUp:
            event = SystemEvent(SystemEventType::EVENT_MOUSEBUTTON_UP, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));
            return 1;
            
        case NSEventTypeRightMouseDown:
            event = SystemEvent(SystemEventType::EVENT_MOUSEBUTTON_DOWN, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));
            return 1;
            
        case NSEventTypeRightMouseUp:
            event = SystemEvent(SystemEventType::EVENT_MOUSEBUTTON_UP, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));
            return 1;
            
        case NSEventTypeOtherMouseDown:
            event = SystemEvent(SystemEventType::EVENT_MOUSEBUTTON_DOWN, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));
            return 1;
            
        case NSEventTypeOtherMouseUp:
            event = SystemEvent(SystemEventType::EVENT_MOUSEBUTTON_UP, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));
            return 1;
            
        case NSEventTypeScrollWheel:
        {
            event = SystemEvent(SystemEventType::EVENT_MOUSESCROLL, platformEvent);
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
            return 1;
        }
            
        default:
            break;
        }
    }
    
    return 0;
}

} // namespace sys
} // namespace hyperion
