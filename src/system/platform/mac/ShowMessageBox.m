#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>

int ShowMessageBox(int type, const char *title, const char *message, int buttons, const char *buttonTexts[3])
{
    
    __block int returnValue = -1;
    
    void (^alertBlock)(void) = ^{
        @autoreleasepool
        {
            NSAlert *alert = [[NSAlert alloc] init];
            [alert setMessageText:[NSString stringWithUTF8String:title]];
            [alert setInformativeText:[NSString stringWithUTF8String:message]];

            for (int i = 0; i < buttons && i < 3; i++)
            {
                [alert addButtonWithTitle:[NSString stringWithUTF8String:buttonTexts[i]]];
            }
            
            if (type == 0)
            {
                [alert setAlertStyle:NSInformationalAlertStyle];
            }
            else if (type == 1)
            {
                [alert setAlertStyle:NSWarningAlertStyle];
            }
            else if (type == 2)
            {
                [alert setAlertStyle:NSCriticalAlertStyle];
            }
            
            NSModalResponse result = [alert runModal];

            if (result == NSAlertFirstButtonReturn || result == NSModalResponseOK)
            {
                returnValue = 0;
            }
            else if (result == NSAlertSecondButtonReturn)
            {
                returnValue = 1;
            }
            else if (result == NSAlertThirdButtonReturn)
            {
                returnValue = 2;
            }
            else
            {
                returnValue = -1;
            }
        }
    };
    
    if ([NSThread isMainThread])
    {
        alertBlock();
    }
    else
    {
        dispatch_async(dispatch_get_main_queue(), alertBlock);
    }
    
    return returnValue;
}
