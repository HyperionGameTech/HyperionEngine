#ifndef MEMORY_BUFFER_HPP
#define  MEMORY_BUFFER_HPP

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
    VMMemoryBuffer(const VMMemoryBuffer &other);
    VMMemoryBuffer &operator=(const VMMemoryBuffer &other);
    VMMemoryBuffer(VMMemoryBuffer &&other) noexcept;
    VMMemoryBuffer &operator=(VMMemoryBuffer &&other) noexcept;
    ~VMMemoryBuffer();

    bool operator==(const VMMemoryBuffer &other) const { return this == &other; }

    SizeType GetSize() const { return m_size; }
    ByteType *GetBuffer() const { return m_buffer; }

    void GetRepresentation(
        std::stringstream &ss,
        bool add_type_name = true,
        int depth = 3
    ) const;

private:
    SizeType m_size;
    ByteType *m_buffer;
};

} // namespace vm
} // namespace hyperion

#endif
