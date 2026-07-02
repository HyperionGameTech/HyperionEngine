#import <UIKit/UIKit.h>

#include <Core/Defines.hpp>

using namespace Hyperion;

extern "C" {

int ShowMessageBox(int type, const char *title, const char *message, int buttons, const char *buttonTexts[3])
{
    // Come back to this when we want to target iOS

    HYP_NOT_IMPLEMENTED();

    // __block int returnValue = -1;

    // void (^alertBlock)(void) = ^{
    //     @autoreleasepool {
    //         UIAlertController *alert = [UIAlertController alertControllerWithTitle:[NSString stringWithUTF8String:title]
    //                                                                        message:[NSString stringWithUTF8String:message]
    //                                                                 preferredStyle:UIAlertControllerStyleAlert];

    //         for (int i = 0; i < buttons && i < 3; i++) {
    //             UIAlertAction *action = [UIAlertAction actionWithTitle:[NSString stringWithUTF8String:buttonTexts[i]]
    //                                                             style:UIAlertActionStyleDefault
    //                                                           handler:^(UIAlertAction * _Nonnull action) {
    //                 returnValue = i;
    //             }];
    //             [alert addAction:action];
    //         }

    //         // Present the alert
    //         UIViewController *rootViewController = [UIApplication sharedApplication].keyWindow.rootViewController;
    //         [rootViewController presentViewController:alert animated:YES completion:nil];
    //     }
    // };
}

} // extern "C"