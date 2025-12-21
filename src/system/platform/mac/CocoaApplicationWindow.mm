/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/CAMetalLayer.h>

#include <SystemPch.hpp>

#include <system/AppContext.hpp>
#include <system/SystemEvent.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/debug/Debug.hpp>

#include <input/InputManager.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/Device.hpp>

#ifdef HYP_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>

#include <rendering/vulkan/VulkanSwapchain.hpp>
#endif

using namespace hyperion;

namespace hyperion {
namespace sys {
KeyCode MapCocoaKeyCodeToKeyCode(unsigned short keyCode);
} // namespace sys
} // namespace hyperion

#pragma mark - HyperionMetalView

@interface HyperionMetalView : NSView
@property (nonatomic, assign) CocoaApplicationWindow* hyperionWindow;
@property (nonatomic, strong) NSTrackingArea* trackingArea;
@end

@implementation HyperionMetalView

+ (objc_class*)layerClass
{
    return (objc_class*)[CAMetalLayer class];
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self)
    {
        self.wantsLayer = YES;
        self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;

        // Setup tracking area for mouse events
        [self updateTrackingAreas];
    }
    return self;
}

- (void)dealloc
{
    if (_trackingArea)
    {
        [self removeTrackingArea:_trackingArea];
        [_trackingArea release];
        _trackingArea = nil;
    }
    [super dealloc];
}

- (BOOL)wantsUpdateLayer
{
    return YES;
}

- (CALayer*)makeBackingLayer
{
    CAMetalLayer* metalLayer = [CAMetalLayer layer];
    return metalLayer;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)canBecomeKeyView
{
    return YES;
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    
    if (_trackingArea)
    {
        [self removeTrackingArea:_trackingArea];
        [_trackingArea release];
    }
    
    NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited |
                                    NSTrackingMouseMoved |
                                    NSTrackingActiveInKeyWindow |
                                    NSTrackingInVisibleRect;
    
    _trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds
                                                 options:options
                                                   owner:self
                                                userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];

    CGFloat scale = self.window ? self.window.backingScaleFactor : [NSScreen mainScreen].backingScaleFactor;
    CGSize drawableSize = CGSizeMake(newSize.width * scale, newSize.height * scale);
    
    // Update metal layer drawable size
    CAMetalLayer* metalLayer = (CAMetalLayer*)self.layer;
    if (metalLayer)
    {
        metalLayer.contentsScale = scale;
        metalLayer.drawableSize = drawableSize;
    }
    
    if (_hyperionWindow)
    {
        const int width = int(drawableSize.width);
        const int height = int(drawableSize.height);

        _hyperionWindow->HandleResize(Vec2i(int(newSize.width), int(newSize.height)));
    }
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    
    if (self.window)
    {
        CAMetalLayer* metalLayer = (CAMetalLayer*)self.layer;
        if (metalLayer)
        {
            CGFloat scale = self.window.backingScaleFactor;
            metalLayer.contentsScale = scale;
            metalLayer.drawableSize = CGSizeMake(
                self.bounds.size.width * scale, 
                self.bounds.size.height * scale);
        }
    }
}

#define HANDLE_COCOA_EVENT(method)                                                          \
    - (void)method:(NSEvent *)event                                                         \
    {                                                                                       \
        if (_hyperionWindow && _hyperionWindow->UseCocoaEvents())                           \
        {                                                                                   \
            SystemEvent systemEvent;                                                        \
            if (_hyperionWindow->HandleNSEvent(event, systemEvent))                         \
            {                                                                               \
                _hyperionWindow->GetInputManager()->ProcessEvent(std::move(systemEvent));   \
            }                                                                               \
        }                                                                                   \
    }

HANDLE_COCOA_EVENT(mouseMoved)
HANDLE_COCOA_EVENT(mouseDown)
HANDLE_COCOA_EVENT(mouseUp)
HANDLE_COCOA_EVENT(mouseDragged)
HANDLE_COCOA_EVENT(rightMouseDown)
HANDLE_COCOA_EVENT(rightMouseUp)
HANDLE_COCOA_EVENT(rightMouseDragged)
HANDLE_COCOA_EVENT(scrollWheel)
HANDLE_COCOA_EVENT(keyDown)
HANDLE_COCOA_EVENT(keyUp)

