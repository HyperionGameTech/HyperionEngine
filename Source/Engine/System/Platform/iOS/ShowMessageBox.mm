#include <SystemPch.hpp>

#import <UIKit/UIKit.h>

extern "C"
{
    int ShowMessageBox(int type, const char* title, const char* message, int buttons, const char* buttonTexts[3])
    {
        @autoreleasepool
        {
            NSString* nsTitle = [NSString stringWithUTF8String:title ? title : ""];
            NSString* nsMessage = [NSString stringWithUTF8String:message ? message : ""];

            UIAlertController* alert = [UIAlertController alertControllerWithTitle:nsTitle
                                                                           message:nsMessage
                                                                    preferredStyle:UIAlertControllerStyleAlert];

            // Add buttons
            if (buttons > 0 && buttonTexts != nullptr)
            {
                for (int i = 0; i < buttons && i < 3; i++)
                {
                    if (buttonTexts[i] == nullptr)
                        break;

                    NSString* buttonTitle = [NSString stringWithUTF8String:buttonTexts[i]];
                    UIAlertActionStyle style = (i == buttons - 1)
                        ? UIAlertActionStyleCancel
                        : UIAlertActionStyleDefault;

                    UIAlertAction* action = [UIAlertAction actionWithTitle:buttonTitle
                                                                     style:style
                                                                   handler:nil];
                    [alert addAction:action];
                }
            }
            else
            {
                UIAlertAction* okAction = [UIAlertAction actionWithTitle:@"OK"
                                                                   style:UIAlertActionStyleDefault
                                                                 handler:nil];
                [alert addAction:okAction];
            }

            // Get the key window to present from
            UIWindow* keyWindow = nil;
            if (@available(iOS 13.0, *))
            {
                NSSet<UIScene*>* scenes = [UIApplication sharedApplication].connectedScenes;
                for (UIScene* scene in scenes)
                {
                    if (scene.activationState == UISceneActivationStateForegroundActive)
                    {
                        UIWindowScene* windowScene = (UIWindowScene*)scene;
                        for (UIWindow* window in windowScene.windows)
                        {
                            if (window.isKeyWindow)
                            {
                                keyWindow = window;
                                break;
                            }
                        }
                        if (keyWindow) break;
                    }
                }
            }
            else
            {
                keyWindow = [UIApplication sharedApplication].keyWindow;
            }

            UIViewController* rootVC = keyWindow.rootViewController;
            if (rootVC)
            {
                [rootVC presentViewController:alert animated:YES completion:nil];
            }
        }

        return 0;
    }
}
