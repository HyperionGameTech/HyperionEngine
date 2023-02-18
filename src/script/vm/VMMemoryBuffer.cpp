#include <script/vm/VMMemoryBuffer.hpp>

#include <core/Core.hpp>
#include <sstream>

namespace hyperion {
namespace vm {

VMMemoryBuffer::VMMemoryBuffer(SizeType size)
    : m_size(size),
      m_buffer(static_cast<ByteType *>(std::malloc(size)))
{
}

VMMemoryBuffer::VMMemoryBuffer(const VMMemoryBuffer &other)
    : m_size(other.m_size),
      m_buffer(static_cast<ByteType *>(std::malloc(other.m_size)))
{
    hyperion::Memory::Copy(m_buffer, other.m_buffer, m_size);
}

VMMemoryBuffer &VMMemoryBuffer::operator=(const VMMemoryBuffer &other)
{
    if (&other == this) {
        return *this;
    }

    if (other.m_size != m_size) {
        if (m_buffer) {
            std::free(m_buffer);
        }

        m_size = other.m_size;
        m_buffer = static_cast<ByteType *>(std::malloc(other.m_size));
    }

    // copy all objects
    hyperion::Memory::Copy(m_buffer, other.m_buffer, m_size);

    return *this;
}

VMMemoryBuffer::VMMemoryBuffer(VMMemoryBuffer &&other) noexcept
    : m_size(other.m_size),
      m_buffer(other.m_buffer)
{
    other.m_size = 0;
    other.m_buffer = nullptr;
}

VMMemoryBuffer &VMMemoryBuffer::operator=(VMMemoryBuffer &&other) noexcept
{
    if (&other == this) {
        return *this;
    }

    if (other.m_size != m_size) {
        if (m_buffer) {
            std::free(m_buffer);
        }

        m_size = other.m_size;
        m_buffer = static_cast<ByteType *>(std::malloc(other.m_size));
    }

    // copy all objects
    hyperion::Memory::Copy(m_buffer, other.m_buffer, m_size);

    return *this;
}

VMMemoryBuffer::~VMMemoryBuffer()
{
    if (m_buffer != nullptr) {
        std::free(m_buffer);
    }
}

void VMMemoryBuffer::GetRepresentation(
    std::stringstream &ss,
    bool add_type_name,
    int depth
) const
{
    if (m_buffer == nullptr || depth == 0) {
        ss << "MemoryBuffer(" << std::hex << static_cast<const void *>(m_buffer) << std::dec << ')';

        return;
    }

    // convert all array elements to string
    for (SizeType i = 0; i < m_size; i++) {
        ss << "MemoryBuffer(" << "\\0x" << std::hex << static_cast<UInt16>(m_buffer[i]) << std::dec << ')';
    }
}

} // namespace vm
} // namespace hyperion
