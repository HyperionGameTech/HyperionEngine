/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/io/MemoryMappedFile.hpp>

#include <core/logging/Logger.hpp>

#include <core/debug/Debug.hpp>
#include <core/Defines.hpp>

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
    SizeType file_size;

#ifdef HYP_WINDOWS
    HANDLE file_handle;
    HANDLE mapping_handle;
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    int fd;
#endif

    MemoryMappedFileImpl()
        : mode(MemoryMappedFile::Mode::READ_ONLY),
            file_size(0)
    {
#ifdef HYP_WINDOWS
        file_handle = nullptr;
        mapping_handle = nullptr;
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
        fd = -1;
#endif
    }
};

struct MemoryMappedFileViewImpl
{
    SizeType map_offset;
    SizeType map_size;
    SizeType view_offset;
    SizeType view_size;
    SizeType file_offset;
    void* address;
    bool is_open;

    MemoryMappedFileViewImpl()
        : map_offset(0),
          map_size(0),
          view_offset(0),
          view_size(0),
          file_offset(0),
          address(nullptr),
          is_open(false)
    {
    }
};

MemoryMappedFileView::MemoryMappedFileView()
    : m_impl(MakePimpl<MemoryMappedFileViewImpl>())
{
}

MemoryMappedFileView::~MemoryMappedFileView()
{
    Close();
}

bool MemoryMappedFileView::IsOpen() const
{
    return m_impl && m_impl->is_open;
}

bool MemoryMappedFileView::Reopen(const MemoryMappedFile& file, SizeType offset, SizeType size)
{
    if (IsOpen())
    {
        Close();
    }

    return file.MapRange(*this, offset, size);
}

void MemoryMappedFileView::Close()
{
    if (!m_impl || !m_impl->is_open)
    {
        return;
    }

#ifdef HYP_WINDOWS
    if (m_impl->address != nullptr)
    {
        UnmapViewOfFile(m_impl->address);
    }
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    if (m_impl->address != nullptr && m_impl->map_size > 0)
    {
        munmap(m_impl->address, m_impl->map_size);
    }
#endif

    m_impl->map_offset = 0;
    m_impl->map_size = 0;
    m_impl->view_offset = 0;
    m_impl->view_size = 0;
    m_impl->file_offset = 0;
    m_impl->address = nullptr;
    m_impl->is_open = false;
}

SizeType MemoryMappedFileView::Size() const
{
    return m_impl ? m_impl->view_size : 0;
}

SizeType MemoryMappedFileView::FileOffset() const
{
    return m_impl ? m_impl->file_offset : 0;
}

void* MemoryMappedFileView::Data()
{
    if (!m_impl || !m_impl->is_open || m_impl->address == nullptr)
    {
        return nullptr;
    }

    return static_cast<ubyte*>(m_impl->address) + m_impl->view_offset;
}

const void* MemoryMappedFileView::Data() const
{
    if (!m_impl || !m_impl->is_open || m_impl->address == nullptr)
    {
        return nullptr;
    }

    return static_cast<const ubyte*>(m_impl->address) + m_impl->view_offset;
}

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

    HANDLE file_handle = CreateFileW(
        wide_path.Data(),
        desired_access,
        share_mode,
        nullptr,
        m_impl->mode == Mode::READ_ONLY ? OPEN_EXISTING : CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file_handle == INVALID_HANDLE_VALUE)
    {
        DWORD errorCode = GetLastError();

        HYP_LOG(Core, Error, "Failed to open file at path {}, Error code was: ({})",\
            m_impl->filepath, errorCode);

        return false;
    }

    LARGE_INTEGER file_size = {};

    if (!GetFileSizeEx(file_handle, &file_size))
    {
        CloseHandle(file_handle);
        return false;
    }

    m_impl->file_size = static_cast<SizeType>(file_size.QuadPart);
    m_impl->file_handle = file_handle;

    if (m_impl->file_size == 0)
    {
        m_impl->mapping_handle = nullptr;
        return true;
    }

    const DWORD protection = m_impl->mode == Mode::READ_WRITE
        ? PAGE_READWRITE
        : PAGE_READONLY;

    HANDLE mapping_handle = CreateFileMappingW(
        file_handle,
        nullptr,
        protection,
        0,
        0,
        nullptr);

    if (mapping_handle == nullptr)
    {
        CloseHandle(file_handle);
        m_impl->file_handle = nullptr;
        return false;
    }

    m_impl->mapping_handle = mapping_handle;

    return true;
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    const int open_flags = m_impl->mode == Mode::READ_WRITE
        ? O_RDWR
        : O_RDONLY;

    const int fd = open(m_impl->filepath.Data(), open_flags);

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

    m_impl->file_size = static_cast<SizeType>(st.st_size);
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

