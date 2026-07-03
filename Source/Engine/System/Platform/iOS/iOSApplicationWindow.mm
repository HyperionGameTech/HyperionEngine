/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include <SystemPch.hpp>

#include <System/AppContext.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Scheduler.hpp>

#include <Core/Debug/Debug.hpp>

#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <Rendering/RenderBackend.hpp>
#include <Rendering/Device.hpp>

#if HYP_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>

#include <Rendering/Vulkan/VulkanInstance.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#include <Rendering/Vulkan/VulkanSwapchain.hpp>
#endif

using namespace Hyperion;

namespace Hyperion {
namespace PlatformUtils {

KeyCode MapiOSKeyCodeToKeyCode(unsigned short keyCode);
bool IsHardwareKeyboardAvailable();

} // namespace PlatformUtils
} // namespace Hyperion

#pragma mark - HyperionMetalView

@interface HyperionMetalView : UIView
@property (nonatomic, assign) iOSApplicationWindow* hyperionWindow;
@end

@implementation HyperionMetalView

+ (objc_class*)layerClass
{
    return (objc_class*)[CAMetalLayer class];
}

- (instancetype)initWithFrame:(CGRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self)
    {
        self.multipleTouchEnabled = YES;
        self.userInteractionEnabled = YES;
        
        CAMetalLayer* metalLayer = (CAMetalLayer*)self.layer;
        metalLayer.presentsWithTransaction = NO;
        metalLayer.contentsScale = [UIScreen mainScreen].nativeScale;
    }
    return self;
}

- (void)dealloc
{
    [super dealloc];
}

- (void)setFrame:(CGRect)frame
{
    [super setFrame:frame];

    CAMetalLayer* metalLayer = (CAMetalLayer*)self.layer;
    Assert(metalLayer != nil);
    
    CGFloat scale = metalLayer.contentsScale;
    
    if (scale <= 0.0f)
    {
        scale = [UIScreen mainScreen].nativeScale;
        metalLayer.contentsScale = scale;
    }

    Assert(frame.size.width * frame.size.height > 0 && scale > 0);

    metalLayer.drawableSize = CGSizeMake(frame.size.width * scale, frame.size.height * scale);

    if (_hyperionWindow)
    {
        _hyperionWindow->HandleResize(Vec2i(int(frame.size.width * scale), int(frame.size.height * scale)));
    }
}

- (void)layoutSubviews
{
    [super layoutSubviews];

    CAMetalLayer* metalLayer = (CAMetalLayer*)self.layer;
    Assert(metalLayer != nil);
    
    CGFloat scale = metalLayer.contentsScale;

    if (scale <= 0.0f)
    {
        scale = [UIScreen mainScreen].nativeScale;
        metalLayer.contentsScale = scale;
    }

    Assert(self.bounds.size.width * self.bounds.size.height > 0 && scale > 0);


    metalLayer.drawableSize = CGSizeMake(self.bounds.size.width * scale, self.bounds.size.height * scale);

    if (_hyperionWindow)
    {
        _hyperionWindow->HandleResize(Vec2i(int(self.bounds.size.width * scale),
                                             int(self.bounds.size.height * scale)));
    }
}

- (void)setContentScaleFactor:(CGFloat)contentScaleFactor
{
    [super setContentScaleFactor:contentScaleFactor];

    CAMetalLayer* metalLayer = (CAMetalLayer*)self.layer;
    metalLayer.contentsScale = contentScaleFactor;
}

#pragma mark - Touch Handling

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    if (!_hyperionWindow)
        return;

    for (UITouch* touch in touches)
    {
        Event engineEvent;
        if (_hyperionWindow->HandleTouchEvent((__bridge void*)touch, (__bridge void*)event, engineEvent))
        {
            _hyperionWindow->GetInputManager()->ProcessEvent(std::move(engineEvent));
        }
    }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    if (!_hyperionWindow)
        return;

    for (UITouch* touch in touches)
    {
        Event engineEvent;
        if (_hyperionWindow->HandleTouchEvent((__bridge void*)touch, (__bridge void*)event, engineEvent))
        {
            _hyperionWindow->GetInputManager()->ProcessEvent(std::move(engineEvent));
        }
    }
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    if (!_hyperionWindow)
        return;

    for (UITouch* touch in touches)
    {
        Event engineEvent;
        if (_hyperionWindow->HandleTouchEvent((__bridge void*)touch, (__bridge void*)event, engineEvent))
        {
            _hyperionWindow->GetInputManager()->ProcessEvent(std::move(engineEvent));
        }
    }
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    if (!_hyperionWindow)
        return;

    // Treat cancelled touches as TOUCH_UP
    for (UITouch* touch in touches)
    {
        Event engineEvent;
        if (_hyperionWindow->HandleTouchEvent((__bridge void*)touch, (__bridge void*)event, engineEvent))
        {
            _hyperionWindow->GetInputManager()->ProcessEvent(std::move(engineEvent));
        }
    }
}

