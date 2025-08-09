#include <windows.h>
#include <commdlg.h>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Result.hpp>

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/io/ByteWriter.hpp>

static const wchar_t* CommDlgErrorToString(DWORD err)
{
    switch (err)
    {
    case 0:                         return L"User canceled or closed the dialog";
    case CDERR_DIALOGFAILURE:       return L"CDERR_DIALOGFAILURE: general failure in dialog box";
    case CDERR_STRUCTSIZE:          return L"CDERR_STRUCTSIZE: invalid lStructSize";
    case CDERR_INITIALIZATION:      return L"CDERR_INITIALIZATION: failed during initialization";
    case CDERR_NOTEMPLATE:          return L"CDERR_NOTEMPLATE: custom template missing or invalid";
    case CDERR_NOHINSTANCE:         return L"CDERR_NOHINSTANCE: hInstance missing";
    case CDERR_LOADSTRFAILURE:      return L"CDERR_LOADSTRFAILURE: failed to load a string resource";
    case CDERR_FINDRESFAILURE:      return L"CDERR_FINDRESFAILURE: failed to find a resource";
    case CDERR_LOADRESFAILURE:      return L"CDERR_LOADRESFAILURE: failed to load a resource";
    case CDERR_LOCKRESFAILURE:      return L"CDERR_LOCKRESFAILURE: failed to lock a resource";
    case CDERR_MEMALLOCFAILURE:     return L"CDERR_MEMALLOCFAILURE: memory allocation failed";
    case CDERR_MEMLOCKFAILURE:      return L"CDERR_MEMLOCKFAILURE: memory lock failed";
    case CDERR_NOHOOK:              return L"CDERR_NOHOOK: hook function pointer invalid";
    case CDERR_REGISTERMSGFAIL:     return L"CDERR_REGISTERMSGFAIL: failed to register a message";
    case FNERR_SUBCLASSFAILURE:     return L"FNERR_SUBCLASSFAILURE: failed to subclass a listbox or editbox";
    case FNERR_INVALIDFILENAME:     return L"FNERR_INVALIDFILENAME: lpstrFile contains invalid characters or too long";
    case FNERR_BUFFERTOOSMALL:      return L"FNERR_BUFFERTOOSMALL: file buffer too small for returned file list";
    default:                        return L"Unknown error code";
    }
}

namespace hyperion {

void ShowOpenFileDialog(
    UTF8StringView title,
    const FilePath &baseDir,
    Span<const ANSIStringView> extensions,
    bool allowMultiple,
    bool allowDirectories,
    void(*callback)(TResult<Array<FilePath>>&& result))
{
    WideString titleWide = String(title).ToWide();
    WideString baseDirWide = String(baseDir).ToWide();

    MemoryByteWriter filterBufferWriter;

    if (extensions.Size() != 0)
    {
        for (const ANSIStringView &ext : extensions)
        {
            WideString tmpString = L"*." + ANSIString(ext).ToWide();
            filterBufferWriter.Write(tmpString.Data(), tmpString.Size() * sizeof(wchar_t));
            filterBufferWriter.Write(L'\0');
            filterBufferWriter.Write(tmpString.Data(), tmpString.Size() * sizeof(wchar_t));
            filterBufferWriter.Write(L'\0');
        }
    }
    else
    {
        static const WideString allFilesString = L"All Files\0*.*\0";

        filterBufferWriter.Write(allFilesString.Data(), allFilesString.Size() * sizeof(wchar_t));
    }

    ByteBuffer fileNameBufferData;
    fileNameBufferData.SetSize(65535 * sizeof(wchar_t));

    static constexpr uint32 maxRetries = 10;
    static constexpr SizeType maxFileNameBufferSize = 1u << 16;

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

            wchar_t *p = reinterpret_cast<wchar_t*>(fileNameBufferData.Data());

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

            callback(std::move(results));

            return;
        }

        DWORD err = CommDlgExtendedError();

        if (err != 0)
        {
            if (err == FNERR_BUFFERTOOSMALL && fileNameBufferData.Size() * 2 <= maxFileNameBufferSize)
            {
                fileNameBufferData.SetSize(fileNameBufferData.Size() * 2);
                retry = true;

                continue;
            }

            callback(HYP_MAKE_ERROR(Error, "Failed to handle open file dialog (error code: {}, message: {})", err, CommDlgErrorToString(err)));

            return;
        }
    }
    while (retry && numRetries < maxRetries);

    callback(HYP_MAKE_ERROR(Error, "Open file cancelled"));
}

} // namespace hyperion