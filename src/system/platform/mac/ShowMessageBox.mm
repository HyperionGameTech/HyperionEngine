#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>

#include <core/functional/Proc.hpp>

#include <core/threading/Task.hpp>

using namespace hyperion;

extern "C" {

int ShowMessageBox(
    int type,
    const char* title,
    const char* message,
    int buttons,
    const char* buttonTexts[3],
    const void* buttonFuncs[3],
    hyperion::TaskPromise<void>* promise)
{
    __block int returnValue = -1;
    __block bool doAsyncCall = ![NSThread isMainThread];

    __block NSString* titleString = [NSString stringWithUTF8String:title];
    __block NSString* messageString = [NSString stringWithUTF8String:message];
    __block NSString** buttonTextStrings = (NSString**)malloc(sizeof(NSString*) * 3);

    __block const Proc<void()>** buttonFuncProcs = doAsyncCall ? (const Proc<void()>**)malloc(sizeof(const Proc<void()>*) * 3) : NULL;

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

        if (doAsyncCall)
        {
            if (buttonFuncs != NULL)
            {
                buttonFuncProcs[i] = (const Proc<void()>*)buttonFuncs[i];
            }
            else
            {
                buttonFuncProcs[i] = NULL;
            }
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

            if (doAsyncCall)
            {
                Assert(promise != NULL);

                if (returnValue >= 0 && returnValue < 3)
                {
                    const Proc<void()>* pProc = buttonFuncProcs[returnValue];
                    if (pProc != NULL)
                    {
                        (*pProc)();
                    }
                }
                
                if (promise != NULL)
                {
                    promise->Fulfill();
                }

                // delete all funcs here - necessary hack for async, see more info in MessageBox.cpp
                for (int i = 0; i < 3; i++)
                {
                    const Proc<void()>* pProc = buttonFuncProcs[i];
                    if (pProc != NULL)
                    {
                        delete pProc;
                    }
                }

                free(buttonFuncProcs);
                buttonFuncProcs = NULL;
            }
        }
    };
    
    if (!doAsyncCall)
    {
        AssertDebug([NSThread isMainThread]);
        
        alertBlock();
    
        return returnValue;
    }

    dispatch_async(dispatch_get_main_queue(), alertBlock);

    return -1; // always return -1 for async calls, we'll invoke via pointer to Proc<> instead
}

} // extern "C"