#undef HANDLE_COCOA_EVENT

@end

#pragma mark - HyperionWindowDelegate

@interface HyperionWindowDelegate : NSObject<NSWindowDelegate>
@property (nonatomic, assign) CocoaApplicationWindow* window;
@end

@implementation HyperionWindowDelegate

- (NSSize)windowWillResize:(NSWindow*)sender toSize:(NSSize)frameSize
{
    return frameSize;
}

- (void)windowDidResize:(NSNotification *)notification
{
    if (_window)
    {
        NSWindow* nsWindow = [notification object];
        NSRect frame = [nsWindow.contentView frame];
        
        int width = (int)frame.size.width;
        int height = (int)frame.size.height;

        _window->HandleResize(Vec2i(width, height));
    }
}

- (void)windowWillClose:(NSNotification *)notification
{
    // Handle window close event
}

- (BOOL)windowShouldClose:(NSWindow *)sender
{
    // Allow the window to close
    return YES;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    // Window gained focus
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    // Window lost focus
}

@end

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

namespace sys {

CocoaApplicationWindow::CocoaApplicationWindow(ANSIString title, Vec2i size)
    : ApplicationWindow(std::move(title), size),
      m_windowDelegate(nullptr),
      m_metalLayer(nullptr),
      m_nsView(nullptr),
      m_mouseLocked(false),
      m_isEmbeddedView(false),
      m_useCocoaEvents(false)
{
}

CocoaApplicationWindow::~CocoaApplicationWindow()
{
    if (m_metalLayer)
    {
        [(id)m_metalLayer release];
        m_metalLayer = nullptr;
    }
    
    if (m_nsView)
    {
        // Only treat m_nsView as a HyperionMetalView if it actually is one.
        // For standalone windows m_nsView is the plain contentView (NSView),
        // and attempting to call HyperionMetalView-only selectors on it
        // causes an unrecognized selector crash (seen in logs).
        if ([(id)m_nsView isKindOfClass:[HyperionMetalView class]])
        {
            HyperionMetalView* view = (HyperionMetalView*)m_nsView;
            view.hyperionWindow = nullptr;

            if (m_isEmbeddedView)
            {
                [view removeFromSuperview];
                [view release];
            }
        }

        m_nsView = nullptr;
    }
    
    if (m_windowDelegate)
    {
        HyperionWindowDelegate* delegate = (HyperionWindowDelegate*)m_windowDelegate;
        delegate.window = nullptr;
        [delegate release];

        m_windowDelegate = nullptr;
    }
    
    // Only close/release window if we created it (not embedded)
    if (m_hwnd && !m_isEmbeddedView)
    {
        NSWindow* window = (NSWindow*)m_hwnd;
        [window close];
        [window release];

        m_hwnd = nullptr;
    }
    else
    {
        m_hwnd = nullptr;
    }
}

void CocoaApplicationWindow::Initialize(WindowOptions windowOptions)
{
    AssertOnThread(g_mainThread);
  
    m_title = windowOptions.title;
    m_size = windowOptions.dimensions;
    m_useCocoaEvents = (windowOptions.flags & uint32(WindowFlags::EVENTS_POLLING)) == 0;

    // If parentHwnd is provided, create an embedded view instead of a standalone window
    if (windowOptions.parentHwnd != nullptr)
    {
        m_isEmbeddedView = true;
        
        id parentObject = (id)windowOptions.parentHwnd;
        NSView* parentView = nil;
        NSWindow* parentWindow = nil;
        
        // handle both NSWindow and NSView as parentHwnd
        if ([parentObject isKindOfClass:[NSWindow class]])
        {
            parentWindow = (NSWindow*)parentObject;
            parentView = [parentWindow contentView];
        }
        else if ([parentObject isKindOfClass:[NSView class]])
        {
            parentView = (NSView*)parentObject;
            parentWindow = [parentView window];
        }
        else
        {
            String className = [[[parentObject class] description] UTF8String];
            HYP_FAIL("CocoaApplicationWindow: parentHwnd is not an NSWindow or NSView! Got: {}", className);
            return;
        }

        AssertDebug(parentView != nil, "Parent NSWindow contentView is null in CocoaApplicationWindow embedded view creation");
        
        // Create the metal view
        NSRect frame = NSMakeRect(0, 0, m_size.x, m_size.y);
        
        HyperionMetalView* metalView = [[HyperionMetalView alloc] initWithFrame:frame];
        metalView.hyperionWindow = this;
        
        // Configure autoresizing to fill parent
        metalView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        
        // Add to parent view
        [parentView addSubview:metalView];
        
        // Get the metal layer from the view
        CAMetalLayer* metalLayer = (CAMetalLayer*)[metalView layer];
        metalLayer.presentsWithTransaction = NO;
        
        if (windowOptions.flags & uint32(WindowFlags::HIGH_DPI))
        {
            metalLayer.contentsScale = parentWindow ? [parentWindow backingScaleFactor] : 2.0;
        }
        else
        {
            metalLayer.contentsScale = 1.0;
        }
        
        metalLayer.drawableSize = CGSizeMake(
            frame.size.width * metalLayer.contentsScale,
            frame.size.height * metalLayer.contentsScale
        );
        
        m_nsView = metalView;
        m_metalLayer = [metalLayer retain];
        m_hwnd = parentWindow; // Store reference to parent window for coordinate conversions

        // trigger initial resize handling
        [metalView setFrameSize:frame.size];

        return;
    }

    // Standard standalone window creation
    NSRect frame = NSMakeRect(0, 0, m_size.x, m_size.y);
    
    NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | 
                                    NSWindowStyleMaskClosable | 
                                    NSWindowStyleMaskMiniaturizable | 
                                    NSWindowStyleMaskResizable;
    
    if (windowOptions.flags & uint32(WindowFlags::HEADLESS))
    {
        // Create window but don't show it
    }
    
    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                    styleMask:styleMask
                                                        backing:NSBackingStoreBuffered
                                                        defer:NO];
    