#pragma mark - Keyboard Handling (iOS 13.4+)

- (void)pressesBegan:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event
{
    if (!_hyperionWindow)
        return;

    [super pressesBegan:presses withEvent:event];

    for (UIPress* press in presses)
    {
        KeyCode keyCode = PlatformUtils::MapiOSKeyCodeToKeyCode(press.key.keyCode);
        if (keyCode != KeyCode::KEY_UNKNOWN)
        {
            PlatformEvent platformEvent {};
            Event engineEvent(EventType::KEYDOWN, _hyperionWindow, platformEvent);
            engineEvent.GetEventData().Set(keyCode);
            _hyperionWindow->GetInputManager()->ProcessEvent(std::move(engineEvent));
        }
    }
}

- (void)pressesEnded:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event
{
    if (!_hyperionWindow)
        return;

    [super pressesEnded:presses withEvent:event];

    for (UIPress* press in presses)
    {
        KeyCode keyCode = PlatformUtils::MapiOSKeyCodeToKeyCode(press.key.keyCode);
        if (keyCode != KeyCode::KEY_UNKNOWN)
        {
            PlatformEvent platformEvent {};
            Event engineEvent(EventType::KEYUP, _hyperionWindow, platformEvent);
            engineEvent.GetEventData().Set(keyCode);
            _hyperionWindow->GetInputManager()->ProcessEvent(std::move(engineEvent));
        }
    }
}

- (void)pressesCancelled:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event
{
    if (!_hyperionWindow)
        return;

    [super pressesCancelled:presses withEvent:event];

    for (UIPress* press in presses)
    {
        KeyCode keyCode = PlatformUtils::MapiOSKeyCodeToKeyCode(press.key.keyCode);
        if (keyCode != KeyCode::KEY_UNKNOWN)
        {
            PlatformEvent platformEvent {};
            Event engineEvent(EventType::KEYUP, _hyperionWindow, platformEvent);
            engineEvent.GetEventData().Set(keyCode);
            _hyperionWindow->GetInputManager()->ProcessEvent(std::move(engineEvent));
        }
    }
}

@end

#pragma mark - HyperionWindowDelegate

@interface HyperionWindowDelegate : NSObject
@property (nonatomic, assign) iOSApplicationWindow* hyperionWindow;
@end

@implementation HyperionWindowDelegate

- (void)windowDidBecomeKey:(NSNotification*)notification
{
    if (_hyperionWindow)
    {
        PlatformEvent platformEvent {};
        Event event(EventType::WINDOW_FOCUS_GAINED, _hyperionWindow, platformEvent);
        _hyperionWindow->GetInputManager()->ProcessEvent(std::move(event));
    }
}

- (void)windowDidResignKey:(NSNotification*)notification
{
    if (_hyperionWindow)
    {
        PlatformEvent platformEvent {};
        Event event(EventType::WINDOW_FOCUS_LOST, _hyperionWindow, platformEvent);
        _hyperionWindow->GetInputManager()->ProcessEvent(std::move(event));
    }
}

@end

#pragma mark - HyperionViewController

@interface HyperionViewController : UIViewController
@property (nonatomic, assign) iOSApplicationWindow* hyperionWindow;
@end

@implementation HyperionViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.view.multipleTouchEnabled = YES;
}

- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];
}

- (BOOL)prefersStatusBarHidden
{
    return YES;
}

- (BOOL)shouldAutorotate
{
    return YES;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
    return UIInterfaceOrientationMaskAll;
}

@end

