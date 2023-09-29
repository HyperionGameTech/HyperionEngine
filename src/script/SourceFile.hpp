#ifndef SOURCE_FILE_HPP
#define SOURCE_FILE_HPP

#include <Types.hpp>
#include <core/lib/String.hpp>
#include <core/lib/ByteBuffer.hpp>

namespace hyperion {

class SourceFile
{
public:
    SourceFile();
    SourceFile(const String &filepath, SizeType size);
    SourceFile(const SourceFile &other);
    SourceFile &operator=(const SourceFile &other);
    ~SourceFile();

    bool IsValid() const { return !m_buffer.Empty(); }

    const String &GetFilePath() const { return m_filepath; }
    const ByteBuffer &GetBuffer() const { return m_buffer; }
    SizeType GetSize() const { return m_buffer.Size(); }
    void SetSize(SizeType size) { m_buffer.SetSize(size); }
    
    void ReadIntoBuffer(const ByteBuffer &input_buffer);
    void ReadIntoBuffer(const UByte *data, SizeType size);

private:
    String m_filepath;
    ByteBuffer m_buffer;
    SizeType m_position;
};

} // namespace hyperion

#endif
