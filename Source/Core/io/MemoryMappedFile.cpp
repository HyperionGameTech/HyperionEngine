/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/io/MemoryMappedFile.hpp>

#include <Core/logging/Logger.hpp>

#include <Core/debug/Debug.hpp>
#include <Core/Defines.hpp>

#ifdef HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

struct MemoryMappedFileImpl
{
    FilePath filepath;
    MemoryMappedFile::Mode mode;
    size_t fileSize;

#ifdef HYP_WINDOWS
    HANDLE fileHandle;
    HANDLE mappingHandle;
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    int fd;
#endif

    MemoryMappedFileImpl()
        : mode(MemoryMappedFile::Mode::READ_ONLY),
          fileSize(0)
    {
#ifdef HYP_WINDOWS
        fileHandle = nullptr;
        mappingHandle = nullptr;
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
        fd = -1;
#endif
    }
};

#pragma region MemoryMappedFileView

MemoryMappedFileView::MemoryMappedFileView()
    : m_mapOffset(0),
      m_mapSize(0),
      m_viewOffset(0),
      m_viewSize(0),
      m_fileOffset(0),
      m_address(nullptr),
      m_isOpen(false)
{
}

MemoryMappedFileView::MemoryMappedFileView(MemoryMappedFileView&& other) noexcept
    : m_mapOffset(other.m_mapOffset),
      m_mapSize(other.m_mapSize),
      m_viewOffset(other.m_viewOffset),
      m_viewSize(other.m_viewSize),
      m_fileOffset(other.m_fileOffset),
      m_address(other.m_address),
      m_isOpen(other.m_isOpen)
{
    other.m_mapOffset = 0;
    other.m_mapSize = 0;
    other.m_viewOffset = 0;
    other.m_viewSize = 0;
    other.m_fileOffset = 0;
    other.m_address = nullptr;
    other.m_isOpen = false;
}

MemoryMappedFileView& MemoryMappedFileView::operator=(MemoryMappedFileView&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    Close();

    m_mapOffset = other.m_mapOffset;
    m_mapSize = other.m_mapSize;
    m_viewOffset = other.m_viewOffset;
    m_viewSize = other.m_viewSize;
    m_fileOffset = other.m_fileOffset;
    m_address = other.m_address;
    m_isOpen = other.m_isOpen;

    other.m_mapOffset = 0;
    other.m_mapSize = 0;
    other.m_viewOffset = 0;
    other.m_viewSize = 0;
    other.m_fileOffset = 0;
    other.m_address = nullptr;
    other.m_isOpen = false;

    return *this;
}

MemoryMappedFileView::~MemoryMappedFileView()
{
    Close();
}

bool MemoryMappedFileView::IsOpen() const
{
    return m_isOpen;
}

bool MemoryMappedFileView::Reopen(const MemoryMappedFile& file, size_t offset, size_t size)
{
    if (IsOpen())
    {
        Close();
    }

    return file.MapRange(offset, size, *this);
}

void MemoryMappedFileView::Close()
{
    if (!m_isOpen)
    {
        return;
    }

#ifdef HYP_WINDOWS
    if (m_address != nullptr)
    {
        UnmapViewOfFile(m_address);
    }
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    if (m_address != nullptr && m_mapSize > 0)
    {
        munmap(m_address, m_mapSize);
    }
#endif

    m_mapOffset = 0;
    m_mapSize = 0;
    m_viewOffset = 0;
    m_viewSize = 0;
    m_fileOffset = 0;
    m_address = nullptr;
    m_isOpen = false;
}

size_t MemoryMappedFileView::Size() const
{
    return m_viewSize;
}

size_t MemoryMappedFileView::FileOffset() const
{
    return m_fileOffset;
}

void* MemoryMappedFileView::Data()
{
    if (!m_isOpen || m_address == nullptr)
    {
        return nullptr;
    }

    return static_cast<ubyte*>(m_address) + m_viewOffset;
}

const void* MemoryMappedFileView::Data() const
{
    if (!m_isOpen || m_address == nullptr)
    {
        return nullptr;
    }

    return static_cast<const ubyte*>(m_address) + m_viewOffset;
}

#pragma endregion MemoryMappedFileView

#pragma region MemoryMappedFile

MemoryMappedFile::MemoryMappedFile()
    : m_impl(MakePimpl<MemoryMappedFileImpl>())
{
}

MemoryMappedFile::MemoryMappedFile(const FilePath& filepath, Mode mode)
    : m_impl(MakePimpl<MemoryMappedFileImpl>())
{
    m_impl->filepath = filepath;
    m_impl->mode = mode;
}

MemoryMappedFile::~MemoryMappedFile()
{
    Close();
}

