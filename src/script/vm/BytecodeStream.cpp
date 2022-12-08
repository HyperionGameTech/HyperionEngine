#include <script/vm/BytecodeStream.hpp>

#include <system/Debug.hpp>

namespace hyperion {
namespace vm {

BytecodeStream BytecodeStream::FromSourceFile(SourceFile &file)
{
    return BytecodeStream(file.GetBuffer(), file.GetSize());
}

BytecodeStream::BytecodeStream()
    : m_buffer(nullptr),
      m_size(0),
      m_position(0)
{
}

BytecodeStream::BytecodeStream(const UByte *buffer, SizeType size, SizeType position)
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

void BytecodeStream::ReadZeroTerminatedString(SChar *ptr)
{
    UByte ch = 0;
    SizeType i = 0;

    do {
        AssertThrowMsg(m_position < m_size, "Attempted to read past end of buffer!");

        ptr[i++] = SChar(ch = m_buffer[m_position++]);
    } while (ch);
}

} // namespace vm
} // namespace hyperion
