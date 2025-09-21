/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/MathUtil.hpp>
#include <core/memory/ByteBuffer.hpp>
#include <core/Types.hpp>

#include <core/filesystem/FilePath.hpp>

#include <iostream>

namespace hyperion {
class ByteReader
{
public:
    virtual ~ByteReader() = default;

    template <typename T>
    void Read(T* ptr, SizeType size = sizeof(T))
    {
        if (size == 0)
        {
            return;
        }

        HYP_CORE_ASSERT(Position() + size <= Max());

        ReadBytes(static_cast<void*>(ptr), size);
    }

    /*! \brief Reads from the current position, to current position + \ref{size}.
        If that position is greater than the maximum position, the number of bytes is truncated.
        Endianness is not taken into account
        @returns The number of bytes read */
    SizeType Read(SizeType size, ByteBuffer& outByteBuffer)
    {
        if (Eof())
        {
            return 0;
        }

        const SizeType readToPosition = MathUtil::Min(
            Position() + size,
            Max());

        if (readToPosition <= SizeType(Position()))
        {
            return 0;
        }

        const SizeType numToRead = readToPosition - SizeType(Position());

        outByteBuffer = ReadBytes(numToRead);

        return numToRead;
    }

    /*! \brief Read from the current position to the end of the stream.
        @returns a ByteBuffer object, containing the data that was read. */
    ByteBuffer Read()
    {
        if (Eof())
        {
            return ByteBuffer();
        }

        return ReadBytes(Max() - Position());
    }

    template <typename T>
    void Peek(T* ptr, SizeType size = sizeof(T))
    {
        Read(ptr, size);
        Rewind(size);
    }

    virtual SizeType Position() const = 0;
    virtual SizeType Max() const = 0;
    virtual void Skip(SizeType amount) = 0;
    virtual void Rewind(SizeType amount) = 0;
    virtual void Seek(SizeType whereTo) = 0;

    bool Eof() const
    {
        return Position() >= Max();
    }

protected:
    virtual void ReadBytes(void* ptr, SizeType size) = 0;
    virtual ByteBuffer ReadBytes(SizeType size) = 0;
};

class MemoryByteReader : public ByteReader
{
public:
    MemoryByteReader(ByteBuffer* byteBuffer)
        : m_byteBuffer(byteBuffer),
          m_pos(0)
    {
    }

    virtual ~MemoryByteReader() override = default;

    virtual SizeType Position() const override
    {
        return m_pos;
    }

    virtual SizeType Max() const override
    {
        return m_byteBuffer != nullptr ? m_byteBuffer->Size() : 0;
    }

    virtual void Skip(SizeType amount) override
    {
        m_pos += amount;
    }

    virtual void Rewind(SizeType amount) override
    {
        m_pos -= amount;
    }

    virtual void Seek(SizeType whereTo) override
    {
        m_pos = whereTo;
    }

protected:
    ByteBuffer* m_byteBuffer;
    SizeType m_pos;

    virtual void ReadBytes(void* ptr, SizeType size) override
    {
        if (m_byteBuffer == nullptr)
        {
            return;
        }

        Memory::MemCpy(ptr, m_byteBuffer->Data() + m_pos, size);
        m_pos += size;
    }

    virtual ByteBuffer ReadBytes(SizeType size) override
    {
        if (m_byteBuffer == nullptr)
        {
            return ByteBuffer();
        }

        const auto previousOffset = m_pos;
        m_pos += size;
        return ByteBuffer(size, m_byteBuffer->Data() + previousOffset);
    }
};

class FileByteReader : public ByteReader
{
public:
    FileByteReader(const FilePath& filepath, std::streampos begin = 0)
        : m_filepath(filepath),
          m_pos(0),
          m_maxPos(0)
    {
        m_file = new std::ifstream(
            filepath.Data(),
            std::ifstream::in
                | std::ifstream::binary
                | std::ifstream::ate);

        m_maxPos = m_file->tellg();
        m_file->seekg(begin);
        m_pos = m_file->tellg();
    }

    virtual ~FileByteReader() override
    {
        delete m_file;
    }

    const FilePath& GetFilepath() const
    {
        return m_filepath;
    }

    virtual SizeType Position() const override
    {
        return m_pos;
    }

    virtual SizeType Max() const override
    {
        return m_maxPos;
    }

    virtual void Skip(SizeType amount) override
    {
        m_file->seekg(m_pos += amount);
    }

    virtual void Rewind(SizeType amount) override
    {
        m_file->seekg(m_pos -= amount);
    }

    virtual void Seek(SizeType whereTo) override
    {
        m_file->seekg(m_pos = whereTo);
    }

protected:
    std::istream* m_file;
    SizeType m_pos;
    SizeType m_maxPos;
    FilePath m_filepath;

    virtual void ReadBytes(void* ptr, size_t size) override
    {
        m_file->read(static_cast<char*>(ptr), size);
        m_pos += size;
    }

    virtual ByteBuffer ReadBytes(SizeType size) override
    {
        m_pos += size;

        ByteBuffer byteBuffer(size);
        m_file->read(reinterpret_cast<char*>(byteBuffer.Data()), size);

        return byteBuffer;
    }
};
} // namespace hyperion
