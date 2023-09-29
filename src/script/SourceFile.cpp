#include <script/SourceFile.hpp>

#include <system/Debug.hpp>

#include <stdexcept>
#include <cstring>

namespace hyperion {

SourceFile::SourceFile()
    : m_filepath("??"),
      m_position(0)
{
}

SourceFile::SourceFile(const String &filepath, SizeType size)
    : m_filepath(filepath),
      m_position(0)
{
    m_buffer.SetSize(size);
}

SourceFile::SourceFile(const SourceFile &other)
    : m_filepath(other.m_filepath),
      m_buffer(other.m_buffer.Copy()),
      m_position(other.m_position)
{
}

SourceFile &SourceFile::operator=(const SourceFile &other)
{
    if (&other == this) {
        return *this;
    }

    m_buffer = other.m_buffer.Copy();
    m_position = other.m_position;
    m_filepath = other.m_filepath;

    return *this;
}

SourceFile::~SourceFile() = default;

void SourceFile::ReadIntoBuffer(const ByteBuffer &input_buffer)
{
    AssertThrow(m_buffer.Size() >= input_buffer.Size());

    if (auto buffer = m_buffer.Lock()) {
        // make sure we have enough space in the buffer
        if (m_position + input_buffer.Size() >= buffer.Size()) {
            AssertThrow("not enough space in buffer");
        }

        for (SizeType i = 0; i < input_buffer.Size(); i++) {
            buffer.Data()[m_position++] = input_buffer.Data()[i];
        }

        m_buffer = buffer;
    }
}

void SourceFile::ReadIntoBuffer(const UByte *data, SizeType size)
{
    AssertThrow(m_buffer.Size() >= size);

    if (auto buffer = m_buffer.Lock()) {
        // make sure we have enough space in the buffer
        if (m_position + size >= buffer.Size()) {
            AssertThrow("not enough space in buffer");
        }

        for (SizeType i = 0; i < size; i++) {
            buffer.Data()[m_position++] = data[i];
        }

        m_buffer = buffer;
    }
}

} // namespace hyperion
