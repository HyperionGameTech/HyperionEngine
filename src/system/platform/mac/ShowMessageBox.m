#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>

int ShowMessageBox(int type, const char* title, const char* message, int buttons, const char* buttonTexts[3])
{
    __block int returnValue = -1;

    __block NSString* titleString = [NSString stringWithUTF8String:title];
    __block NSString* messageString = [NSString stringWithUTF8String:message];
    __block NSString** buttonTextStrings = malloc(sizeof(NSString*) * 3);

    for (int i = 0; i < 3; i++)
    {
        if (buttonTexts[i] == NULL)
        {
            buttonTextStrings[i] = NULL;
        }
        else
        {
            buttonTextStrings[i] = [NSString stringWithUTF8String:buttonTexts[i]];
        }
    }
    
    void (^alertBlock)(void) = ^{
        @autoreleasepool
        {
            NSAlert* alert = [[NSAlert alloc] init];
            [alert setMessageText:titleString];
            [alert setInformativeText:messageString];

            for (int i = 0; i < 3; i++)
            {
                if (buttonTextStrings[i] == NULL)
                {
                    break; // Stop adding buttons if we hit a NULL
                }

                [alert addButtonWithTitle:buttonTextStrings[i]];
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

            free(buttonTextStrings);
            buttonTextStrings = NULL;
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
