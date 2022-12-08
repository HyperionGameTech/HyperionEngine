#ifndef BYTECODE_STREAM_HPP
#define BYTECODE_STREAM_HPP

#include <script/SourceFile.hpp>
#include <system/Debug.hpp>

#include <Types.hpp>

namespace hyperion {
namespace vm {

class BytecodeStream
{
public:
    static BytecodeStream FromSourceFile(SourceFile &file);

public:
    BytecodeStream();
    BytecodeStream(const UByte *buffer, SizeType size, SizeType position = 0);
    BytecodeStream(const BytecodeStream &other);
    ~BytecodeStream() = default;

    BytecodeStream &operator=(const BytecodeStream &other);

    const UByte *GetBuffer() const
        { return m_buffer; }

    void ReadBytes(UByte *ptr, SizeType num_bytes)
    {
        AssertThrowMsg(m_position + num_bytes < m_size + 1, "cannot read past end of buffer");

        for (SizeType i = 0; i < num_bytes; i++) {
            ptr[i] = m_buffer[m_position++];
        }
    }

    template <class T>
    void Read(T *ptr, SizeType num_bytes = sizeof(T))
        { ReadBytes(reinterpret_cast<UByte *>(ptr), num_bytes); }

    SizeType Position() const
        { return m_position; }

    void SetPosition(SizeType position)
        { m_position = position; }

    SizeType Size() const
        { return m_size; }

    void Seek(SizeType address)
        { m_position = address; }

    void Skip(SizeType amount)
        { m_position += amount; }

    bool Eof() const
        { return m_position >= m_size; }

    void ReadZeroTerminatedString(SChar *ptr);

private:
    const UByte *m_buffer;
    SizeType m_size;
    SizeType m_position;
};

} // namespace vm
} // namespace hyperion

#endif
