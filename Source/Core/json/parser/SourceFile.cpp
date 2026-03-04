/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/json/parser/SourceFile.hpp>

#include <Core/debug/Debug.hpp>

#include <cstring>

namespace Hyperion::JSON {

SourceFile::SourceFile()
    : m_filepath("??"),
      m_position(0)
{
}

SourceFile::SourceFile(const String& filepath, size_t size)
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
    HYP_CORE_ASSERT(m_buffer.Size() >= inputBuffer.Size());

    // make sure we have enough space in the buffer
    HYP_CORE_ASSERT(m_position + inputBuffer.Size() <= m_buffer.Size(), "not enough space in buffer");

    for (size_t i = 0; i < inputBuffer.Size(); i++)
    {
        m_buffer.Data()[m_position++] = inputBuffer.Data()[i];
    }
}

void SourceFile::ReadIntoBuffer(const ubyte* data, size_t size)
{
    HYP_CORE_ASSERT(m_buffer.Size() >= size);

    // make sure we have enough space in the buffer
    HYP_CORE_ASSERT(m_position + size <= m_buffer.Size(), "not enough space in buffer");

    for (size_t i = 0; i < size; i++)
    {
        m_buffer.Data()[m_position++] = data[i];
    }
}

} // namespace Hyperion::JSON
