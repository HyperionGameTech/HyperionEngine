#ifndef BYTECODE_STREAM_HPP
#define BYTECODE_STREAM_HPP

#include <script/SourceFile.hpp>

#include <system/Debug.hpp>

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

    const char *GetBuffer() const { return m_buffer; }

    void ReadBytes(char *ptr, size_t num_bytes)
    {
        AssertThrowMsg(m_position + num_bytes < m_size + 1, "cannot read past end of buffer");
        for (size_t i = 0; i < num_bytes; i++) {
            ptr[i] = m_buffer[m_position++];
        }
    }

    template <typename T>
    void Read(T *ptr, size_t num_bytes = sizeof(T))
        { ReadBytes(reinterpret_cast<char*>(ptr), num_bytes); }
    size_t Position() const
        { return m_position; }
    void SetPosition(size_t position)
        { m_position = position; }
    size_t Size() const
        { return m_size; }
    void Seek(size_t address)
        { m_position = address; }
    void Skip(size_t amount)
        { m_position += amount; }
    bool Eof() const
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
