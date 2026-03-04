#include <SystemPch.hpp>

#include <windows.h>
#include <commdlg.h>
#include <cderr.h>
#include <shobjidl.h>
#include <winerror.h>

#include <Core/utilities/StringView.hpp>
#include <Core/utilities/Result.hpp>
#include <Core/utilities/DeferredScope.hpp>

#include <Core/filesystem/FilePath.hpp>

#include <Core/functional/Proc.hpp>

#include <Core/io/ByteWriter.hpp>

#include <Core/threading/Threads.hpp>
#include <Core/threading/Thread.hpp>

#include <Core/debug/Debug.hpp>

namespace Hyperion {

static const wchar_t* CommDlgErrorToString(DWORD err)
{
    switch (err)
    {
    case 0:
        return L"User canceled or closed the dialog";
    case CDERR_DIALOGFAILURE:
        return L"CDERR_DIALOGFAILURE: general failure in dialog box";
    case CDERR_STRUCTSIZE:
        return L"CDERR_STRUCTSIZE: invalid lStructSize";
    case CDERR_INITIALIZATION:
        return L"CDERR_INITIALIZATION: failed during initialization";
    case CDERR_NOTEMPLATE:
        return L"CDERR_NOTEMPLATE: custom template missing or invalid";
    case CDERR_NOHINSTANCE:
        return L"CDERR_NOHINSTANCE: hInstance missing";
    case CDERR_LOADSTRFAILURE:
        return L"CDERR_LOADSTRFAILURE: failed to load a string resource";
    case CDERR_FINDRESFAILURE:
        return L"CDERR_FINDRESFAILURE: failed to find a resource";
    case CDERR_LOADRESFAILURE:
        return L"CDERR_LOADRESFAILURE: failed to load a resource";
    case CDERR_LOCKRESFAILURE:
        return L"CDERR_LOCKRESFAILURE: failed to lock a resource";
    case CDERR_MEMALLOCFAILURE:
        return L"CDERR_MEMALLOCFAILURE: memory allocation failed";
    case CDERR_MEMLOCKFAILURE:
        return L"CDERR_MEMLOCKFAILURE: memory lock failed";
    case CDERR_NOHOOK:
        return L"CDERR_NOHOOK: hook function pointer invalid";
    case CDERR_REGISTERMSGFAIL:
        return L"CDERR_REGISTERMSGFAIL: failed to register a message";
    case FNERR_SUBCLASSFAILURE:
        return L"FNERR_SUBCLASSFAILURE: failed to subclass a listbox or editbox";
    case FNERR_INVALIDFILENAME:
        return L"FNERR_INVALIDFILENAME: lpstrFile contains invalid characters or too long";
    case FNERR_BUFFERTOOSMALL:
        return L"FNERR_BUFFERTOOSMALL: file buffer too small for returned file list";
    default:
        return L"Unknown error code";
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

static void BuildFilterBuffer(Span<const ANSIStringView> extensions, MemoryByteWriter& filterBufferWriter)
{
    auto WriteWideString = [&](const WideString& str)
    {
        filterBufferWriter.Write(str.Data(), str.Size() * sizeof(wchar_t));
    };

    auto WriteNullTerminator = [&]()
    {
        filterBufferWriter.Write(L'\0');
    };

    if (extensions.Size() != 0)
    {
        WideString patternString;

        for (size_t i = 0; i < extensions.Size(); i++)
        {
            if (i != 0)
            {
                patternString += L";";
            }

            patternString += L"*." + ANSIString(extensions[i]).ToWide();
        }

        WideString displayName = L"Supported Files (" + patternString + L")";

        WriteWideString(displayName);
        WriteNullTerminator();
        WriteWideString(patternString);
        WriteNullTerminator();
    }

    static const WideString s_allFilesDisplay = L"All Files (*.*)";
    static const WideString s_allFilesPattern = L"*.*";

    WriteWideString(s_allFilesDisplay);
    WriteNullTerminator();
    WriteWideString(s_allFilesPattern);
    WriteNullTerminator();

    // Double-null terminator required by Windows to signal the end of the filter list
    WriteNullTerminator();
}

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
        
        thread->AddOnExitCallback([]()
        {
            if (s_isCOMInitialized)
            {
                CoUninitialize();
                s_isCOMInitialized = false;
            }
        });
    }
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
    WideString titleWide = String(title).ToWide();
    WideString baseDirWide = String(baseDir).ToWide();

    MemoryByteWriter filterBufferWriter;
    BuildFilterBuffer(extensions, filterBufferWriter);

    ByteBuffer fileNameBufferData;
    fileNameBufferData.SetSize(65535 * sizeof(wchar_t));

    static constexpr uint32 MaxRetries = 10;
    static constexpr size_t MaxFileNameBufferSize = 1u << 16;

    bool retry;
    uint32 numRetries = 0;

    do
    {
        retry = false;

        OPENFILENAMEW ofn {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFile = reinterpret_cast<wchar_t*>(fileNameBufferData.Data());
        ofn.nMaxFile = (DWORD)fileNameBufferData.Size();
        ofn.lpstrFilter = reinterpret_cast<wchar_t*>(filterBufferWriter.GetBuffer().Data());
        ofn.nFilterIndex = 1;
        ofn.lpstrTitle = titleWide.Data();
        ofn.lpstrInitialDir = baseDirWide.Data();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (allowMultiple)
        {
            ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;
        }

        if (allowDirectories)
        {
            ofn.Flags |= OFN_NOVALIDATE;
        }

        if (GetOpenFileNameW(&ofn))
        {
            Array<FilePath> results;

            wchar_t* p = reinterpret_cast<wchar_t*>(fileNameBufferData.Data());

            WideString dir = p;

            p += dir.Size() + 1;

            if (*p == 0)
            {
                results.PushBack(FilePath(dir));
            }
            else
            {
                // Multi select
                while (*p != L'\0')
                {
                    WideString filename = p;
                    p += filename.Size() + 1;

                    results.PushBack(FilePath(dir) / filename);
                }
            }

            if (callback)
            {
                callback(std::move(results));
            }

            return;
        }

        DWORD err = CommDlgExtendedError();

        if (err != 0)
        {
            if (err == FNERR_BUFFERTOOSMALL && fileNameBufferData.Size() * 2 <= MaxFileNameBufferSize)
            {
                fileNameBufferData.SetSize(fileNameBufferData.Size() * 2);
                retry = true;

                continue;
            }

            if (callback)
            {
                callback(HYP_MAKE_ERROR(Error, "Failed to handle open file dialog (error code: {}, message: {})", err, CommDlgErrorToString(err)));
            }

            return;
        }
    }
    while (retry && numRetries < MaxRetries);

    if (callback)
    {
        callback(HYP_MAKE_ERROR(Error, "Open file cancelled"));
    }
}

#pragma endregion Open File Dialog

#pragma region Save File Dialog

void ShowSaveFileDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Span<const ANSIStringView> extensions,
    Proc<void(TResult<FilePath>&& result)>&& callback)
{
    WideString titleWide = String(title).ToWide();
    WideString baseDirWide = String(baseDir).ToWide();

    MemoryByteWriter filterBufferWriter;
    BuildFilterBuffer(extensions, filterBufferWriter);

    ByteBuffer fileNameBufferData;
    fileNameBufferData.SetSize(MAX_PATH * sizeof(wchar_t));
    Memory::Fill(fileNameBufferData.Data(), 0, fileNameBufferData.Size());

    OPENFILENAMEW ofn {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = reinterpret_cast<wchar_t*>(fileNameBufferData.Data());
    ofn.nMaxFile = (DWORD)(fileNameBufferData.Size() / sizeof(wchar_t));
    ofn.lpstrFilter = reinterpret_cast<wchar_t*>(filterBufferWriter.GetBuffer().Data());
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = titleWide.Data();
    ofn.lpstrInitialDir = baseDirWide.Data();
    ofn.Flags = OFN_OVERWRITEPROMPT;

    // Set default extension if provided
    WideString defaultExtWide;
    if (extensions.Size() != 0)
    {
        defaultExtWide = ANSIString(extensions[0]).ToWide();
        ofn.lpstrDefExt = defaultExtWide.Data();
    }

    if (GetSaveFileNameW(&ofn))
    {
        wchar_t* p = reinterpret_cast<wchar_t*>(fileNameBufferData.Data());
        FilePath result = FilePath(String(p));

        if (callback)
        {
            callback(std::move(result));
        }

        return;
    }

    DWORD err = CommDlgExtendedError();

    if (err != 0)
    {
        if (callback)
        {
            callback(HYP_MAKE_ERROR(Error, "Failed to handle save file dialog (error code: {}, message: {})", err, CommDlgErrorToString(err)));
        }

        return;
    }

    if (callback)
    {
        callback(HYP_MAKE_ERROR(Error, "Save file cancelled"));
    }
}

#pragma endregion Open File Dialog

#pragma region Select Folder Dialog

void ShowSelectFolderDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Proc<void(TResult<FilePath>&& result)>&& callback)
{
    InitializeCOM();

    IFileDialog* pFileDialog = NULL;

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
        return;
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

    IShellItem* pItem;
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

#pragma endregion Select Folder Dialog

} // namespace Hyperion
