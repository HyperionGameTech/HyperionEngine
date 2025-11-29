/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/CAMetalLayer.h>

#include <system/AppContext.hpp>

#include <core/debug/Debug.hpp>
#include <core/logging/Logger.hpp>

using namespace hyperion;

#pragma mark - HyperionMetalView

@interface HyperionMetalView : NSView
@property (nonatomic, assign) CocoaApplicationWindow* hyperionWindow;
@property (nonatomic, strong) NSTrackingArea* trackingArea;
@end

@implementation HyperionMetalView

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
    
    if (_hyperionWindow)
    {
        _hyperionWindow->HandleResize(Vec2i((int)newSize.width, (int)newSize.height));
    }
    
    // Update metal layer drawable size
    CAMetalLayer* metalLayer = (CAMetalLayer*)self.layer;
    if (metalLayer)
    {
        CGFloat scale = self.window ? self.window.backingScaleFactor : 1.0;
        metalLayer.contentsScale = scale;
        metalLayer.drawableSize = CGSizeMake(newSize.width * scale, newSize.height * scale);
    }
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    
    // Update content scale when view moves to a window
    if (self.window)
    {
        CAMetalLayer* metalLayer = (CAMetalLayer*)self.layer;
        if (metalLayer)
        {
            CGFloat scale = self.window.backingScaleFactor;
            metalLayer.contentsScale = scale;
            metalLayer.drawableSize = CGSizeMake(self.bounds.size.width * scale, 
                                                  self.bounds.size.height * scale);
        }
    }
}

@end

#pragma mark - HyperionWindowDelegate

@interface HyperionWindowDelegate : NSObject<NSWindowDelegate>
@property (nonatomic, assign) CocoaApplicationWindow* window;
@end

@implementation HyperionWindowDelegate

- (void)windowDidResize:(NSNotification *)notification
{
    if (_window)
    {
        NSWindow* nsWindow = [notification object];
        NSRect frame = [nsWindow.contentView frame];
        _window->HandleResize(Vec2i((int)frame.size.width, (int)frame.size.height));
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
      m_isEmbeddedView(false)
{
}

CocoaApplicationWindow::~CocoaApplicationWindow()
{
    if (m_metalLayer)
    {
        CAMetalLayer* metalLayer = (CAMetalLayer*)m_metalLayer;
        [metalLayer release];
        m_metalLayer = nullptr;
    }
    
    if (m_nsView)
    {
        HyperionMetalView* view = (HyperionMetalView*)m_nsView;
        view.hyperionWindow = nullptr;

        if (m_isEmbeddedView)
        {
            [view removeFromSuperview];
            [view release];
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

void CocoaApplicationWindow::Initialize(WindowOptions windowOptions, HWND parentHwnd)
{
    m_title = windowOptions.title;
    m_size = windowOptions.dimensions;

    // If parentHwnd is provided, create an embedded view instead of a standalone window
    if (parentHwnd != nullptr)
    {
        m_isEmbeddedView = true;
        
        // parentHwnd can be either an NSWindow* or an NSView*
        // We'll try to handle both cases
        id parentObject = (id)parentHwnd;
        NSView* parentView = nil;
        NSWindow* parentWindow = nil;
        
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
            HYP_LOG(Core, Error, "CocoaApplicationWindow: parentHwnd is not an NSWindow or NSView");
            return;
        }
        
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
        
        m_nsView = (void*)metalView;
        m_metalLayer = (void*)[metalLayer retain];
        m_hwnd = (void*)parentWindow; // Store reference to parent window for coordinate conversions
        
        HYP_LOG(Core, Debug, "CocoaApplicationWindow initialized as embedded view: {} ({}x{})", 
                m_title, m_size.x, m_size.y);
        
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
        
        
        m_metalLayer = (void*)metalLayer;
    }
    
    m_hwnd = (void*)window;
    m_windowDelegate = (void*)delegate;

    if (!(windowOptions.flags & uint32(WindowFlags::HEADLESS)))
    {
        [window makeKeyAndOrderFront:nil];
    }
    
    HYP_LOG(Core, Debug, "CocoaApplicationWindow initialized: {} ({}x{})", 
            m_title, m_size.x, m_size.y);
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

void* CocoaApplicationWindow::GetCAMetalLayer() const
{
    return m_metalLayer;
}

} // namespace sys
} // namespace hyperion
