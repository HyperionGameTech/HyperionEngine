#include <core/system/SharedMemory.hpp>
#include <core/lib/CMemory.hpp>
#include <util/Defines.hpp>
#include <system/Debug.hpp>

#ifdef HYP_UNIX
#include <sys/mman.h>
#include <sys/fcntl.h>
#endif

namespace hyperion {

SharedMemory::SharedMemory(const String &id, SizeType size, Mode mode)
    : m_id(id),
      m_size(size),
      m_mode(mode),
      m_handle(-1),
      m_address(nullptr)
{
}

SharedMemory::SharedMemory(SharedMemory &&other) noexcept
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

SharedMemory &SharedMemory::operator=(SharedMemory &&other) noexcept
{
    if (IsOpened()) {
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
    if (!IsOpened()) {
        return true; // already closed
    }

#ifdef HYP_UNIX
    const int munmap_result = munmap(m_address, m_size);

    m_handle = -1;
    m_address = nullptr;
    m_size = 0;

    if (munmap_result == 0) {
        return true;
    }
#else
    AssertThrow(false, "Unsupported platform for mapped memory, or not yet implemented!");
#endif

    return false;
}

bool SharedMemory::Open()
{
    if (IsOpened()) {
        return true; // already opened
    }

#ifdef HYP_UNIX
    m_handle = shm_open(m_id.Data(), m_mode == Mode::READ_WRITE ? O_RDWR : O_RDONLY, 0666);

    if (m_handle < 0) {
        return false;
    }

    m_address = mmap(nullptr, m_size, PROT_READ | (m_mode == Mode::READ_WRITE ? PROT_WRITE : 0), MAP_SHARED, m_handle, 0);
    
    if (m_address == nullptr) {
        m_handle = -1;
        return false;
    }

    return true;
#else
    AssertThrow(false, "Unsupported platform for mapped memory, or not yet implemented!");

    return false;
#endif
}

void SharedMemory::Write(const void *data, SizeType count)
{
    AssertThrowMsg(m_mode == Mode::READ_WRITE, "SharedMemory was not constructed with READ_WRITE mode enabled");
    AssertThrowMsg(IsOpened(), "SharedMemory not opened!\n");
    AssertThrow(count <= m_size);

    Memory::MemCpy(m_address, data, count);
}

} // namespace hyperion