bool MemoryMappedFile::MapRange(MemoryMappedFileView& out_view, SizeType offset, SizeType size) const
{
    if (!m_impl || !IsOpen())
    {
        return false;
    }

    HYP_CORE_ASSERT(!out_view.IsOpen(), "MemoryMappedFileView is already open");

    if (!out_view.m_impl)
    {
        out_view.m_impl = MakePimpl<MemoryMappedFileViewImpl>();
    }

    if (m_impl->mode == Mode::READ_WRITE && size > 0)
    {
        const SizeType end_offset = offset + size;

        if (end_offset < offset)
        {
            return false;
        }

        if (end_offset > m_impl->file_size)
        {
            auto* self = const_cast<MemoryMappedFile*>(this);

            if (!self->Resize(end_offset))
            {
                return false;
            }
        }
    }

    if (m_impl->mode != Mode::READ_WRITE || size == 0)
    {
        if (offset > m_impl->file_size)
        {
            offset = m_impl->file_size;
        }
    }

    if (size == 0)
    {
        size = m_impl->file_size > offset ? (m_impl->file_size - offset) : 0;
    }
    else if (m_impl->mode != Mode::READ_WRITE && offset + size > m_impl->file_size)
    {
        size = m_impl->file_size - offset;
    }

    out_view.m_impl->file_offset = offset;
    out_view.m_impl->view_size = size;
    out_view.m_impl->view_offset = 0;
    out_view.m_impl->map_offset = 0;
    out_view.m_impl->map_size = 0;
    out_view.m_impl->address = nullptr;
    out_view.m_impl->is_open = true;

    if (out_view.m_impl->view_size == 0)
    {
        return true;
    }

#ifdef HYP_WINDOWS
    if (m_impl->mapping_handle == nullptr)
    {
        return false;
    }

    SYSTEM_INFO system_info = {};
    GetSystemInfo(&system_info);

    const SizeType granularity = static_cast<SizeType>(system_info.dwAllocationGranularity);
    const SizeType aligned_offset = (offset / granularity) * granularity;
    const SizeType view_delta = offset - aligned_offset;
    const SizeType map_size = view_delta + out_view.m_impl->view_size;

    const DWORD map_access = m_impl->mode == Mode::READ_WRITE
        ? (FILE_MAP_READ | FILE_MAP_WRITE)
        : FILE_MAP_READ;

    ULARGE_INTEGER offset_value = {};
    offset_value.QuadPart = static_cast<ULONGLONG>(aligned_offset);

    void* address = MapViewOfFile(
        m_impl->mapping_handle,
        map_access,
        offset_value.HighPart,
        offset_value.LowPart,
        static_cast<SIZE_T>(map_size));

    if (address == nullptr)
    {
        out_view.m_impl->is_open = false;
        out_view.m_impl->view_size = 0;
        return false;
    }

    out_view.m_impl->address = address;
    out_view.m_impl->map_offset = aligned_offset;
    out_view.m_impl->map_size = map_size;
    out_view.m_impl->view_offset = view_delta;

    return true;
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    const long page_size = sysconf(_SC_PAGE_SIZE);
    const SizeType granularity = page_size > 0 ? static_cast<SizeType>(page_size) : 4096;
    const SizeType aligned_offset = (offset / granularity) * granularity;
    const SizeType view_delta = offset - aligned_offset;
    const SizeType map_size = view_delta + out_view.m_impl->view_size;

    const int prot = PROT_READ | (m_impl->mode == Mode::READ_WRITE ? PROT_WRITE : 0);

    void* address = mmap(nullptr, map_size, prot, MAP_SHARED, m_impl->fd, static_cast<off_t>(aligned_offset));

    if (address == MAP_FAILED)
    {
        out_view.m_impl->is_open = false;
        out_view.m_impl->view_size = 0;
        out_view.m_impl->address = nullptr;
        return false;
    }

    out_view.m_impl->address = address;
    out_view.m_impl->map_offset = aligned_offset;
    out_view.m_impl->map_size = map_size;
    out_view.m_impl->view_offset = view_delta;

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
    if (m_impl->mapping_handle != nullptr)
    {
        CloseHandle(m_impl->mapping_handle);
        m_impl->mapping_handle = nullptr;
    }

    if (m_impl->file_handle != nullptr)
    {
        CloseHandle(m_impl->file_handle);
        m_impl->file_handle = nullptr;
    }
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    if (m_impl->fd >= 0)
    {
        close(m_impl->fd);
        m_impl->fd = -1;
    }
#endif

    m_impl->file_size = 0;
}

bool MemoryMappedFile::IsOpen() const
{
    if (!m_impl)
    {
        return false;
    }

#ifdef HYP_WINDOWS
    return m_impl->file_handle != nullptr;
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    return m_impl->fd >= 0;
#else
    return false;
#endif
}

SizeType MemoryMappedFile::FileSize() const
{
    return m_impl ? m_impl->file_size : 0;
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

bool MemoryMappedFile::Resize(SizeType new_size)
{
    if (!m_impl || !IsOpen())
    {
        return false;
    }

    if (new_size == m_impl->file_size)
    {
        return true;
    }

    Assert(new_size <= INT32_MAX);

#ifdef HYP_WINDOWS
    LARGE_INTEGER distance = {};
    distance.QuadPart = static_cast<LONGLONG>(new_size);

    if (!SetFilePointerEx(m_impl->file_handle, distance, nullptr, FILE_BEGIN))
    {
        return false;
    }

    if (!SetEndOfFile(m_impl->file_handle))
    {
        return false;
    }

    m_impl->file_size = new_size;

    if (m_impl->mapping_handle != nullptr)
    {
        CloseHandle(m_impl->mapping_handle);
        m_impl->mapping_handle = nullptr;
    }

    if (m_impl->file_size == 0)
    {
        return true;
    }

    const DWORD protection = m_impl->mode == Mode::READ_WRITE
        ? PAGE_READWRITE
        : PAGE_READONLY;

    HANDLE mapping_handle = CreateFileMappingA(
        m_impl->file_handle,
        nullptr,
        protection,
        0,
        0,
        nullptr);

    if (mapping_handle == nullptr)
    {
        return false;
    }

    m_impl->mapping_handle = mapping_handle;
    return true;
#elif defined(HYP_LINUX) || defined(HYP_MACOS)
    if (ftruncate(m_impl->fd, static_cast<off_t>(new_size)) != 0)
    {
        return false;
    }

    m_impl->file_size = new_size;
    return true;
#else
    HYP_FAIL("Unsupported platform for memory mapped files");
    return false;
#endif
}

} // namespace Hyperion