    m_nsView = [window contentView];
    AssertDebug(m_nsView != nullptr);

    [window setTitle:[NSString stringWithUTF8String:m_title.Data()]];
    [window center];

    
    // Setup delegate
    HyperionWindowDelegate* delegate = [[HyperionWindowDelegate alloc] init];
    delegate.window = this;
    [window setDelegate:delegate];
    
    // Accept mouse moved events
    [window setAcceptsMouseMovedEvents:YES];
    
    // Register for drag-and-drop
    [window registerForDraggedTypes:@[NSPasteboardTypeFileURL]];
    
    // Create Metal layer
    if (!(windowOptions.flags & uint32(WindowFlags::NO_GFX)))
    {
        CAMetalLayer* metalLayer = [CAMetalLayer layer];
        [window.contentView setLayer:metalLayer];
        [window.contentView setWantsLayer:YES];
        
        if (windowOptions.flags & uint32(WindowFlags::HIGH_DPI))
        {
            metalLayer.contentsScale = [window backingScaleFactor];
        }
        else
        {
            metalLayer.contentsScale = 1.0;
        }
        
        metalLayer.drawableSize = CGSizeMake(
            [window.contentView bounds].size.width * metalLayer.contentsScale,
            [window.contentView bounds].size.height * metalLayer.contentsScale
        );
        
        m_metalLayer = [metalLayer retain];
    }
    
    m_hwnd = window;
    m_windowDelegate = delegate;

    if (!(windowOptions.flags & uint32(WindowFlags::HEADLESS)))
    {
        [window makeKeyAndOrderFront:nil];
    }
}

