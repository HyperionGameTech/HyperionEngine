#include "FxFile.hpp"

#include <Core/FxDefines.hpp>
#include <Core/MemPool/FxMemPool.hpp>

using DataType = FxFile::DataType;
using ModType = FxFile::ModType;

FxFile::FxFile(const char* path, FxFile::ModType mt, FxFile::DataType dt) { Open(path, mt, dt); }

static const char* MakeModeStr(FxFile::ModType mt, FxFile::DataType dt)
{
    if (dt == DataType::eBinary) {
        if (mt == ModType::eRead) {
            return "rb";
        }
        else if (mt == ModType::eWrite) {
            return "wb";
        }
    }

    if (mt == ModType::eRead) {
        return "r";
    }
    else if (mt == ModType::eWrite) {
        return "w";
    }

    return "r";
}

void FxFile::Open(const char* path, FxFile::ModType mt, DataType dt)
{
    mModType = mt;
    mDataType = dt;

    const char* mode = MakeModeStr(mt, dt);

    // If the file handle is already set, reopen it with the new path or mode.
    if (pFileHandle != nullptr) {
#ifdef FX_PLATFORM_WINDOWS
        errno_t result = freopen_s(&pFileHandle, path, mode, pFileHandle);
        if (result) {
            pFileHandle = nullptr;
        }

        return;
#else
        freopen(path, mode, pFileHandle);
        return;
#endif
    }

#ifdef FX_PLATFORM_WINDOWS
    errno_t result = fopen_s(&pFileHandle, path, mode);

    if (result) {
        pFileHandle = nullptr;
    }

#else
    pFileHandle = fopen(path, mode);
#endif
}

uint64 FxFile::GetFileSize()
{
    if (pFileHandle == nullptr) {
        return 0;
    }

    if (mbSizeOutOfDate) {
        uint64 current_seek = ftell(pFileHandle);

        SeekToEnd();
        mFileSize = ftell(pFileHandle);
        SeekTo(current_seek);

        mbSizeOutOfDate = false;
    }

    return mFileSize;
}

void FxFile::Write(const char* str)
{
    if (!pFileHandle) {
        FxLogWarning(spcErrCannotWriteUnopened);
        return;
    }

    fputs(str, pFileHandle);
}

void FxFile::WriteRaw(const void* data, uint64 size)
{
    if (!pFileHandle) {
        FxLogWarning(spcErrCannotWriteUnopened);
        return;
    }

    fwrite(data, 1, size, pFileHandle);
}

void FxFile::SeekTo(uint64 index) { fseek(pFileHandle, index, SEEK_SET); }
void FxFile::SeekToEnd() { fseek(pFileHandle, 0, SEEK_END); }
void FxFile::SeekBy(uint32 by) { fseek(pFileHandle, by, SEEK_CUR); }

uint64 FxFile::GetPosition() const { return ftell(pFileHandle); }


void FxFile::Flush()
{
    if (!pFileHandle) {
        return;
    }
    fflush(pFileHandle);
}

void FxFile::Write(char ch)
{
    if (!pFileHandle) {
        return;
    }
    fputc(ch, pFileHandle);
}

void FxFile::Close()
{
    if (pFileHandle == nullptr) {
        return;
    }

    fclose(pFileHandle);
    pFileHandle = nullptr;
}

#include <filesystem>


std::filesystem::file_time_type FxFile::GetLastModifiedTime(const std::string& path)
{
    return std::filesystem::last_write_time(path);
}