bool MemoryMappedFile::Open()
{
    if (!m_impl)
    {
        m_impl = MakePimpl<MemoryMappedFileImpl>();
    }

    if (IsOpen())
    {
        return true;
    }

#ifdef HYP_WINDOWS
    const DWORD desired_access = m_impl->mode == Mode::READ_WRITE
        ? (GENERIC_READ | GENERIC_WRITE)
        : GENERIC_READ;

    const DWORD share_mode = m_impl->mode == Mode::READ_ONLY
        ? FILE_SHARE_READ
        : 0;

    const WideString wide_path = m_impl->filepath.ToWide();

    HANDLE fileHandle = CreateFileW(
        wide_path.Data(),
        desired_access,
        share_mode,
        nullptr,
        m_impl->mode == Mode::READ_ONLY ? OPEN_EXISTING : OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        DWORD errorCode = GetLastError();

        HYP_LOG(Core, Error, "Failed to open file at path {}, Error code was: ({})",
            m_impl->filepath, errorCode);

        return false;
    }

    LARGE_INTEGER fileSize = {};

    if (!GetFileSizeEx(fileHandle, &fileSize))
    {
        CloseHandle(fileHandle);
        return false;
    }

    m_impl->fileSize = static_cast<size_t>(fileSize.QuadPart);
    m_impl->fileHandle = fileHandle;

    if (m_impl->fileSize == 0)
    {
        m_impl->mappingHandle = nullptr;
        return true;
    }

    const DWORD protection = m_impl->mode == Mode::READ_WRITE
        ? PAGE_READWRITE
        : PAGE_READONLY;

    HANDLE mappingHandle = CreateFileMappingW(
        fileHandle,
        nullptr,
        protection,
        0,
        0,
        nullptr);

    if (mappingHandle == nullptr)
    {
        CloseHandle(fileHandle);
        m_impl->fileHandle = nullptr;
        return false;
    }

    m_impl->mappingHandle = mappingHandle;

    return true;
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    const int openFlags = m_impl->mode == Mode::READ_WRITE
        ? O_RDWR | O_CREAT
        : O_RDONLY;

    const int fd = open(m_impl->filepath.Data(), openFlags);

    if (fd < 0)
    {
        return false;
    }

    struct stat st = {};

    if (fstat(fd, &st) != 0)
    {
        close(fd);
        return false;
    }

    m_impl->fileSize = st.st_size;
    m_impl->fd = fd;

    return true;
#else
    HYP_FAIL("Unsupported platform for memory mapped files");
    return false;
#endif
}

bool MemoryMappedFile::Open(const FilePath& filepath, Mode mode)
{
    if (!m_impl)
    {
        m_impl = MakePimpl<MemoryMappedFileImpl>();
    }

    if (IsOpen())
    {
        Close();
    }

    m_impl->filepath = filepath;
    m_impl->mode = mode;

    return Open();
}

bool MemoryMappedFile::MapRange(size_t offset, size_t size, MemoryMappedFileView& outView) const
{
    if (!m_impl || !IsOpen())
    {
        return false;
    }

    HYP_CORE_ASSERT(!outView.IsOpen(), "MemoryMappedFileView is already open");

    if (m_impl->mode == Mode::READ_WRITE && size > 0)
    {
        const size_t endOffset = offset + size;

        if (endOffset < offset)
        {
            return false;
        }

        if (endOffset > m_impl->fileSize)
        {
            auto* self = const_cast<MemoryMappedFile*>(this);

            if (!self->Resize(endOffset))
            {
                return false;
            }
        }
    }

    if (m_impl->mode != Mode::READ_WRITE || size == 0)
    {
        if (offset > m_impl->fileSize)
        {
            offset = m_impl->fileSize;
        }
    }

    if (size == 0)
    {
        size = m_impl->fileSize > offset ? (m_impl->fileSize - offset) : 0;
    }
    else if (m_impl->mode != Mode::READ_WRITE && offset + size > m_impl->fileSize)
    {
        size = m_impl->fileSize - offset;
    }

    outView.m_fileOffset = offset;
    outView.m_viewSize = size;
    outView.m_viewOffset = 0;
    outView.m_mapOffset = 0;
    outView.m_mapSize = 0;
    outView.m_address = nullptr;
    outView.m_isOpen = true;

    if (outView.m_viewSize == 0)
    {
        return true;
    }

#ifdef HYP_WINDOWS
    if (m_impl->mappingHandle == nullptr)
    {
        return false;
    }

    SYSTEM_INFO systemInfo = {};
    GetSystemInfo(&systemInfo);

    const size_t granularity = static_cast<size_t>(systemInfo.dwAllocationGranularity);
    const size_t alignedOffset = (offset / granularity) * granularity;
    const size_t viewDelta = offset - alignedOffset;
    const size_t mapSize = viewDelta + outView.m_viewSize;

    const DWORD mapAccess = m_impl->mode == Mode::READ_WRITE
        ? (FILE_MAP_READ | FILE_MAP_WRITE)
        : FILE_MAP_READ;

    ULARGE_INTEGER offsetValue = {};
    offsetValue.QuadPart = static_cast<ULONGLONG>(alignedOffset);

    void* address = MapViewOfFile(
        m_impl->mappingHandle,
        mapAccess,
        offsetValue.HighPart,
        offsetValue.LowPart,
        static_cast<SIZE_T>(mapSize));

    if (address == nullptr)
    {
        outView.m_isOpen = false;
        outView.m_viewSize = 0;
        return false;
    }

    outView.m_address = address;
    outView.m_mapOffset = alignedOffset;
    outView.m_mapSize = mapSize;
    outView.m_viewOffset = viewDelta;

    return true;
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    const long pageSize = sysconf(_SC_PAGE_SIZE);
    const size_t granularity = pageSize > 0 ? static_cast<size_t>(pageSize) : 4096;
    const size_t alignedOffset = (offset / granularity) * granularity;
    const size_t viewDelta = offset - alignedOffset;
    const size_t mapSize = viewDelta + outView.m_viewSize;

    const int prot = PROT_READ | (m_impl->mode == Mode::READ_WRITE ? PROT_WRITE : 0);

    void* address = mmap(nullptr, mapSize, prot, MAP_SHARED, m_impl->fd, static_cast<off_t>(alignedOffset));

    if (address == MAP_FAILED)
    {
        outView.m_isOpen = false;
        outView.m_viewSize = 0;
        outView.m_address = nullptr;
        HYP_BREAKPOINT;
        return false;
    }

    outView.m_address = address;
    outView.m_mapOffset = alignedOffset;
    outView.m_mapSize = mapSize;
    outView.m_viewOffset = viewDelta;

    return true;
#else
    HYP_FAIL("Unsupported platform for memory mapped files");
    return false;
#endif
}