bool CocoaApplicationWindow::HandleNSEvent(NSEvent* nsEvent, SystemEvent& event)
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    event = SystemEvent();
    
    @autoreleasepool
    {
        PlatformEvent platformEvent {};
        platformEvent.cocoaEvent.nsEvent = (void*)[nsEvent retain]; // keep it around, we release it manually in DestroyCocoaEvent()

        switch ([nsEvent type])
        {
        case NSEventTypeKeyDown:
            event = SystemEvent(SystemEvent::KEYDOWN, this, platformEvent);
            event.GetEventData().Set(MapCocoaKeyCodeToKeyCode([nsEvent keyCode]));
            return true;
            
        case NSEventTypeKeyUp:
            event = SystemEvent(SystemEvent::KEYUP, this, platformEvent);
            event.GetEventData().Set(MapCocoaKeyCodeToKeyCode([nsEvent keyCode]));
            return true;
            
        case NSEventTypeMouseMoved:
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDragged:
        case NSEventTypeOtherMouseDragged:
        {
            event = SystemEvent(SystemEvent::MOUSEMOTION, this, platformEvent);
            
            bool isMouseLocked = IsMouseLocked();

            HYP_LOG_TEMP("Mouse moved event received, position: ({}, {}), isMouseLocked: {}", [nsEvent locationInWindow].x, [nsEvent locationInWindow].y, isMouseLocked);
            
            if (isMouseLocked)
            {
                CGFloat deltaX = [nsEvent deltaX];
                CGFloat deltaY = [nsEvent deltaY];
                
                Vec2i newPos = GetMousePosition() + Vec2i((int)deltaX, (int)deltaY);
                
                event.GetEventData().Set(newPos);
            }
            else
            {
                NSPoint location = [nsEvent locationInWindow];

                if (m_isEmbeddedView && m_nsView)
                {
                    // Convert to view coordinates
                    HyperionMetalView* view = (HyperionMetalView*)m_nsView;
                    location = [view convertPoint:location fromView:nil];
                    
                    // Flip Y coordinate
                    NSRect viewFrame = [view frame];
                    location.y = viewFrame.size.height - location.y;
                }
                else
                {
                    // Flip Y coordinate for window content view
                    NSWindow* window = (NSWindow*)m_hwnd;
                    NSRect contentFrame = [window.contentView frame];
                    location.y = contentFrame.size.height - location.y;
                }

                event.GetEventData().Set(Vec2i((int)location.x, (int)location.y));
            }
            
            return true;
        }
            
        case NSEventTypeLeftMouseDown:
            event = SystemEvent(SystemEvent::MOUSEBUTTON_DOWN, this, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));
            return true;
            
        case NSEventTypeLeftMouseUp:
            event = SystemEvent(SystemEvent::MOUSEBUTTON_UP, this, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::LEFT));
            return true;
            
        case NSEventTypeRightMouseDown:
            event = SystemEvent(SystemEvent::MOUSEBUTTON_DOWN, this, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));
            return true;
            
        case NSEventTypeRightMouseUp:
            event = SystemEvent(SystemEvent::MOUSEBUTTON_UP, this, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::RIGHT));
            return true;
            
        case NSEventTypeOtherMouseDown:
            event = SystemEvent(SystemEvent::MOUSEBUTTON_DOWN, this, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));
            return true;
            
        case NSEventTypeOtherMouseUp:
            event = SystemEvent(SystemEvent::MOUSEBUTTON_UP, this, platformEvent);
            event.GetEventData().Set(EnumFlags<MouseButtonState>(MouseButtonState::MIDDLE));
            return true;
            
        case NSEventTypeScrollWheel:
        {
            event = SystemEvent(SystemEvent::MOUSESCROLL, this, platformEvent);
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
            return true;
        }
            
        default:
            break;
        }
    }

    return false;
}

void CocoaApplicationWindow::SetMousePosition(Vec2i position)
{
    if (!m_mouseLocked)
    {
        if (m_isEmbeddedView && m_nsView)
        {
            HyperionMetalView* view = (HyperionMetalView*)m_nsView;
            NSWindow* window = [view window];
            if (window)
            {
                NSRect viewFrame = [view frame];
                NSPoint viewPoint = NSMakePoint(position.x, viewFrame.size.height - position.y);
                NSPoint windowPoint = [view convertPoint:viewPoint toView:nil];
                NSPoint screenPoint = [window convertPointToScreen:windowPoint];
                
                CGPoint point = CGPointMake(screenPoint.x, CGDisplayBounds(CGMainDisplayID()).size.height - screenPoint.y);
                CGWarpMouseCursorPosition(point);
                CGAssociateMouseAndMouseCursorPosition(true);
            }
        }
        else
        {
            NSWindow* window = (NSWindow*)m_hwnd;
            NSRect windowFrame = [window frame];
            NSRect contentFrame = [window.contentView frame];
            
            // Convert from content coordinates to screen coordinates
            CGFloat screenY = windowFrame.origin.y + (contentFrame.size.height - position.y);
            CGFloat screenX = windowFrame.origin.x + position.x;
            
            CGPoint point = CGPointMake(screenX, screenY);
            CGWarpMouseCursorPosition(point);
            CGAssociateMouseAndMouseCursorPosition(true);
        }
    }

    m_mousePosition = position;
}

