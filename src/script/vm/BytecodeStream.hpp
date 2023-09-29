#ifndef BYTECODE_STREAM_HPP
#define BYTECODE_STREAM_HPP

#include <script/SourceFile.hpp>
#include <system/Debug.hpp>
#include <core/lib/ByteBuffer.hpp>

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
    BytecodeStream(const ByteBuffer &byte_buffer, SizeType position = 0);
    BytecodeStream(const BytecodeStream &other);
    ~BytecodeStream() = default;

    BytecodeStream &operator=(const BytecodeStream &other);

    const UByte *GetBuffer() const
        { return m_byte_buffer.Data(); }

    void ReadBytes(UByte *ptr, SizeType num_bytes)
    {
        AssertThrowMsg(m_position + num_bytes < m_byte_buffer.Size() + 1, "cannot read past end of buffer");
        
        const auto *data = m_byte_buffer.Data();

        for (SizeType i = 0; i < num_bytes; i++) {
            ptr[i] = data[m_position++];
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
        { return m_byte_buffer.Size(); }

    void Seek(SizeType address)
        { m_position = address; }

    void Skip(SizeType amount)
        { m_position += amount; }

    bool Eof() const
        { return m_position >= m_byte_buffer.Size(); }

    void ReadZeroTerminatedString(SChar *ptr);

private:
    ByteBuffer m_byte_buffer;
    SizeType m_position;
};

} // namespace vm
} // namespace hyperion

#endif
