/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/math/MathUtil.hpp>
#include <core/memory/ByteBuffer.hpp>
#include <core/Types.hpp>

#include <core/filesystem/FilePath.hpp>

#include <cstdio>

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

    /*! \brief Reads from the current position, to current position + \p size.
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
    FileByteReader(const FilePath& filepath, SizeType offset = 0)
        : m_file(nullptr),
          m_pos(0),
          m_maxPos(0),
          m_filepath(filepath)
    {
        m_file = std::fopen(filepath.Data(), "rb");

        if (m_file == nullptr)
        {
            m_maxPos = 0;
            m_pos = 0;

            return;
        }

        // determine file size
        if (std::fseek(m_file, 0, SEEK_END) == 0)
        {
            long endPos = std::ftell(m_file);

            if (endPos >= 0)
            {
                m_maxPos = static_cast<SizeType>(endPos);
            }
            else
            {
                m_maxPos = 0;
            }
        }
        else
        {
            m_maxPos = 0;
        }

        // seek to beginning offset
        if (offset > m_maxPos)
        {
            offset = m_maxPos;
        }

        std::fseek(m_file, static_cast<long>(offset), SEEK_SET);

        long cur = std::ftell(m_file);
        m_pos = cur >= 0 ? static_cast<SizeType>(cur) : 0;
    }

    virtual ~FileByteReader() override
    {
        if (m_file != nullptr)
        {
            std::fclose(m_file);
            m_file = nullptr;
        }
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
        if (m_file == nullptr)
        {
            return;
        }

        m_pos += amount;

        if (m_pos > m_maxPos)
        {
            m_pos = m_maxPos;
        }

        std::fseek(m_file, static_cast<long>(m_pos), SEEK_SET);
    }

    virtual void Rewind(SizeType amount) override
    {
        if (m_file == nullptr)
        {
            return;
        }

        if (amount > m_pos)
        {
            m_pos = 0;
        }
        else
        {
            m_pos -= amount;
        }

        std::fseek(m_file, static_cast<long>(m_pos), SEEK_SET);
    }

    virtual void Seek(SizeType whereTo) override
    {
        if (!m_file)
        {
            return;
        }

        if (whereTo > m_maxPos)
        {
            whereTo = m_maxPos;
        }

        m_pos = whereTo;
        std::fseek(m_file, static_cast<long>(m_pos), SEEK_SET);
    }

protected:
    FILE* m_file;
    SizeType m_pos;
    SizeType m_maxPos;
    FilePath m_filepath;

    virtual void ReadBytes(void* ptr, SizeType size) override
    {
        if (m_file == nullptr || size == 0)
        {
            return;
        }

        const SizeType remaining = m_maxPos > m_pos ? (m_maxPos - m_pos) : 0;
        const SizeType toRead = MathUtil::Min(size, remaining);

        if (toRead == 0)
        {
            return;
        }

        const SizeType readBytes = std::fread(ptr, 1, toRead, m_file);
        m_pos += readBytes;
    }

    virtual ByteBuffer ReadBytes(SizeType size) override
    {
        if (m_file == nullptr)
        {
            return ByteBuffer();
        }

        const SizeType remaining = m_maxPos > m_pos ? (m_maxPos - m_pos) : 0;
        const SizeType toRead = MathUtil::Min(size, remaining);

        if (toRead == 0)
        {
            return ByteBuffer();
        }

        ByteBuffer byteBuffer;
        byteBuffer.SetSize(toRead);

        const SizeType readBytes = std::fread(byteBuffer.Data(), 1, toRead, m_file);
        m_pos += readBytes;

        if (readBytes == toRead)
        {
            return byteBuffer;
        }

        return ByteBuffer(readBytes, byteBuffer.Data());
    }
};
} // namespace hyperion