Vec2i CocoaApplicationWindow::GetMousePosition() const
{
    if (!m_mouseLocked)
    {
        if (m_isEmbeddedView && m_nsView)
        {
            HyperionMetalView* view = (HyperionMetalView*)m_nsView;
            NSWindow* window = [view window];
            if (window)
            {
                NSPoint mouseLocation = [NSEvent mouseLocation];
                NSPoint windowPoint = [window convertPointFromScreen:mouseLocation];
                NSPoint viewPoint = [view convertPoint:windowPoint fromView:nil];
                
                NSRect viewFrame = [view frame];
                m_mousePosition = Vec2i((int)viewPoint.x, (int)(viewFrame.size.height - viewPoint.y));
            }
        }
        else
        {
            NSWindow* window = (NSWindow*)m_hwnd;
            NSPoint mouseLocation = [NSEvent mouseLocation];
            NSRect windowFrame = [window frame];
            NSRect contentFrame = [window.contentView frame];
            
            // Convert from screen coordinates to content coordinates
            CGFloat localX = mouseLocation.x - windowFrame.origin.x;
            CGFloat localY = contentFrame.size.height - (mouseLocation.y - windowFrame.origin.y);

            m_mousePosition = Vec2i((int)localX, (int)localY);
        }
    }
    
    return m_mousePosition;
}

Vec2i CocoaApplicationWindow::GetDimensions() const
{
    if (m_isEmbeddedView && m_nsView)
    {
        HyperionMetalView* view = (HyperionMetalView*)m_nsView;
        NSRect frame = [view frame];
        
        return Vec2i((int)frame.size.width, (int)frame.size.height);
    }
    
    NSWindow* window = (NSWindow*)m_hwnd;
    NSRect frame = [window.contentView frame];
    return Vec2i((int)frame.size.width, (int)frame.size.height);
}

void CocoaApplicationWindow::SetIsMouseLocked(bool locked)
{
    m_mouseLocked = locked;
    
    if (locked)
    {
        CGAssociateMouseAndMouseCursorPosition(false);
        [NSCursor hide];
    }
    else
    {
        CGAssociateMouseAndMouseCursorPosition(true);
        [NSCursor unhide];
    }
}

bool CocoaApplicationWindow::HasMouseFocus() const
{
    if (m_isEmbeddedView && m_nsView)
    {
        HyperionMetalView* view = (HyperionMetalView*)m_nsView;
        NSWindow* window = [view window];
        if (!window || ![window isKeyWindow])
        {
            return false;
        }
        
        // Check if mouse is within our view
        NSPoint mouseLocation = [NSEvent mouseLocation];
        NSRect screenFrame = [view convertRect:[view bounds] toView:nil];
        screenFrame = [window convertRectToScreen:screenFrame];
        return NSPointInRect(mouseLocation, screenFrame);
    }
    
    NSWindow* window = (NSWindow*)m_hwnd;
    return [window isKeyWindow];
}

bool CocoaApplicationWindow::IsHighDPI() const
{
    if (m_isEmbeddedView && m_nsView)
    {
        HyperionMetalView* view = (HyperionMetalView*)m_nsView;
        NSWindow* window = [view window];
        return window ? [window backingScaleFactor] > 1.0 : true; // Assume high DPI if no window
    }
    
    NSWindow* window = (NSWindow*)m_hwnd;
    return [window backingScaleFactor] > 1.0;
}

} // namespace sys
} // namespace hyperion