namespace Hyperion {

CORE_API HYP_DECLARE_LOG_CHANNEL(Core);

static constexpr int32 IOS_ACTION_DOWN = 0;
static constexpr int32 IOS_ACTION_MOVE = 1;
static constexpr int32 IOS_ACTION_UP = 2;
static constexpr int32 IOS_ACTION_CANCELLED = 3;

iOSApplicationWindow::iOSApplicationWindow(ANSIString title, Vec2i size)
    : ApplicationWindow(std::move(title), size),
      m_metalLayer(nullptr),
      m_uiView(nullptr),
      m_mouseLocked(false)
{
}

iOSApplicationWindow::~iOSApplicationWindow()
{
    if (m_metalLayer)
    {
        [(id)m_metalLayer release];
        m_metalLayer = nullptr;
    }

    if (m_uiView)
    {
        HyperionMetalView* view = (HyperionMetalView*)m_uiView;
        view.hyperionWindow = nullptr;
        [view removeFromSuperview];
        [view release];
        m_uiView = nullptr;
    }

    if (m_hwnd)
    {
        UIWindow* window = (UIWindow*)m_hwnd;
        window.hidden = YES;
        [window release];
        m_hwnd = nullptr;
    }
}

void iOSApplicationWindow::Initialize(WindowOptions windowOptions)
{
    AssertOnThread(g_mainThread);

    TUniqueLock lock(m_mtx);

    m_title = windowOptions.title;
    m_size = windowOptions.dimensions;

    lock.Reset();

    CGRect screenBounds = [UIScreen mainScreen].bounds;
    CGFloat screenScale = [UIScreen mainScreen].nativeScale;

    if (windowOptions.parentHwnd != nullptr)
    {
        id parentObject = (id)windowOptions.parentHwnd;
        UIView* parentView = nil;

        if ([parentObject isKindOfClass:[UIView class]])
        {
            parentView = (UIView*)parentObject;
        }
        else
        {
            String className = [[[parentObject class] description] UTF8String];
            HYP_FAIL("iOSApplicationWindow: parentHwnd is not a UIView! Got: {}", className);
            return;
        }

        CGRect frame = CGRectMake(0, 0, windowOptions.dimensions.x, windowOptions.dimensions.y);

        HyperionMetalView* metalView = [[HyperionMetalView alloc] initWithFrame:frame];
        metalView.hyperionWindow = this;
        metalView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

        [parentView addSubview:metalView];

        Assert(frame.size.width * frame.size.height > 0);

        CAMetalLayer* metalLayer = (CAMetalLayer*)metalView.layer;
        metalLayer.contentsScale = (windowOptions.flags & uint32(WindowFlags::HIGH_DPI))
            ? screenScale
            : 1.0;
        metalLayer.drawableSize = CGSizeMake(
            frame.size.width * metalLayer.contentsScale,
            frame.size.height * metalLayer.contentsScale);

        m_uiView = metalView;
        m_metalLayer = [metalLayer retain];
        m_hwnd = nil; // no standalone window in embedded mode

        // Trigger resize with initial dimensions
        if (frame.size.width > 0.0f && frame.size.height > 0.0f)
        {
            HandleResize(Vec2i(int(frame.size.width * metalLayer.contentsScale),
                               int(frame.size.height * metalLayer.contentsScale)));
        }

        return;
    }

    // Standalone window creation
    CGRect frame = CGRectMake(0, 0,
                              windowOptions.dimensions.x > 0 ? windowOptions.dimensions.x : screenBounds.size.width,
                              windowOptions.dimensions.y > 0 ? windowOptions.dimensions.y : screenBounds.size.height);

    UIWindow* window = [[UIWindow alloc] initWithFrame:frame];
    window.backgroundColor = [UIColor blackColor];

    // Create metal view as the root view
    HyperionMetalView* metalView = [[HyperionMetalView alloc] initWithFrame:window.bounds];
    metalView.hyperionWindow = this;
    metalView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    // Create view controller to manage the view
    HyperionViewController* viewController = [[HyperionViewController alloc] init];
    viewController.hyperionWindow = this;
    viewController.view = metalView;

    window.rootViewController = viewController;
    [viewController release];

    // Setup delegate for focus notifications
    HyperionWindowDelegate* delegate = [[HyperionWindowDelegate alloc] init];
    delegate.hyperionWindow = this;

    [[NSNotificationCenter defaultCenter] addObserver:delegate
                                             selector:@selector(windowDidBecomeKey:)
                                                 name:UIWindowDidBecomeKeyNotification
                                               object:window];
    [[NSNotificationCenter defaultCenter] addObserver:delegate
                                             selector:@selector(windowDidResignKey:)
                                                 name:UIWindowDidResignKeyNotification
                                               object:window];
    Assert(metalView.bounds.size.width * metalView.bounds.size.height > 0);

    CAMetalLayer* metalLayer = (CAMetalLayer*)metalView.layer;
    metalLayer.contentsScale = (windowOptions.flags & uint32(WindowFlags::HIGH_DPI))
        ? screenScale
        : 1.0;
    metalLayer.drawableSize = CGSizeMake(
        metalView.bounds.size.width * metalLayer.contentsScale,
        metalView.bounds.size.height * metalLayer.contentsScale);
    m_metalLayer = [metalLayer retain];

    m_uiView = metalView;
    m_hwnd = [window retain];

    if (!(windowOptions.flags & uint32(WindowFlags::HEADLESS)))
    {
        [window makeKeyAndVisible];
    }

    HandleResize(Vec2i(int(metalView.bounds.size.width * metalLayer.contentsScale),
                       int(metalView.bounds.size.height * metalLayer.contentsScale)));
}

void iOSApplicationWindow::SetNativeWindow(void* nativeWindow)
{
    Vec2i newSize = m_size;

    {
        TUniqueLock lock(m_mtx);

        UIWindow* window = (UIWindow*)nativeWindow;
        if (window == nil)
        {
            m_hwnd = nullptr;
            return;
        }

        m_hwnd = [window retain];
        HyperionMetalView* metalView = nil;

        for (UIView* subview in window.rootViewController.view.subviews)
        {
            if ([subview isKindOfClass:[HyperionMetalView class]])
            {
                metalView = (HyperionMetalView*)subview;
                break;
            }
        }

        if (metalView == nil)
        {
            metalView = [[HyperionMetalView alloc] initWithFrame:window.bounds];
            metalView.hyperionWindow = this;
            metalView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

            if (window.rootViewController)
            {
                window.rootViewController.view = metalView;
            }
            else
            {
                [window addSubview:metalView];
            }

            [metalView release];
        }

        metalView.hyperionWindow = this;

        CAMetalLayer* metalLayer = (CAMetalLayer*)metalView.layer;

        if (m_metalLayer)
        {
            [(id)m_metalLayer release];
        }

        m_metalLayer = [metalLayer retain];
        m_uiView = metalView;

        newSize = Vec2i(int(window.bounds.size.width * metalLayer.contentsScale),
                        int(window.bounds.size.height * metalLayer.contentsScale));
    }

    HandleResize(newSize);
}

bool iOSApplicationWindow::HandleTouchEvent(void* touchPtr, void* eventPtr, Event& outEvent)
{
    UITouch* uiTouch = (__bridge UITouch*)touchPtr;
    UIEvent* uiEvent = (__bridge UIEvent*)eventPtr;

    CGPoint location = [uiTouch locationInView:(UIView*)m_uiView];
    CGFloat scale = m_metalLayer ? ((CAMetalLayer*)m_metalLayer).contentsScale : 1.0;

    const Vec2f currentPos = Vec2f(location.x * scale, location.y * scale);
    const int32 pointerId = (int32)(uintptr_t)uiTouch;

    {
        TUniqueLock lock(m_mtx);
        m_touchPosition = currentPos;
    }

    PlatformEvent platformEvent {};
#ifdef HYP_IOS
    platformEvent.iosEvent.uiEvent = (__bridge void*)[uiEvent retain];
#endif

    switch (uiTouch.phase)
    {
    case UITouchPhaseBegan:
    {
        outEvent = Event(EventType::TOUCH_DOWN, this, platformEvent);

        MotionData motionData { currentPos, Vec2f::Zero(), /* isAbsolute */ false };
        outEvent.GetEventData().Set(TouchEventData { pointerId, motionData });

        if (pointerId >= 0 && pointerId < static_cast<int32>(m_touchPrevPositions.Size()))
        {
            m_touchPrevPositions[pointerId] = currentPos;
        }

        return true;
    }

    case UITouchPhaseMoved:
    {
        outEvent = Event(EventType::TOUCH_MOVE, this, platformEvent);

        Vec2f prevPos = currentPos;
        if (pointerId >= 0 && pointerId < static_cast<int32>(m_touchPrevPositions.Size()))
        {
            prevPos = m_touchPrevPositions[pointerId];
        }

        Vec2f delta = currentPos - prevPos;
        MotionData motionData { currentPos, delta, /* isAbsolute */ false };
        outEvent.GetEventData().Set(TouchEventData { pointerId, motionData });

        if (pointerId >= 0 && pointerId < static_cast<int32>(m_touchPrevPositions.Size()))
        {
            m_touchPrevPositions[pointerId] = currentPos;
        }

        return true;
    }

    case UITouchPhaseEnded:
    case UITouchPhaseCancelled:
    {
        outEvent = Event(EventType::TOUCH_UP, this, platformEvent);

        Vec2f prevPos = currentPos;
        if (pointerId >= 0 && pointerId < static_cast<int32>(m_touchPrevPositions.Size()))
        {
            prevPos = m_touchPrevPositions[pointerId];
        }

        MotionData motionData { currentPos, currentPos - prevPos, /* isAbsolute */ false };
        outEvent.GetEventData().Set(TouchEventData { pointerId, motionData });

        if (pointerId >= 0 && pointerId < static_cast<int32>(m_touchPrevPositions.Size()))
        {
            m_touchPrevPositions[pointerId] = Vec2f::Zero();
        }

        return true;
    }

    default:
#ifdef HYP_IOS
        if (platformEvent.iosEvent.uiEvent != nullptr)
        {
            [(UIEvent*)platformEvent.iosEvent.uiEvent release];
        }
#endif
        break;
    }

    return false;
}

void iOSApplicationWindow::Close()
{
    TUniqueLock lock(m_mtx);

    if (m_hwnd)
    {
        UIWindow* window = (UIWindow*)m_hwnd;
        window.hidden = YES;
        [window release];
        m_hwnd = nullptr;
    }

    if (m_metalLayer)
    {
        [(id)m_metalLayer release];
        m_metalLayer = nullptr;
    }

    if (m_uiView)
    {
        HyperionMetalView* view = (HyperionMetalView*)m_uiView;
        view.hyperionWindow = nullptr;
        [view removeFromSuperview];
        [view release];
        m_uiView = nullptr;
    }

#if HYP_VULKAN
    if (m_vkSurface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(
            RI.GetInstance()->GetInstance(),
            m_vkSurface,
            nullptr);
        m_vkSurface = VK_NULL_HANDLE;
    }
#endif
}

void iOSApplicationWindow::SetMousePosition(Vec2i position)
{
    // NO-OP
}

Vec2i iOSApplicationWindow::GetMousePosition() const
{
    TSharedLock lock(m_mtx);
    return Vec2i(m_touchPosition);
}

Vec2i iOSApplicationWindow::GetDimensions() const
{
    TSharedLock lock(m_mtx);

    CGFloat scale = m_metalLayer ? ((CAMetalLayer*)m_metalLayer).contentsScale : 1.0;

    if (m_uiView)
    {
        UIView* view = (UIView*)m_uiView;
        CGRect bounds = view.bounds;
        return Vec2i(int(bounds.size.width * scale), int(bounds.size.height * scale));
    }

    return m_size;
}

void iOSApplicationWindow::SetIsMouseLocked(bool locked)
{
    m_mouseLocked = locked;
}

bool iOSApplicationWindow::HasMouseFocus() const
{
    if (m_hwnd)
    {
        UIWindow* window = (UIWindow*)m_hwnd;
        return window.isKeyWindow;
    }

    return m_uiView != nullptr;
}

float iOSApplicationWindow::GetContentScaleFactor() const
{
    if (m_metalLayer)
    {
        return float(((CAMetalLayer*)m_metalLayer).contentsScale);
    }

    return float([UIScreen mainScreen].nativeScale);
}

float iOSApplicationWindow::GetRenderTargetScale() const
{
    // Render at 65% of native res
    return 0.65f;
}

} // namespace Hyperion
