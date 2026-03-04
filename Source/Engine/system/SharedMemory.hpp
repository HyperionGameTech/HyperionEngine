/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/String.hpp>
#include <Core/Defines.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class HYP_API SharedMemory
{
    using Address = void*;

public:
    enum class Mode
    {
        READ_ONLY,
        READ_WRITE
    };

    SharedMemory(const String& id, size_t size, Mode mode = Mode::READ_ONLY);
    SharedMemory(const SharedMemory& other) = delete;
    SharedMemory& operator=(const SharedMemory& other) = delete;
    SharedMemory(SharedMemory&& other) noexcept;
    SharedMemory& operator=(SharedMemory&& other) noexcept;
    ~SharedMemory();

    const String& Id() const
    {
        return m_id;
    }

    size_t GetSize() const
    {
        return m_size;
    }

    Mode GetMode() const
    {
        return m_mode;
    }

    int GetHandle() const
    {
        return m_handle;
    }

    Address GetAddress() const
    {
        return m_address;
    }

    bool IsOpened() const
    {
        return m_handle != -1;
    }

    /*! \brief Maps the memory. Returns true on success, false on failure. */
    bool Open();

    /*! \brief Unmaps the memory. Returns true on success, false on failure. */
    bool Close();

    /*! \brief Write data into the shared memory. The shared memory must have been constructed with
        READ_WRITE option. */
    void Write(const void* data, size_t count);

private:
    String m_id;
    size_t m_size;
    Mode m_mode;
    int m_handle;
    Address m_address;
};

} // namespace Hyperion
