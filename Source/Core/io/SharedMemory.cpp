/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/io/SharedMemory.hpp>

#include <Core/debug/Debug.hpp>

#include <Core/memory/Memory.hpp>

#if HYP_UNIX && !HYP_ANDROID
#include <sys/mman.h>
#include <sys/fcntl.h>
#endif

namespace Hyperion {

SharedMemory::SharedMemory(const String& id, size_t size, Mode mode)
    : m_id(id),
      m_size(size),
      m_mode(mode),
      m_handle(-1),
      m_address(nullptr)
{
}

SharedMemory::SharedMemory(SharedMemory&& other) noexcept
    : m_id(std::move(other.m_id)),
      m_size(other.m_size),
      m_mode(other.m_mode),
      m_handle(other.m_handle),
      m_address(other.m_address)
{
    other.m_handle = -1;
    other.m_size = 0;
    other.m_address = nullptr;
}

SharedMemory& SharedMemory::operator=(SharedMemory&& other) noexcept
{
    if (IsOpened())
    {
        Close();
    }

    m_id = std::move(other.m_id);
    m_size = other.m_size;
    m_mode = other.m_mode;
    m_handle = other.m_handle;
    m_address = other.m_address;

    other.m_handle = -1;
    other.m_size = 0;
    other.m_address = nullptr;

    return *this;
}

SharedMemory::~SharedMemory()
{
    Close();
}

bool SharedMemory::Close()
{
    if (!IsOpened())
    {
        return true; // already closed
    }

#if HYP_UNIX && !HYP_ANDROID
    const int munmapResult = munmap(m_address, m_size);

    m_handle = -1;
    m_address = nullptr;
    m_size = 0;

    if (munmapResult == 0)
    {
        return true;
    }
#else
    HYP_FAIL("Unsupported platform for mapped memory, or not yet implemented!");
#endif

    return false;
}

bool SharedMemory::Open()
{
    if (IsOpened())
    {
        return true; // already opened
    }

#if HYP_UNIX && !HYP_ANDROID
    m_handle = shm_open(m_id.Data(), m_mode == Mode::READ_WRITE ? O_RDWR : O_RDONLY, 0666);

    if (m_handle < 0)
    {
        return false;
    }

    m_address = mmap(nullptr, m_size, PROT_READ | (m_mode == Mode::READ_WRITE ? PROT_WRITE : 0), MAP_SHARED, m_handle, 0);

    if (m_address == nullptr)
    {
        m_handle = -1;
        return false;
    }

    return true;
#else
    HYP_FAIL("Unsupported platform for mapped memory, or not yet implemented!");

    return false;
#endif
}

void SharedMemory::Write(const void* data, size_t count)
{
    Assert(m_mode == Mode::READ_WRITE, "SharedMemory was not constructed with READ_WRITE mode enabled");
    Assert(IsOpened(), "SharedMemory not opened!\n");
    Assert(count <= m_size);

    Memory::Copy(m_address, data, count);
}

} // namespace Hyperion
