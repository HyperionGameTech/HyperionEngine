#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>

#include <system/OpenFileDialog.hpp>

#import "Util/BlockInvoker.h"

namespace hyperion {

void ShowOpenFileDialog(
    UTF8StringView title, const FilePath& baseDir, Span<const ANSIStringView> extensions,
    bool allowMultiple, bool allowDirectories,
    void(*callback)(TResult<Array<FilePath>>&& result))
{
    __block dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    
    void (^openFileDialogBlock)(void) = ^{
        @autoreleasepool
        {
            auto invokeCallback = ^(TResult<Array<FilePath>>&& r)
            {
                callback(std::move(r));
            };
            
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            panel.canChooseDirectories = allowDirectories ? YES : NO;
            panel.allowsMultipleSelection = allowMultiple ? YES : NO;
            
            if (extensions.Size() != 0)
            {
                NSMutableArray<NSString*>* types = [NSMutableArray arrayWithCapacity:extensions.Size()];
                
                for (const ANSIStringView &ext : extensions)
                {
                    [types addObject:[NSString stringWithUTF8String:ext.Data()]];
                }
                
                panel.allowedFileTypes = types;
            }
            
            dispatch_semaphore_signal(sem);
            
            Array<FilePath> filepaths;
            
            NSModalResponse result = [panel runModal];
            
            if (result == NSModalResponseOK)
            {
                for (NSURL* url in panel.URLs)
                {
                    filepaths.EmplaceBack([[url path] UTF8String]);
                }
                
                invokeCallback(std::move(filepaths));
            }
            else
            {
                invokeCallback(HYP_MAKE_ERROR(Error, "Result: {}", result));
            }
        }
    };
    
    if ([NSThread isMainThread])
    {
        openFileDialogBlock();
    }
    else
    {
        dispatch_async(dispatch_get_main_queue(), openFileDialogBlock);
    }
    
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
}

} // namespace hyperion
