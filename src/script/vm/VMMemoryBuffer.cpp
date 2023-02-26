#include <script/vm/VMMemoryBuffer.hpp>

#include <core/Core.hpp>
#include <sstream>

namespace hyperion {
namespace vm {

VMMemoryBuffer::VMMemoryBuffer(SizeType size)
    : m_bytes(size)
{
}

VMMemoryBuffer::VMMemoryBuffer(const ByteBuffer &bytes)
    : m_bytes(bytes)
{
}

void VMMemoryBuffer::GetRepresentation(
    std::stringstream &ss,
    bool add_type_name,
    int depth
) const
{
    if (depth == 0) {
        ss << "MemoryBuffer(" << ((const void *)m_bytes.Data()) << ")\n";

        return;
    }

    // convert all array elements to string
    for (SizeType i = 0; i < m_bytes.Size(); i++) {
        ss << "\\0x" << std::hex << static_cast<UInt16>(m_bytes.Data()[i]) << std::dec;
    }
}

} // namespace vm
} // namespace hyperion
