#include <Core/DataProcessing/Shared/SourceStream.hpp>
#include <Core/IO/ByteReader.hpp>
#include <Core/Debug/Debug.hpp>

namespace Hyperion::DataProcessing {

SourceStream::SourceStream(ByteReader* reader, const String& filepath)
    : m_reader(reader),
      m_filepath(filepath)
{
    HYP_CORE_ASSERT(m_reader != nullptr);
}

SourceStream::SourceStream(const SourceStream& other)
    : m_reader(other.m_reader),
      m_filepath(other.m_filepath)
{
}

size_t SourceStream::GetPosition() const
{
    return m_reader->Position();
}

bool SourceStream::HasNext() const
{
    return !m_reader->Eof();
}

utf::Char32 SourceStream::Peek() const
{
    const size_t startPos = m_reader->Position();

    if (startPos >= m_reader->Max())
    {
        return utf::Char32('\0');
    }

    ubyte firstByte;
    m_reader->Read(&firstByte, 1);

    utf::Char32 u32Ch = 0;
    char* bytes = utf::ToUtf8Chars(u32Ch);
    bytes[0] = char(firstByte);

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
        m_reader->Seek(startPos);

        return utf::Char32('\0');
    }

    if (numContinuationBytes > 0)
    {
        m_reader->Read(bytes + 1, size_t(numContinuationBytes));
    }

    m_reader->Seek(startPos);

    return u32Ch;
}

utf::Char32 SourceStream::Next()
{
    int tmp;
    return Next(tmp);
}

utf::Char32 SourceStream::Next(int& posChange)
{
    const size_t posBefore = m_reader->Position();

    if (posBefore >= m_reader->Max())
    {
        return utf::Char32('\0');
    }

    ubyte firstByte;
    m_reader->Read(&firstByte, 1);

    utf::Char32 u32Ch = 0;
    char* bytes = utf::ToUtf8Chars(u32Ch);
    bytes[0] = char(firstByte);

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
        numContinuationBytes = 0;
        u32Ch = utf::Char32('\0');
    }

    if (numContinuationBytes > 0)
    {
        m_reader->Read(bytes + 1, size_t(numContinuationBytes));
    }

    posChange = int(m_reader->Position() - posBefore);

    return u32Ch;
}

void SourceStream::GoBack(int n)
{
    HYP_CORE_ASSERT((int(m_reader->Position()) - n) >= 0, "not large enough to go back");

    m_reader->Rewind(size_t(n));
}

void SourceStream::Read(char* ptr, size_t numBytes)
{
    HYP_CORE_ASSERT(m_reader->Position() + numBytes < m_reader->Max(), "attempted to read past the limit");

    m_reader->Read(static_cast<void*>(ptr), numBytes);
}

} // namespace Hyperion::DataProcessing
