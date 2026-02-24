#pragma once

#include <script/SourceFile.hpp>
#include <Core/debug/Debug.hpp>
#include <Core/memory/ByteBuffer.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class BytecodeStream
{
public:
    static BytecodeStream FromSourceFile(SourceFile& file);

public:
    BytecodeStream();
    BytecodeStream(const ubyte* buffer, SizeType size, SizeType position = 0);
    BytecodeStream(const ByteBuffer& byteBuffer, SizeType position = 0);
    BytecodeStream(const BytecodeStream& other);
    ~BytecodeStream() = default;

    BytecodeStream& operator=(const BytecodeStream& other);

    HYP_FORCE_INLINE const ubyte* GetBuffer() const
    {
        return m_byteBuffer.Data();
    }

    HYP_FORCE_INLINE void ReadBytes(ubyte* ptr, SizeType numBytes)
    {
        Memory::Copy(ptr, m_byteBuffer.Data() + m_position, numBytes);
        m_position += numBytes;
    }

    template <class T>
    HYP_FORCE_INLINE void Read(T* ptr, SizeType numBytes = sizeof(T))
    {
        ReadBytes(reinterpret_cast<ubyte*>(ptr), numBytes);
    }

    HYP_FORCE_INLINE SizeType Position() const
    {
        return m_position;
    }

    HYP_FORCE_INLINE void SetPosition(SizeType position)
    {
        m_position = position;
    }

    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_byteBuffer.Size();
    }

    HYP_FORCE_INLINE void Seek(SizeType address)
    {
        m_position = address;
    }

    HYP_FORCE_INLINE void Skip(SizeType amount)
    {
        m_position += amount;
    }

    HYP_FORCE_INLINE bool Eof() const
    {
        return m_position >= m_byteBuffer.Size();
    }

    void ReadZeroTerminatedString(char* ptr);

private:
    ByteBuffer m_byteBuffer;
    SizeType m_position;
};

} // namespace Hyperion
