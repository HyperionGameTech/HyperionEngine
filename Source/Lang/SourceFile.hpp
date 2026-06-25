#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Memory/ByteBuffer.hpp>

namespace Hyperion {

class SCRIPT_API SourceFile
{
public:
    SourceFile();
    SourceFile(const FilePath& filepath, size_t size);

    SourceFile(const SourceFile& other);
    SourceFile& operator=(const SourceFile& other);

    ~SourceFile();

    bool IsValid() const
    {
        return !m_buffer.Empty();
    }

    const FilePath& GetFilePath() const
    {
        return m_filepath;
    }

    const ByteBuffer& GetBuffer() const
    {
        return m_buffer;
    }

    size_t GetSize() const
    {
        return m_buffer.Size();
    }

    void SetSize(size_t size)
    {
        m_buffer.SetSize(size);
    }

    void ReadIntoBuffer(const ByteBuffer& inputBuffer);
    void ReadIntoBuffer(const ubyte* data, size_t size);

private:
    FilePath m_filepath;
    ByteBuffer m_buffer;
    size_t m_position;
};

} // namespace Hyperion
