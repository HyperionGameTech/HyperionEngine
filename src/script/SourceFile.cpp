#include <script/SourceFile.hpp>

#include <core/debug/Debug.hpp>

#include <stdexcept>
#include <cstring>

namespace hyperion {

SourceFile::SourceFile()
    : m_filepath(),
      m_position(0)
{
}

SourceFile::SourceFile(const FilePath& filepath, SizeType size)
    : m_filepath(filepath),
      m_position(0)
{
    m_buffer.SetSize(size);
}

SourceFile::SourceFile(const SourceFile& other)
    : m_filepath(other.m_filepath),
      m_buffer(other.m_buffer.Copy()),
      m_position(other.m_position)
{
}

SourceFile& SourceFile::operator=(const SourceFile& other)
{
    if (&other == this)
    {
        return *this;
    }

    m_buffer = other.m_buffer.Copy();
    m_position = other.m_position;
    m_filepath = other.m_filepath;

    return *this;
}

SourceFile::~SourceFile() = default;

void SourceFile::ReadIntoBuffer(const ByteBuffer& inputBuffer)
{
    Assert(m_buffer.Size() >= inputBuffer.Size());

    // make sure we have enough space in the buffer
    if (m_position + inputBuffer.Size() >= m_buffer.Size())
    {
        Assert("not enough space in buffer");
    }

    for (SizeType i = 0; i < inputBuffer.Size(); i++)
    {
        m_buffer.Data()[m_position++] = inputBuffer.Data()[i];
    }
}

void SourceFile::ReadIntoBuffer(const ubyte* data, SizeType size)
{
    Assert(m_buffer.Size() >= size);

    // make sure we have enough space in the buffer
    if (m_position + size >= m_buffer.Size())
    {
        HYP_FAIL("not enough space in buffer");
    }

    for (SizeType i = 0; i < size; i++)
    {
        m_buffer.Data()[m_position++] = data[i];
    }
}

} // namespace hyperion
