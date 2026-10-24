/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/IO/ByteWriter.hpp>

#include <Core/Utilities/Format.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {
namespace utilities {

/// Copies formatted output that starts at a given offset into a fixed-size buffer.
class FormatBufferWriter final : public ByteWriter
{
public:
    FormatBufferWriter(char* buffer, size_t capacity, size_t offset = 0)
        : m_buffer(buffer),
          m_capacity(capacity),
          m_offset(offset),
          m_position(0)
    {
    }

    virtual size_t Position() const override
    {
        return m_position;
    }

    virtual void Seek(size_t position, bool truncate = false) override
    {
        (void)truncate;

        m_position = position;
    }

    virtual void Close() override
    {
        m_position = 0;
    }

    virtual void Flush() override
    {
        // do nothing
    }

protected:
    virtual void WriteBytes(const char* ptr, size_t size) override
    {
        const size_t copyBegin = MathUtil::Max(m_position, m_offset);
        const size_t copyEnd = MathUtil::Min(m_position + size, m_offset + m_capacity);

        if (copyBegin < copyEnd)
        {
            Memory::Copy(m_buffer + (copyBegin - m_offset), ptr + (copyBegin - m_position), copyEnd - copyBegin);
        }

        m_position += size;
    }

private:
    char* m_buffer;
    size_t m_capacity;
    size_t m_offset;
    size_t m_position;
};

} // namespace utilities
} // namespace Hyperion
