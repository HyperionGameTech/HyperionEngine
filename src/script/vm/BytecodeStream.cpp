#include <script/vm/BytecodeStream.hpp>

#include <system/Debug.hpp>

namespace hyperion {
namespace vm {

BytecodeStream BytecodeStream::FromSourceFile(SourceFile &file)
{
    return BytecodeStream(file.GetBuffer(), file.GetSize());
}

BytecodeStream::BytecodeStream()
    : m_position(0)
{
}

BytecodeStream::BytecodeStream(const UByte *buffer, SizeType size, SizeType position)
    : m_byte_buffer(size, buffer),
      m_position(position)
{
}

BytecodeStream::BytecodeStream(const ByteBuffer &byte_buffer, SizeType position)
{
    
}

BytecodeStream::BytecodeStream(const BytecodeStream &other)
    : m_byte_buffer(other.m_byte_buffer),
      m_position(other.m_position)
{
}

BytecodeStream &BytecodeStream::operator=(const BytecodeStream &other)
{
    m_byte_buffer = other.m_byte_buffer;
    m_position = other.m_position;

    return *this;
}

void BytecodeStream::ReadZeroTerminatedString(SChar *ptr)
{
    UByte ch;
    SizeType i = 0;

    const auto *data = m_byte_buffer.Data();

    do {
        AssertThrowMsg(m_position < m_byte_buffer.Size(), "Attempted to read past end of buffer!");

        ptr[i++] = SChar(ch = data[m_position++]);
    } while (ch);
}

} // namespace vm
} // namespace hyperion
