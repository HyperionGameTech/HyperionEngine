#pragma once

#include <script/SourceFile.hpp>
#include <core/debug/Debug.hpp>
#include <core/memory/ByteBuffer.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Script_Stream
{
public:
    static Script_Stream FromSourceFile(SourceFile& file);

public:
    Script_Stream();
    Script_Stream(const ubyte* buffer, SizeType size, SizeType position = 0);
    Script_Stream(const ByteBuffer& byteBuffer, SizeType position = 0);
    Script_Stream(const Script_Stream& other);
    ~Script_Stream() = default;

    Script_Stream& operator=(const Script_Stream& other);

    const ubyte* GetBuffer() const
    {
        return m_byteBuffer.Data();
    }

    void ReadBytes(ubyte* ptr, SizeType numBytes)
    {
        Assert(m_position + numBytes < m_byteBuffer.Size() + 1, "cannot read past end of buffer");

        const auto* data = m_byteBuffer.Data();

        for (SizeType i = 0; i < numBytes; i++)
        {
            ptr[i] = data[m_position++];
        }
    }

    template <class T>
    void Read(T* ptr, SizeType numBytes = sizeof(T))
    {
        ReadBytes(reinterpret_cast<ubyte*>(ptr), numBytes);
    }

    SizeType Position() const
    {
        return m_position;
    }

    void SetPosition(SizeType position)
    {
        m_position = position;
    }

    SizeType Size() const
    {
        return m_byteBuffer.Size();
    }

    void Seek(SizeType address)
    {
        m_position = address;
    }

    void Skip(SizeType amount)
    {
        m_position += amount;
    }

    bool Eof() const
    {
        return m_position >= m_byteBuffer.Size();
    }

    void ReadZeroTerminatedString(char* ptr);

private:
    ByteBuffer m_byteBuffer;
    SizeType m_position;
};

} // namespace hyperion
