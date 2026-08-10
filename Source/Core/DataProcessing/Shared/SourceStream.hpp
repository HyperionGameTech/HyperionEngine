#pragma once

#include <Core/Containers/String.hpp>
#include <Core/Unicode.hpp>

namespace Hyperion {
class ByteReader;
} // namespace Hyperion

namespace Hyperion::DataProcessing {

class SourceStream
{
public:
    SourceStream(ByteReader* reader, const String& filepath = String::empty);
    SourceStream(const SourceStream& other);

    ByteReader* GetReader() const { return m_reader; }
    const String& GetFilePath() const { return m_filepath; }
    size_t GetPosition() const;
    bool HasNext() const;

    utf::Char32 Peek() const;
    utf::Char32 Next();
    utf::Char32 Next(int& posChange);
    void GoBack(int n = 1);
    void Read(char* ptr, size_t numBytes);

private:
    ByteReader* m_reader;
    String m_filepath;
};

} // namespace Hyperion::DataProcessing
