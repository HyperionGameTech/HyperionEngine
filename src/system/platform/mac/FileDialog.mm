#import <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>

#include <core/functional/Proc.hpp>

#import "Util/BlockInvoker.h"

namespace Hyperion {

static NSMutableArray<NSString*>* BuildAllowedFileTypes(Span<const ANSIStringView> extensions)
{
    if (extensions.Size() == 0)
    {
        return nil;
    }
    
    NSMutableArray<NSString*>* types = [NSMutableArray arrayWithCapacity:extensions.Size()];
    
    for (const ANSIStringView& ext : extensions)
    {
        [types addObject:[NSString stringWithUTF8String:ext.Data()]];
    }
    
    return types;
}

template <typename ResultType>
static void DispatchMainThread(
    void (^panelBlock)(void),
    dispatch_semaphore_t sem)
{
    if ([NSThread isMainThread])
    {
        panelBlock();
    }
    else
    {
        dispatch_async(dispatch_get_main_queue(), panelBlock);
    }
    
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
}

#pragma region Open File Dialog

void ShowOpenFileDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Span<const ANSIStringView> extensions,
    bool allowMultiple,
    bool allowDirectories,
    Proc<void(TResult<Array<FilePath>>&& result)>&& callback)
{
    __block dispatch_semaphore_t sem = dispatch_semaphore_create(0);

    __block Proc<void(TResult<Array<FilePath>>&& result)>* pCallback = callback.IsValid()
        ? new Proc<void(TResult<Array<FilePath>>&& result)>(std::move(callback))
        : nullptr;
    
    void (^dialogBlock)(void) = ^{
        @autoreleasepool
        {
            auto invokeCallback = ^(TResult<Array<FilePath>>&& r)
            {
                if (pCallback != nullptr)
                {
                    (*pCallback)(std::move(r));
                    delete pCallback;
                }
            };
            
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            [panel setCanChooseFiles:YES];
            [panel setCanChooseDirectories:allowDirectories ? YES : NO];
            [panel setAllowsMultipleSelection:allowMultiple ? YES : NO];
            
            NSString* titleStr = [NSString stringWithUTF8String:String(title).Data()];
            [panel setTitle:titleStr];
            
            NSString* baseDirStr = [NSString stringWithUTF8String:String(baseDir).Data()];
            [panel setDirectoryURL:[NSURL fileURLWithPath:baseDirStr]];
            
            NSMutableArray<NSString*>* types = BuildAllowedFileTypes(extensions);
            if (types != nil)
            {
                panel.allowedFileTypes = types;
            }
            
            dispatch_semaphore_signal(sem);
            
            NSModalResponse result = [panel runModal];
            
            if (result == NSModalResponseOK)
            {
                Array<FilePath> filepaths;
                
                for (NSURL* url in panel.URLs)
                {
                    filepaths.EmplaceBack([[url path] UTF8String]);
                }
                
                invokeCallback(std::move(filepaths));
            }
            else
            {
                invokeCallback(HYP_MAKE_ERROR(Error, "Open file cancelled"));
            }
        }
    };
    
    DispatchMainThread<Array<FilePath>>(dialogBlock, sem);
}

#pragma endregion Open File Dialog

#pragma region Save File Dialog

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
    
    void (^dialogBlock)(void) = ^{
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
            
            NSMutableArray<NSString*>* types = BuildAllowedFileTypes(extensions);
            if (types != nil)
            {
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
    
    DispatchMainThread<FilePath>(dialogBlock, sem);
}

#pragma endregion Save File Dialog

#pragma region Select Folder
void ShowSelectFolderDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Proc<void(TResult<FilePath>&& result)>&& callback)
{
    __block dispatch_semaphore_t sem = dispatch_semaphore_create(0);

    __block Proc<void(TResult<FilePath>&& result)>* pCallback = callback.IsValid()
        ? new Proc<void(TResult<FilePath>&& result)>(std::move(callback))
        : nullptr;
    
    void (^dialogBlock)(void) = ^{
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
            
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            [panel setCanChooseFiles:NO];
            [panel setCanChooseDirectories:YES];
            [panel setAllowsMultipleSelection:NO];
            [panel setCanCreateDirectories:YES];
            
            NSString* titleStr = [NSString stringWithUTF8String:String(title).Data()];
            [panel setTitle:titleStr];
            [panel setMessage:titleStr];
            
            NSString* baseDirStr = [NSString stringWithUTF8String:String(baseDir).Data()];
            [panel setDirectoryURL:[NSURL fileURLWithPath:baseDirStr]];
            
            dispatch_semaphore_signal(sem);
            
            NSModalResponse result = [panel runModal];
            
            if (result == NSModalResponseOK)
            {
                NSURL* url = [[panel URLs] firstObject];
                FilePath filepath([[url path] UTF8String]);
                
                invokeCallback(std::move(filepath));
            }
            else
            {
                invokeCallback(HYP_MAKE_ERROR(Error, "Folder selection cancelled"));
            }
        }
    };
    
    DispatchMainThread<FilePath>(dialogBlock, sem);
}

#pragma endregion Select Folder

} // namespace Hyperion
