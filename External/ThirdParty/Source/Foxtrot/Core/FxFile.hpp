#pragma once

#include <stdio.h>
#include <stdlib.h>

#include <Core/FxSlice.hpp>
#include <Core/FxTypes.hpp>
#include <Core/MemPool/FxMemPool.hpp>
#include <FxEngine.hpp>
#include <Math/FxMathUtil.hpp>
#include <filesystem>

class FxFile
{
private:
    static constexpr const char* spcErrCannotWriteUnopened = "Cannot write to file as it has not been opened!";
    static constexpr const char* spcErrCannotReadUnopened = "Cannot read from file as it has not been opened!";

public:
    enum DataType
    {
        eText = 0,
        eBinary,
    };

    enum ModType
    {
        eRead = 0,
        eWrite,
    };

public:
    FxFile() {}
    FxFile(const char* path, ModType mt, DataType dt);

    static std::filesystem::file_time_type GetLastModifiedTime(const std::string& path);

    void Open(const char* path, ModType mt, DataType dt);
    uint64 GetFileSize();

    template <typename TDataType>
    FxSlice<TDataType> Read(const FxSlice<TDataType>& out_buffer)
    {
        if (!pFileHandle) {
            return nullptr;
        }

        constexpr uint32 type_size = sizeof(TDataType);

        const uint64 n_to_read = std::fmin(GetFileSize() / type_size, out_buffer.Size);
        uint64 items_read = fread(out_buffer.pData, type_size, n_to_read, pFileHandle);

        return FxMakeSlice<TDataType>(out_buffer.pData, items_read);
    }

    template <typename TDataType>
    FxSlice<TDataType> Read()
    {
        if (!pFileHandle) {
            FxLogWarning(spcErrCannotReadUnopened);
            return nullptr;
        }

        const uint64 buffer_size = FxMath::AlignValue<sizeof(TDataType)>(GetFileSize());
        TDataType* buffer = gEnginePool->Alloc<TDataType>(buffer_size);

        return Read(FxMakeSlice(buffer, buffer_size));
    }

    void WriteRaw(const void* data, uint64 size);

    void Write(const char* str);
    void Write(const std::string& str) { Write(str.c_str()); }

    template <typename... TTypes>
    void WriteMulti(TTypes... values)
    {
        (Write(values), ...);
    }

    template <typename TType>
        requires std::is_integral_v<TType>
    void Write(TType value)
    {
        if (!pFileHandle) {
            FxLogWarning(spcErrCannotWriteUnopened);
            return;
        }

        if (mDataType == eBinary) {
            fwrite(&value, sizeof(value), 1, pFileHandle);
        }
        else {
            std::string str_value = std::to_string(value);
            fputs(str_value.c_str(), pFileHandle);
        }
    }

    template <typename TType>
    void Write(TType* data, uint64 size)
    {
        WriteRaw(reinterpret_cast<const void*>(data), size);
    }

    bool IsFileOpen() const { return (pFileHandle != nullptr); }

    void Write(char ch);
    void Flush();

    void SeekTo(uint64 index);
    void SeekBy(uint32 by);
    void SeekToEnd();
    uint64 GetPosition() const;

    void Close();

    ~FxFile() { Close(); }

public:
    FILE* pFileHandle = nullptr;

private:
    ModType mModType = ModType::eRead;
    DataType mDataType = DataType::eText;

    uint64 mFileSize = 0;
    bool mbSizeOutOfDate : 1 = true;
};
