/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Memory/Pimpl.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class MemoryMappedFile;

class CORE_API MemoryMappedFileView
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
    bool Reopen(const MemoryMappedFile& file, size_t offset, size_t size);

    size_t Size() const;
    size_t FileOffset() const;
    void* Data();
    const void* Data() const;

private:
    friend class MemoryMappedFile;

    size_t m_mapOffset;
    size_t m_mapSize;
    size_t m_viewOffset;
    size_t m_viewSize;
    size_t m_fileOffset;
    void* m_address;
    bool m_isOpen;
};

class CORE_API MemoryMappedFile
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
    size_t FileSize() const;

    Mode GetMode() const;
    void SetMode(Mode mode);

    bool Resize(size_t newSize);
    bool EnsureCapacity(size_t capacity);

    bool MapRange(size_t offset, size_t size, MemoryMappedFileView& outView) const;

private:
    Pimpl<struct MemoryMappedFileImpl> m_impl;
};

} // namespace Hyperion
