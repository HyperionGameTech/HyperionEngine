/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Types.hpp>
#include <Core/containers/String.hpp>
#include <Core/memory/ByteBuffer.hpp>

namespace Hyperion::JSON {

class SourceFile
{
public:
    SourceFile();
    SourceFile(const String& filepath, SizeType size);
    SourceFile(const SourceFile& other);
    SourceFile& operator=(const SourceFile& other);
    ~SourceFile();

    bool IsValid() const
    {
        return !m_buffer.Empty();
    }

    const String& GetFilePath() const
    {
        return m_filepath;
    }

    const ByteBuffer& GetBuffer() const
    {
        return m_buffer;
    }

    SizeType GetSize() const
    {
        return m_buffer.Size();
    }

    void SetSize(SizeType size)
    {
        m_buffer.SetSize(size);
    }

    void ReadIntoBuffer(const ByteBuffer& inputBuffer);
    void ReadIntoBuffer(const ubyte* data, SizeType size);

private:
    String m_filepath;
    ByteBuffer m_buffer;
    SizeType m_position;
};

} // namespace Hyperion::JSON
