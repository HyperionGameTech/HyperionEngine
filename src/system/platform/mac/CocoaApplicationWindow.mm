/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/CAMetalLayer.h>

#include <system/AppContext.hpp>

#include <core/debug/Debug.hpp>
#include <core/logging/Logger.hpp>

using namespace hyperion;

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
      m_nsWindow(nullptr),
      m_windowDelegate(nullptr),
      m_metalLayer(nullptr),
      m_mouseLocked(false)
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
    
    if (m_windowDelegate)
    {
        HyperionWindowDelegate* delegate = (HyperionWindowDelegate*)m_windowDelegate;
        delegate.window = nullptr;
        [delegate release];

        m_windowDelegate = nullptr;
    }
    
    if (m_nsWindow)
    {
        NSWindow* window = (NSWindow*)m_nsWindow;
        [window close];
        [window release];

        m_nsWindow = nullptr;
    }
}

void CocoaApplicationWindow::Initialize(WindowOptions windowOptions)
{
    m_title = windowOptions.title;
    m_size = windowOptions.size;

    NSRect frame = NSMakeRect(0, 0, m_size.x, m_size.y);
    
    NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | 
                                    NSWindowStyleMaskClosable | 
                                    NSWindowStyleMaskMiniaturizable | 
                                    NSWindowStyleMaskResizable;
    
    if (windowOptions.flags & WindowFlags::HEADLESS)
    {
        // Create window but don't show it
    }
    
    NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                    styleMask:styleMask
                                                        backing:NSBackingStoreBuffered
                                                        defer:NO];
    
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
    if (!(windowOptions.flags & WindowFlags::NO_GFX))
    {
        CAMetalLayer* metalLayer = [CAMetalLayer layer];
        [window.contentView setLayer:metalLayer];
        [window.contentView setWantsLayer:YES];
        
        if (windowOptions.flags & WindowFlags::HIGH_DPI)
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
    
    if (!(windowOptions.flags & WindowFlags::HEADLESS))
    {
        [window makeKeyAndOrderFront:nil];
    }
    
    m_nsWindow = (void*)window;
    m_windowDelegate = (void*)delegate;
    
    HYP_LOG(Core, Debug, "CocoaApplicationWindow initialized: {} ({}x{})", 
            m_title, m_size.x, m_size.y);
}

void CocoaApplicationWindow::SetMousePosition(Vec2i position)
{
    HYP_LOG(Core, Debug, "Setting mouse position to ({}, {})", position.x, position.y);

    if (!m_mouseLocked)
    {
        NSWindow* window = (NSWindow*)m_nsWindow;
        NSRect windowFrame = [window frame];
        NSRect contentFrame = [window.contentView frame];
        
        // Convert from content coordinates to screen coordinates
        CGFloat screenY = windowFrame.origin.y + (contentFrame.size.height - position.y);
        CGFloat screenX = windowFrame.origin.x + position.x;
        
        CGPoint point = CGPointMake(screenX, screenY);
        CGWarpMouseCursorPosition(point);
        CGAssociateMouseAndMouseCursorPosition(true);
    }

    m_mousePosition = position;
}

Vec2i CocoaApplicationWindow::GetMousePosition() const
{
    if (!m_mouseLocked)
    {
        NSWindow* window = (NSWindow*)m_nsWindow;
        NSPoint mouseLocation = [NSEvent mouseLocation];
        NSRect windowFrame = [window frame];
        NSRect contentFrame = [window.contentView frame];
        
        // Convert from screen coordinates to content coordinates
        CGFloat localX = mouseLocation.x - windowFrame.origin.x;
        CGFloat localY = contentFrame.size.height - (mouseLocation.y - windowFrame.origin.y);

        m_mousePosition = Vec2i((int)localX, (int)localY);
    }
    
    return m_mousePosition;
}

Vec2i CocoaApplicationWindow::GetDimensions() const
{
    NSWindow* window = (NSWindow*)m_nsWindow;
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
    NSWindow* window = (NSWindow*)m_nsWindow;
    return [window isKeyWindow];
}

bool CocoaApplicationWindow::IsHighDPI() const
{
    NSWindow* window = (NSWindow*)m_nsWindow;
    return [window backingScaleFactor] > 1.0;
}

void* CocoaApplicationWindow::GetCAMetalLayer() const
{
    return m_metalLayer;
}

} // namespace sys
} // namespace hyperion
