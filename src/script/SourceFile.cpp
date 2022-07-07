#include <script/SourceFile.hpp>

#include <system/Debug.hpp>

#include <stdexcept>
#include <cstring>

namespace hyperion {

SourceFile::SourceFile()
    : m_filepath("??"),
      m_position(0),
      m_size(0),
      m_buffer(nullptr)
{
}

SourceFile::SourceFile(const std::string &filepath, size_t size)
    : m_filepath(filepath),
      m_position(0),
      m_size(size)
{
    m_buffer = new char[m_size];
    std::memset(m_buffer, '\0', m_size);
}

SourceFile::SourceFile(const SourceFile &other)
    : m_filepath(other.m_filepath),
      m_position(other.m_position),
      m_size(other.m_size)
{
    m_buffer = new char[m_size];

    if (m_size != 0) {
        std::memcpy(m_buffer, other.m_buffer, m_size);
    }
}

SourceFile &SourceFile::operator=(const SourceFile &other)
{
    if (&other == this) {
        return *this;
    }

    if (m_size != other.m_size) {
        if (m_buffer != nullptr) {
            delete[] m_buffer;
            m_buffer = nullptr;
        }

        if (other.m_size != 0) {
            m_buffer = new char[other.m_size];
        }
    }

    m_size = other.m_size;
    m_position = other.m_position;
    m_filepath = other.m_filepath;

    if (m_buffer != nullptr) {
        std::memcpy(m_buffer, other.m_buffer, m_size);
    }

    return *this;
}

SourceFile::~SourceFile()
{
    if (m_buffer != nullptr) {
        delete[] m_buffer;
    }
}

SourceFile &SourceFile::operator>>(const std::string &str)
{
    AssertThrowMsg(m_buffer != nullptr, "Not allocated");

    size_t length = str.length();
    // make sure we have enough space in the buffer
    if (m_position + length >= m_size) {
        throw std::out_of_range("not enough space in buffer");
    }

    for (size_t i = 0; i < length; i++) {
        m_buffer[m_position++] = str[i];
    }

    return *this;
}

void SourceFile::ReadIntoBuffer(const char *data, size_t size)
{
    AssertThrowMsg(m_buffer != nullptr, "Not allocated");
    AssertThrow(m_size >= size);
    std::memcpy(m_buffer, data, size);
}

} // namespace hyperion
