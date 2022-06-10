#ifndef BYTECODE_STREAM_HPP
#define BYTECODE_STREAM_HPP

#include <script/SourceFile.hpp>

#include <system/debug.h>

namespace hyperion {
namespace vm {

class BytecodeStream {
public:
    static BytecodeStream FromSourceFile(SourceFile *file_ptr);

public:
    BytecodeStream();
    BytecodeStream(const char *buffer, size_t size, size_t position = 0);
    BytecodeStream(const BytecodeStream &other);
    ~BytecodeStream() = default;

    BytecodeStream &operator=(const BytecodeStream &other);

    inline const char *GetBuffer() const { return m_buffer; }

    inline void ReadBytes(char *ptr, size_t num_bytes)
    {
        AssertThrowMsg(m_position + num_bytes < m_size + 1, "cannot read past end of buffer");
        for (size_t i = 0; i < num_bytes; i++) {
            ptr[i] = m_buffer[m_position++];
        }
    }

    template <typename T>
    inline void Read(T *ptr, size_t num_bytes = sizeof(T))
        { ReadBytes(reinterpret_cast<char*>(ptr), num_bytes); }
    inline size_t Position() const
        { return m_position; }
    inline void SetPosition(size_t position)
        { m_position = position; }
    inline size_t Size() const
        { return m_size; }
    inline void Seek(size_t address)
        { m_position = address; }
    inline void Skip(size_t amount)
        { m_position += amount; }
    inline bool Eof() const
        { return m_position >= m_size; }

    void ReadZeroTerminatedString(char *ptr);

private:
    const char *m_buffer;
    size_t m_size;
    size_t m_position;
};

} // namespace vm
} // namespace hyperion

#endif
