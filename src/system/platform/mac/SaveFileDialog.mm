#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>

#include <system/SaveFileDialog.hpp>

#include <core/functional/Proc.hpp>

#import "Util/BlockInvoker.h"

namespace Hyperion {

void ShowSaveFileDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Span<const ANSIStringView> extensions,
    Proc<void(TResult<FilePath>&& result)>&& callback)
{
    __block dispatch_semaphore_t sem = dispatch_semaphore_create(0);

    __block Proc<void(TResult<FilePath>&& result)>* pCallback = callback.IsValid()
        ? new Proc<void(TResult<FilePath>&& result)>(std::move(callback))
        : nullptr;
    
    void (^saveFileDialogBlock)(void) = ^{
        @autoreleasepool
        {
            auto invokeCallback = ^(TResult<FilePath>&& r)
            {
                if (pCallback != nullptr)
                {
                    (*pCallback)(std::move(r));
                    delete pCallback;
                }
            };
            
            NSSavePanel* panel = [NSSavePanel savePanel];
            
            NSString* titleStr = [NSString stringWithUTF8String:String(title).Data()];
            [panel setTitle:titleStr];
            
            NSString* baseDirStr = [NSString stringWithUTF8String:String(baseDir).Data()];
            [panel setDirectoryURL:[NSURL fileURLWithPath:baseDirStr]];
            
            if (extensions.Size() != 0)
            {
                NSMutableArray<NSString*>* types = [NSMutableArray arrayWithCapacity:extensions.Size()];
                
                for (const ANSIStringView& ext : extensions)
                {
                    [types addObject:[NSString stringWithUTF8String:ext.Data()]];
                }
                
                panel.allowedFileTypes = types;
            }
            
            dispatch_semaphore_signal(sem);
            
            NSModalResponse result = [panel runModal];
            
            if (result == NSModalResponseOK)
            {
                NSURL* url = [panel URL];
                FilePath filepath([[url path] UTF8String]);
                
                invokeCallback(std::move(filepath));
            }
            else
            {
                invokeCallback(HYP_MAKE_ERROR(Error, "Save file cancelled"));
            }
        }
    };
    
    if ([NSThread isMainThread])
    {
        saveFileDialogBlock();
    }
    else
    {
        dispatch_async(dispatch_get_main_queue(), saveFileDialogBlock);
    }
    
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
}

} // namespace Hyperion
