#ifndef SOURCE_STREAM_HPP
#define SOURCE_STREAM_HPP

#include <script/compiler/SourceFile.hpp>
#include <util/utf8.hpp>

#include <cstddef>

namespace hyperion::compiler {

class SourceStream {
public:
    SourceStream(SourceFile *file);
    SourceStream(const SourceStream &other);

    inline SourceFile *GetFile() const { return m_file; }
    inline size_t GetPosition() const { return m_position; }
    inline bool HasNext() const { return m_position < m_file->GetSize(); }
    utf::u32char Peek() const;
    utf::u32char Next();
    utf::u32char Next(int &pos_change);
    void GoBack(int n = 1);
    void Read(char *ptr, size_t num_bytes);

private:
    SourceFile *m_file;
    size_t m_position;
};

} // namespace hyperion::compiler

#endif
