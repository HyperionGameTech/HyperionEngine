#pragma once

#include <Lang/SourceFile.hpp>

#include <Core/Debug/Debug.hpp>
#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class BytecodeStream
{
public:
    static BytecodeStream FromSourceFile(SourceFile& file);

public:
    BytecodeStream();
    
    BytecodeStream(const ubyte* buffer, size_t size, size_t position = 0);
    BytecodeStream(const ByteBuffer& byteBuffer, size_t position = 0);
    
    BytecodeStream(const BytecodeStream& other);

    ~BytecodeStream() = default;

    BytecodeStream& operator=(const BytecodeStream& other);

    HYP_FORCE_INLINE const ubyte* GetBuffer() const
    {
        return m_byteBuffer.Data();
    }

    HYP_FORCE_INLINE void ReadBytes(ubyte* ptr, size_t numBytes)
    {
        Memory::Copy(ptr, m_byteBuffer.Data() + m_position, numBytes);
        m_position += numBytes;
    }

    template <class T>
    HYP_FORCE_INLINE void Read(T* ptr, size_t numBytes = sizeof(T))
    {
        ReadBytes(reinterpret_cast<ubyte*>(ptr), numBytes);
    }

    HYP_FORCE_INLINE size_t Position() const
    {
        return m_position;
    }

    HYP_FORCE_INLINE void SetPosition(size_t position)
    {
        m_position = position;
    }

    HYP_FORCE_INLINE size_t Size() const
    {
        return m_byteBuffer.Size();
    }

    HYP_FORCE_INLINE void Seek(size_t address)
    {
        m_position = address;
    }

    HYP_FORCE_INLINE void Skip(size_t amount)
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
    size_t m_position;
};

} // namespace Hyperion
