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
    VMMemoryBuffer(SizeType size = 0);
    VMMemoryBuffer(const ByteBuffer &bytes);
    VMMemoryBuffer(const VMMemoryBuffer &other);
    ~VMMemoryBuffer();

    VMMemoryBuffer &operator=(const VMMemoryBuffer &other);
    bool operator==(const VMMemoryBuffer &other) const { return this == &other; }

    SizeType GetSize() const { return m_bytes.Size(); }

    void *GetBuffer() { return m_bytes.Data(); }
    const void *GetBuffer() const { return m_bytes.Data(); }

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
