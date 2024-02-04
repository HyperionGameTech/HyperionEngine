#ifndef HYPERION_V2_CORE_SYSTEM_SHARED_MEMORY_HPP
#define HYPERION_V2_CORE_SYSTEM_SHARED_MEMORY_HPP

#include <core/lib/String.hpp>

#include <Types.hpp>

namespace hyperion {

class SharedMemory
{
    using Address = void *;

public:
    enum class Mode
    {
        READ_ONLY,
        READ_WRITE
    };

    SharedMemory(const String &id, SizeType size, Mode mode = Mode::READ_ONLY);
    SharedMemory(const SharedMemory &other)             = delete;
    SharedMemory &operator=(const SharedMemory &other)  = delete;
    SharedMemory(SharedMemory &&other) noexcept;
    SharedMemory &operator=(SharedMemory &&other) noexcept;
    ~SharedMemory();

    const String &GetID() const
        { return m_id; }

    SizeType GetSize() const
        { return m_size; }

    Mode GetMode() const
        { return m_mode; }

    Int GetHandle() const
        { return m_handle; }

    Address GetAddress() const
        { return m_address; }

    bool IsOpened() const
        { return m_handle != -1; }

    /*! \brief Maps the memory. Returns true on success, false on failure. */
    bool Open();

    /*! \brief Unmaps the memory. Returns true on success, false on failure. */
    bool Close();

    /*! \brief Write data into the shared memory. The shared memory must have been constructed with
        READ_WRITE option. */
    void Write(const void *data, SizeType count);

private:
    String      m_id;
    SizeType    m_size;
    Mode        m_mode;
    Int         m_handle;
    Address     m_address;
};

} // namespace hyperion

#endif