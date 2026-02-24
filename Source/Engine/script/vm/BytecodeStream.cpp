#include <script/vm/BytecodeStream.hpp>

#include <Core/debug/Debug.hpp>

namespace Hyperion {

BytecodeStream BytecodeStream::FromSourceFile(SourceFile& file)
{
    return BytecodeStream(file.GetBuffer(), 0);
}

BytecodeStream::BytecodeStream()
    : m_position(0)
{
}

BytecodeStream::BytecodeStream(const ubyte* buffer, SizeType size, SizeType position)
    : m_byteBuffer(size, buffer),
      m_position(position)
{
}

BytecodeStream::BytecodeStream(const ByteBuffer& byteBuffer, SizeType position)
    : m_byteBuffer(byteBuffer),
      m_position(position)
{
}

BytecodeStream::BytecodeStream(const BytecodeStream& other)
    : m_byteBuffer(other.m_byteBuffer),
      m_position(other.m_position)
{
}

BytecodeStream& BytecodeStream::operator=(const BytecodeStream& other)
{
    m_byteBuffer = other.m_byteBuffer;
    m_position = other.m_position;

    return *this;
}

void BytecodeStream::ReadZeroTerminatedString(char* ptr)
{
    ubyte ch;
    SizeType i = 0;

    const auto* data = m_byteBuffer.Data();

    do
    {
        Assert(m_position < m_byteBuffer.Size(), "Attempted to read past end of buffer!");

        ptr[i++] = char(ch = data[m_position++]);
    }
    while (ch);
}

} // namespace Hyperion
