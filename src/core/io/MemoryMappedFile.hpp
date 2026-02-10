/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/filesystem/FilePath.hpp>
#include <core/memory/Pimpl.hpp>
#include <core/Types.hpp>

namespace Hyperion {

class HYP_API MemoryMappedFileView
{
public:
    MemoryMappedFileView();
    MemoryMappedFileView(const MemoryMappedFileView& other) = delete;
    MemoryMappedFileView& operator=(const MemoryMappedFileView& other) = delete;

    MemoryMappedFileView(MemoryMappedFileView&&) noexcept = default;
    MemoryMappedFileView& operator=(MemoryMappedFileView&&) noexcept = default;

    ~MemoryMappedFileView();

    bool IsOpen() const;
    void Close();

    SizeType Size() const;
    SizeType FileOffset() const;
    void* Data();
    const void* Data() const;

private:
    friend class MemoryMappedFile;

    Pimpl<struct MemoryMappedFileViewImpl> m_impl;
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

    bool MapRange(MemoryMappedFileView& out_view, SizeType offset, SizeType size) const;

private:
    Pimpl<struct MemoryMappedFileImpl> m_impl;
};

} // namespace Hyperion
