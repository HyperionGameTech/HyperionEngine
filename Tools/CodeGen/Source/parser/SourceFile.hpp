/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#ifndef HYPERION_CODEGEN_SOURCE_FILE_HPP
#define HYPERION_CODEGEN_SOURCE_FILE_HPP

#include <Core/Types.hpp>
#include <Core/Containers/String.hpp>
#include <Core/Memory/ByteBuffer.hpp>

namespace Hyperion::CodeGen {

class SourceFile
{
public:
    SourceFile();
    SourceFile(const String& filepath, size_t size);
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
    String m_filepath;
    ByteBuffer m_buffer;
    size_t m_position;
};

} // namespace Hyperion::CodeGen

#endif
