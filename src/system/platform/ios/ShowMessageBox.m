#import <UIKit/UIKit.h>

int ShowMessageBox(int type, const char *title, const char *message, int buttons, const char *buttonTexts[3])
{
    // iOS does not have a direct equivalent to NSAlert, but we can use UIAlertController
    // to present an alert with buttons. However, UIAlertController requires a UIViewController
    // to present it, and we need to ensure that we are on the main thread.

    __block int returnValue = -1;

    void (^alertBlock)(void) = ^{
        @autoreleasepool {
            UIAlertController *alert = [UIAlertController alertControllerWithTitle:[NSString stringWithUTF8String:title]
                                                                           message:[NSString stringWithUTF8String:message]
                                                                    preferredStyle:UIAlertControllerStyleAlert];

            for (int i = 0; i < buttons && i < 3; i++) {
                UIAlertAction *action = [UIAlertAction actionWithTitle:[NSString stringWithUTF8String:buttonTexts[i]]
                                                                style:UIAlertActionStyleDefault
                                                              handler:^(UIAlertAction * _Nonnull action) {
                    returnValue = i;
                }];
                [alert addAction:action];
            }

            // Present the alert
            UIViewController *rootViewController = [UIApplication sharedApplication].keyWindow.rootViewController;
            [rootViewController presentViewController:alert animated:YES completion:nil];
        }
    };
}