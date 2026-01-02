#include <windows.h>
#include <shobjidl.h>
#include <winerror.h>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Result.hpp>
#include <core/utilities/DeferredScope.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/functional/Proc.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Thread.hpp>

#include <core/debug/Debug.hpp>

namespace Hyperion {

thread_local bool s_isCOMInitialized = false;

static void InitializeCOM()
{
    if (!s_isCOMInitialized)
    {
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        
        Assert(SUCCEEDED(hr), "Failed to initialize COM library");

        s_isCOMInitialized = true;

        ThreadBase* thread = CurrentThreadObject();
        Assert(thread != nullptr, "CurrentThreadObject returned null");
        
        thread->AtExit([]()
        {
            if (s_isCOMInitialized)
            {
                CoUninitialize();
                s_isCOMInitialized = false;
            }
        });
    }
}

static TResult<FilePath> ResultFromHResult(HRESULT hr)
{
    switch (hr)
    {
    case E_ACCESSDENIED:
        return TResult<FilePath>(HYP_MAKE_ERROR(Error, "Access denied"));
    case E_OUTOFMEMORY:
        return TResult<FilePath>(HYP_MAKE_ERROR(Error, "Out of memory"));
    case E_INVALIDARG:
        return TResult<FilePath>(HYP_MAKE_ERROR(Error, "Invalid argument"));
    case HRESULT_FROM_WIN32(ERROR_CANCELLED):
        return TResult<FilePath>(HYP_MAKE_ERROR(Error, "Operation cancelled by user"));
    default:
        return TResult<FilePath>(HYP_MAKE_ERROR(Error, "Unknown error (HRESULT: {})", hr));
    }
}

void ShowSelectFolderDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Proc<void(TResult<FilePath>&& result)>&& callback)
{
    InitializeCOM();

    IFileDialog *pFileDialog = NULL;

    HRESULT hr = CoCreateInstance(
        CLSID_FileOpenDialog, 
        NULL, 
        CLSCTX_INPROC_SERVER, 
        IID_PPV_ARGS(&pFileDialog));

    if (!SUCCEEDED(hr))
    {
        callback(ResultFromHResult(hr));
        return;
    }

    HYP_DEFER({
        if (pFileDialog)
        {
            pFileDialog->Release();
        }
    });

    DWORD dwOptions;
    if (SUCCEEDED(pFileDialog->GetOptions(&dwOptions)))
    {
        pFileDialog->SetOptions(dwOptions | FOS_PICKFOLDERS);
    }
    else
    {
        callback(ResultFromHResult(hr));
    }

    if (baseDir.Any() && baseDir.IsDirectory())
    {
        WideString baseDirWide = String(baseDir).ToWide();

        IShellItem* pFolderItem = nullptr;
        hr = SHCreateItemFromParsingName(
            baseDirWide.Data(),
            NULL,
            IID_PPV_ARGS(&pFolderItem));

        if (!SUCCEEDED(hr))
        {
            callback(ResultFromHResult(hr));
            return;
        }
        else
        {
            pFileDialog->SetFolder(pFolderItem);
            pFolderItem->Release();
        }
    }

    WideString titleWide = String(title).ToWide();

    pFileDialog->SetTitle(titleWide.Data());

    hr = pFileDialog->Show(NULL);

    if (!SUCCEEDED(hr))
    {
        callback(ResultFromHResult(hr));
        return;
    }

    IShellItem *pItem;
    hr = pFileDialog->GetResult(&pItem);

    HYP_DEFER({
        if (pItem)
        {
            pItem->Release();
        }
    });

    if (!SUCCEEDED(hr))
    {
        callback(ResultFromHResult(hr));
        return;
    }

    PWSTR pszFilePath;
    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

    if (!SUCCEEDED(hr))
    {
        callback(ResultFromHResult(hr));
        return;
    }
    
    FilePath selectedPath = FilePath(WideString(pszFilePath).ToUtf8());
    callback(TResult<FilePath>(std::move(selectedPath)));

    CoTaskMemFree(pszFilePath);
}

} // namespace Hyperion