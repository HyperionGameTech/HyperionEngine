#include <script/vm/BytecodeStream.hpp>

#include <system/debug.h>

namespace hyperion {
namespace vm {

BytecodeStream BytecodeStream::FromSourceFile(SourceFile *file_ptr)
{
    AssertThrow(file_ptr != nullptr);
    return BytecodeStream(file_ptr->GetBuffer(), file_ptr->GetSize());
}

BytecodeStream::BytecodeStream()
    : m_buffer(nullptr),
      m_size(0),
      m_position(0)
{
}

BytecodeStream::BytecodeStream(const char *buffer, size_t size, size_t position)
    : m_buffer(buffer),
      m_size(size),
      m_position(position)
{
}

BytecodeStream::BytecodeStream(const BytecodeStream &other)
    : m_buffer(other.m_buffer),
      m_size(other.m_size),
      m_position(other.m_position)
{
}

BytecodeStream &BytecodeStream::operator=(const BytecodeStream &other)
{
    m_buffer = other.m_buffer;
    m_size = other.m_size;
    m_position = other.m_position;

    return *this;
}

void BytecodeStream::ReadZeroTerminatedString(char *ptr)
{
    char ch = 0;
    size_t i = 0;

    do {
        if (m_position >= m_size) {
            throw std::out_of_range("attempted to read past the limit");
        }

        ptr[i++] = ch = m_buffer[m_position++];

    } while (ch != 0);
}

} // namespace vm
} // namespace hyperion
