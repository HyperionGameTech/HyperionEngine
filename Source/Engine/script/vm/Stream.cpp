#include <script/vm/Stream.hpp>

#include <Core/debug/Debug.hpp>

namespace Hyperion {

Script_Stream Script_Stream::FromSourceFile(SourceFile& file)
{
    return Script_Stream(file.GetBuffer(), 0);
}

Script_Stream::Script_Stream()
    : m_position(0)
{
}

Script_Stream::Script_Stream(const ubyte* buffer, SizeType size, SizeType position)
    : m_byteBuffer(size, buffer),
      m_position(position)
{
}

Script_Stream::Script_Stream(const ByteBuffer& byteBuffer, SizeType position)
    : m_byteBuffer(byteBuffer),
      m_position(position)
{
}

Script_Stream::Script_Stream(const Script_Stream& other)
    : m_byteBuffer(other.m_byteBuffer),
      m_position(other.m_position)
{
}

Script_Stream& Script_Stream::operator=(const Script_Stream& other)
{
    m_byteBuffer = other.m_byteBuffer;
    m_position = other.m_position;

    return *this;
}

void Script_Stream::ReadZeroTerminatedString(char* ptr)
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