void MemoryMappedFile::Close()
{
    if (!m_impl)
    {
        return;
    }

#ifdef HYP_WINDOWS
    if (m_impl->mappingHandle != nullptr)
    {
        CloseHandle(m_impl->mappingHandle);
        m_impl->mappingHandle = nullptr;
    }

    if (m_impl->fileHandle != nullptr)
    {
        CloseHandle(m_impl->fileHandle);
        m_impl->fileHandle = nullptr;
    }
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    if (m_impl->fd >= 0)
    {
        close(m_impl->fd);
        m_impl->fd = -1;
    }
#endif

    m_impl->fileSize = 0;
}

bool MemoryMappedFile::IsOpen() const
{
    if (!m_impl)
    {
        return false;
    }

#ifdef HYP_WINDOWS
    return m_impl->fileHandle != nullptr;
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    return m_impl->fd >= 0;
#else
    return false;
#endif
}

size_t MemoryMappedFile::FileSize() const
{
    return m_impl ? m_impl->fileSize : 0;
}

MemoryMappedFile::Mode MemoryMappedFile::GetMode() const
{
    return m_impl ? m_impl->mode : Mode::READ_ONLY;
}

void MemoryMappedFile::SetMode(Mode mode)
{
    if (!m_impl)
    {
        m_impl = MakePimpl<MemoryMappedFileImpl>();
    }

    if (mode == m_impl->mode)
    {
        // same mode, do nothing
        return;
    }

    Assert(!IsOpen(), "Cannot change mode of an open memory mapped file. Close the file before changing the mode.");

    m_impl->mode = mode;
}

bool MemoryMappedFile::EnsureCapacity(size_t capacity)
{
    if (!m_impl || !IsOpen())
    {
        return false;
    }

    if (capacity <= m_impl->fileSize)
    {
        return true;
    }

    return Resize(capacity);
}

bool MemoryMappedFile::Resize(size_t newSize)
{
    if (!m_impl || !IsOpen())
    {
        return false;
    }

    if (newSize == m_impl->fileSize)
    {
        return true;
    }

    Assert(newSize <= INT32_MAX);

#ifdef HYP_WINDOWS
    if (m_impl->mappingHandle != nullptr)
    {
        CloseHandle(m_impl->mappingHandle);
        m_impl->mappingHandle = nullptr;
    }

    LARGE_INTEGER distance = {};
    distance.QuadPart = static_cast<LONGLONG>(newSize);

    if (!SetFilePointerEx(m_impl->fileHandle, distance, nullptr, FILE_BEGIN))
    {
        return false;
    }

    if (!SetEndOfFile(m_impl->fileHandle))
    {
        return false;
    }

    m_impl->fileSize = newSize;

    if (m_impl->fileSize == 0)
    {
        return true;
    }

    const DWORD protection = m_impl->mode == Mode::READ_WRITE
        ? PAGE_READWRITE
        : PAGE_READONLY;

    HANDLE mappingHandle = CreateFileMappingA(
        m_impl->fileHandle,
        nullptr,
        protection,
        0,
        0,
        nullptr);

    if (mappingHandle == nullptr)
    {
        return false;
    }

    m_impl->mappingHandle = mappingHandle;
    return true;
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    if (ftruncate(m_impl->fd, static_cast<off_t>(newSize)) != 0)
    {
        return false;
    }

    m_impl->fileSize = newSize;
    return true;
#else
    HYP_FAIL("Unsupported platform for memory mapped files");
    return false;
#endif
}

#pragma endregion MemoryMappedFile

} // namespace Hyperion
