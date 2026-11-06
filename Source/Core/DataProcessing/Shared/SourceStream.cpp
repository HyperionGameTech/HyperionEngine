#include <Core/DataProcessing/Shared/SourceStream.hpp>
#include <Core/Debug/Debug.hpp>

#include <cstring>

namespace Hyperion::DataProcessing {

SourceStream::SourceStream(const char* data, size_t size)
    : m_data(data),
      m_size(size),
      m_position(0)
{
}

int SourceStream::DecodeAt(size_t position, utf::Char32& outChar) const
{
    outChar = 0;

    const ubyte firstByte = ubyte(m_data[position]);

    int numContinuationBytes;

    if (firstByte <= 127)
    {
        numContinuationBytes = 0;
    }
    else if ((firstByte & 0xE0) == 0xC0)
    {
        numContinuationBytes = 1;
    }
    else if ((firstByte & 0xF0) == 0xE0)
    {
        numContinuationBytes = 2;
    }
    else if ((firstByte & 0xF8) == 0xF0)
    {
        numContinuationBytes = 3;
    }
    else
    {
        return 1;
    }

    size_t numBytes = size_t(1 + numContinuationBytes);

    if (numBytes > m_size - position)
    {
        numBytes = m_size - position;
    }

    std::memcpy(utf::ToUtf8Chars(outChar), m_data + position, numBytes);

    return int(numBytes);
}

utf::Char32 SourceStream::Peek() const
{
    if (m_position >= m_size)
    {
        return utf::Char32('\0');
    }

    utf::Char32 ch;
    DecodeAt(m_position, ch);

    return ch;
}

utf::Char32 SourceStream::Next()
{
    int tmp;
    return Next(tmp);
}

utf::Char32 SourceStream::Next(int& posChange)
{
    if (m_position >= m_size)
    {
        posChange = 0;

        return utf::Char32('\0');
    }

    utf::Char32 ch;
    posChange = DecodeAt(m_position, ch);
    m_position += size_t(posChange);

    return ch;
}

void SourceStream::GoBack(int n)
{
    HYP_CORE_ASSERT(int(m_position) - n >= 0, "not large enough to go back");

    m_position -= size_t(n);
}

} // namespace Hyperion::DataProcessing
