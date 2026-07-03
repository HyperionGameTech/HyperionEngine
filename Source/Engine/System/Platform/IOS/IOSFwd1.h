#ifndef HYP_IOS_FWD_H
#define HYP_IOS_FWD_H

#ifdef __OBJC__
@class UIView;
@class UIWindow;
@class UIEvent;
@class UITouch;
@class UIViewController;
@class UIScreen;
@class CADisplayLink;
#else
struct UIView;
struct UIWindow;
struct UIEvent;
struct UITouch;
struct UIViewController;
struct UIScreen;
struct CADisplayLink;
#endif

#endif
