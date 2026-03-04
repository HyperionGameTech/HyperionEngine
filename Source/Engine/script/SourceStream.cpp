#include <script/SourceStream.hpp>

#include <stdexcept>

namespace Hyperion {

SourceStream::SourceStream(SourceFile* file)
    : m_file(file),
      m_position(0)
{
}

SourceStream::SourceStream(const SourceStream& other)
    : m_file(other.m_file),
      m_position(other.m_position)
{
}

utf::Char32 SourceStream::Peek() const
{
    size_t pos = m_position;
    if (pos >= m_file->GetSize())
    {
        return '\0';
    }

    // the current character
    char ch = m_file->GetBuffer()[pos];

    // the character as a utf-32 character
    union
    {
        utf::Char32 u32Ch;
        utf::Char8 bytes[sizeof(utf::Char32)];
    };

    u32Ch = 0;

    // check to see if it is a utf-8 character
    const unsigned char uc = (unsigned char)ch;

    if (uc >= 0 && uc <= 127)
    {
        // 1-byte character
        bytes[0] = ch;
    }
    else if ((uc & 0xE0) == 0xC0)
    {
        // 2-byte character
        bytes[0] = ch;
        bytes[1] = m_file->GetBuffer()[pos + 1];
    }
    else if ((uc & 0xF0) == 0xE0)
    {
        // 3-byte character
        bytes[0] = ch;
        bytes[1] = m_file->GetBuffer()[pos + 1];
        bytes[2] = m_file->GetBuffer()[pos + 2];
    }
    else if ((uc & 0xF8) == 0xF0)
    {
        // 4-byte character
        bytes[0] = ch;
        bytes[1] = m_file->GetBuffer()[pos + 1];
        bytes[2] = m_file->GetBuffer()[pos + 2];
        bytes[3] = m_file->GetBuffer()[pos + 3];
    }
    else
    {
        // invalid utf-8
        u32Ch = (utf::Char32)('\0');
    }

    return u32Ch;
}

utf::Char32 SourceStream::Next()
{
    int tmp;
    return Next(tmp);
}

utf::Char32 SourceStream::Next(int& posChange)
{
    int posBefore = m_position;

    if (m_position >= m_file->GetSize())
    {
        return (utf::Char32)('\0');
    }

    // the current character
    char ch = m_file->GetBuffer()[m_position++];

    // the character as a utf-32 character
    union
    {
        utf::Char32 u32Ch;
        utf::Char8 bytes[sizeof(utf::Char32)];
    };

    u32Ch = 0;

    // check to see if it is a utf-8 character
    const unsigned char uc = (unsigned char)ch;

    if (uc >= 0 && uc <= 127)
    {
        // 1-byte character
        bytes[0] = ch;
    }
    else if ((uc & 0xE0) == 0xC0)
    {
        // 2-byte character
        bytes[0] = ch;
        bytes[1] = m_file->GetBuffer()[m_position++];
    }
    else if ((uc & 0xF0) == 0xE0)
    {
        // 3-byte character
        bytes[0] = ch;
        bytes[1] = m_file->GetBuffer()[m_position++];
        bytes[2] = m_file->GetBuffer()[m_position++];
    }
    else if ((uc & 0xF8) == 0xF0)
    {
        // 4-byte character
        bytes[0] = ch;
        bytes[1] = m_file->GetBuffer()[m_position++];
        bytes[2] = m_file->GetBuffer()[m_position++];
        bytes[3] = m_file->GetBuffer()[m_position++];
    }
    else
    {
        // invalid utf-8
        u32Ch = (utf::Char32)('\0');
    }

    posChange = m_position - posBefore;

    return u32Ch;
}

void SourceStream::GoBack(int n)
{
    if (((int)m_position - n) < 0)
    {
        HYP_FAIL("not large enough to go back");
    }
    m_position -= n;
}

void SourceStream::Read(char* ptr, size_t numBytes)
{
    for (size_t i = 0; i < numBytes; i++)
    {
        if (m_position >= m_file->GetSize())
        {
            HYP_FAIL("attempted to read past the limit");
        }

        ptr[i] = m_file->GetBuffer()[m_position++];
    }
}

} // namespace Hyperion
