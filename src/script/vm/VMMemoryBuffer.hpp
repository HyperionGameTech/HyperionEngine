#ifndef MEMORY_BUFFER_HPP
#define  MEMORY_BUFFER_HPP

#include <core/lib/ByteBuffer.hpp>

#include <system/Debug.hpp>
#include <Types.hpp>

#include <sstream>

namespace hyperion {
namespace vm {

class VMMemoryBuffer
{
public:
    using ByteType = UByte;

    VMMemoryBuffer(SizeType size = 0);
    VMMemoryBuffer(const ByteBuffer &bytes);
    VMMemoryBuffer(const VMMemoryBuffer &other)                 = default;
    VMMemoryBuffer &operator=(const VMMemoryBuffer &other)      = default;
    VMMemoryBuffer(VMMemoryBuffer &&other) noexcept             = default;
    VMMemoryBuffer &operator=(VMMemoryBuffer &&other) noexcept  = default;
    ~VMMemoryBuffer()                                           = default;

    bool operator==(const VMMemoryBuffer &other) const
        { return this == &other; }

    SizeType GetSize() const
        { return m_bytes.Size(); }

    void *GetBuffer()
        { return m_bytes.Data(); }

    const void *GetBuffer() const
        { return m_bytes.Data(); }

    const ByteBuffer &GetByteBuffer() const
        { return m_bytes; }

    void GetRepresentation(
        std::stringstream &ss,
        bool add_type_name = true,
        int depth = 3
    ) const;

private:
    ByteBuffer m_bytes;
};

} // namespace vm
} // namespace hyperion

#endif
