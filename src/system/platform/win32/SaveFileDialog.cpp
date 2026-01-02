#include <SystemPch.hpp>

#include <windows.h>
#include <commdlg.h>
#include <cderr.h>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Result.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/functional/Proc.hpp>

#include <core/io/ByteWriter.hpp>

// Defined in OpenFileDialog.cpp
extern const wchar_t* CommDlgErrorToString(DWORD err);

namespace Hyperion {

void ShowSaveFileDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Span<const ANSIStringView> extensions,
    bool allowDirectories,
    Proc<void(TResult<FilePath>&& result)>&& callback)
{
    WideString titleWide = String(title).ToWide();
    WideString baseDirWide = String(baseDir).ToWide();

    MemoryByteWriter filterBufferWriter;

    if (extensions.Size() != 0)
    {
        for (const ANSIStringView& ext : extensions)
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
    fileNameBufferData.SetSize(MAX_PATH * sizeof(wchar_t));
    Memory::MemSet(fileNameBufferData.Data(), 0, fileNameBufferData.Size());

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
        FilePath result(WideString(p));

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

} // namespace Hyperion
