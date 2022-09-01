#ifndef MEMORY_BUFFER_HPP
#define  MEMORY_BUFFER_HPP

#include <system/Debug.hpp>

#include <sstream>

namespace hyperion {
namespace vm {

class MemoryBuffer {
public:
    MemoryBuffer(size_t size = 0);
    MemoryBuffer(const MemoryBuffer &other);
    ~MemoryBuffer();

    MemoryBuffer &operator=(const MemoryBuffer &other);
    bool operator==(const MemoryBuffer &other) const { return this == &other; }

    size_t GetSize() const { return m_size; }
    void *GetBuffer() const { return m_buffer; }

    void GetRepresentation(
        std::stringstream &ss,
        bool add_type_name = true,
        int depth = 3
    ) const;

private:
    size_t m_size;
    void *m_buffer;
};

} // namespace vm
} // namespace hyperion

#endif
