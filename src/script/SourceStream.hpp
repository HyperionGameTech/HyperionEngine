#ifndef SOURCE_STREAM_HPP
#define SOURCE_STREAM_HPP

#include <script/SourceFile.hpp>
#include <util/UTF8.hpp>

#include <cstddef>

namespace hyperion {

class SourceStream
{
public:
    SourceStream(SourceFile *file);
    SourceStream(const SourceStream &other);

    SourceFile *GetFile() const { return m_file; }
    SizeType GetPosition() const { return m_position; }
    bool HasNext() const { return m_position < m_file->GetSize(); }
    utf::u32char Peek() const;
    utf::u32char Next();
    utf::u32char Next(int &pos_change);
    void GoBack(int n = 1);
    void Read(char *ptr, SizeType num_bytes);

private:
    SourceFile *m_file;
    SizeType m_position;
};

} // namespace hyperion

#endif
