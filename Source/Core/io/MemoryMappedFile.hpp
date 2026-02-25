/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/filesystem/FilePath.hpp>

#include <Core/memory/Pimpl.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class MemoryMappedFile;

class HYP_API MemoryMappedFileView
{
public:
    MemoryMappedFileView();
    MemoryMappedFileView(const MemoryMappedFileView& other) = delete;
    MemoryMappedFileView& operator=(const MemoryMappedFileView& other) = delete;

    MemoryMappedFileView(MemoryMappedFileView&& other) noexcept;
    MemoryMappedFileView& operator=(MemoryMappedFileView&& other) noexcept;

    ~MemoryMappedFileView();

    bool IsOpen() const;
    void Close();
    bool Reopen(const MemoryMappedFile& file, SizeType offset, SizeType size);

    SizeType Size() const;
    SizeType FileOffset() const;
    void* Data();
    const void* Data() const;

private:
    friend class MemoryMappedFile;

    SizeType m_mapOffset;
    SizeType m_mapSize;
    SizeType m_viewOffset;
    SizeType m_viewSize;
    SizeType m_fileOffset;
    void* m_address;
    bool m_isOpen;
};

class HYP_API MemoryMappedFile
{
public:
    enum class Mode
    {
        READ_ONLY,
        READ_WRITE
    };

    MemoryMappedFile();
    explicit MemoryMappedFile(const FilePath& filepath, Mode mode = Mode::READ_ONLY);

    MemoryMappedFile(const MemoryMappedFile& other) = delete;
    MemoryMappedFile& operator=(const MemoryMappedFile& other) = delete;

    MemoryMappedFile(MemoryMappedFile&&) noexcept = default;
    MemoryMappedFile& operator=(MemoryMappedFile&&) noexcept = default;

    ~MemoryMappedFile();

    bool Open();
    bool Open(const FilePath& filepath, Mode mode = Mode::READ_ONLY);

    // Caller must ensure all mapped views are closed before closing the file.
    void Close();

    bool IsOpen() const;
    SizeType FileSize() const;

    Mode GetMode() const;
    void SetMode(Mode mode);

    bool Resize(SizeType newSize);
    bool EnsureCapacity(SizeType capacity);

    bool MapRange(SizeType offset, SizeType size, MemoryMappedFileView& outView) const;

private:
    Pimpl<struct MemoryMappedFileImpl> m_impl;
};

} // namespace Hyperion
