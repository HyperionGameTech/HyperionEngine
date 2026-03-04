/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/math/MathUtil.hpp>
#include <Core/memory/ByteBuffer.hpp>
#include <Core/Types.hpp>

#include <Core/filesystem/FilePath.hpp>
#include <Core/io/MemoryMappedFile.hpp>

#include <cstdio>

namespace Hyperion {
class ByteReader
{
public:
    virtual ~ByteReader() = default;

    template <typename T>
    void Read(T* ptr, size_t size = sizeof(T))
    {
        if (size == 0)
        {
            return;
        }

        HYP_CORE_ASSERT(Position() + size <= Max());

        Read(static_cast<void*>(ptr), size);
    }

    /*! \brief Reads from the current position, to current position + \p size.
        If that position is greater than the maximum position, the number of bytes is truncated.
        Endianness is not taken into account
        @returns The number of bytes read */
    size_t Read(size_t size, ByteBuffer& outByteBuffer)
    {
        if (Eof())
        {
            return 0;
        }

        const size_t readToPosition = MathUtil::Min(
            Position() + size,
            Max());

        if (readToPosition <= size_t(Position()))
        {
            return 0;
        }

        const size_t numToRead = readToPosition - size_t(Position());

        outByteBuffer = Read(numToRead);

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

        return Read(Max() - Position());
    }

    virtual size_t Read(void* ptr, size_t size) = 0;
    virtual ByteBuffer Read(size_t size) = 0;

    template <typename T>
    void Peek(T* ptr, size_t size = sizeof(T))
    {
        Read(ptr, size);
        Rewind(size);
    }

    virtual size_t Position() const = 0;
    virtual size_t Max() const = 0;
    virtual void Skip(size_t amount) = 0;
    virtual void Rewind(size_t amount) = 0;
    virtual void Seek(size_t whereTo) = 0;
    virtual void Close() = 0;

    bool Eof() const
    {
        return Position() >= Max();
    }
};

class MemoryByteReader : public ByteReader
{
public:
    explicit MemoryByteReader(ConstByteView byteView)
        : m_byteView(byteView),
          m_pos(0)
    {
    }

    virtual ~MemoryByteReader() override = default;

    virtual size_t Position() const override
    {
        return m_pos;
    }

    virtual size_t Max() const override
    {
        return m_byteView.Size();
    }

    virtual void Skip(size_t amount) override
    {
        m_pos += amount;
    }

    virtual void Rewind(size_t amount) override
    {
        m_pos -= amount;
    }

    virtual void Seek(size_t whereTo) override
    {
        m_pos = whereTo;
    }

    virtual size_t Read(void* ptr, size_t size) override
    {
        if (!m_byteView)
        {
            return 0;
        }

        size_t toRead = MathUtil::Min(m_byteView.Size() - m_pos, size);

        Memory::Copy(ptr, m_byteView.Data() + m_pos, toRead);
        m_pos += toRead;

        return toRead;
    }

    virtual ByteBuffer Read(size_t size) override
    {
        if (!m_byteView)
        {
            return ByteBuffer();
        }

        const auto previousOffset = m_pos;
        m_pos += size;
        return ByteBuffer(size, m_byteView.Data() + previousOffset);
    }

    virtual void Close() override
    {
        // do nothing
    }

protected:
    ConstByteView m_byteView;
    size_t m_pos;
};

class FileByteReader : public ByteReader
{
public:
    FileByteReader(const FilePath& filepath, size_t offset = 0)
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
                m_maxPos = static_cast<size_t>(endPos);
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
        m_pos = cur >= 0 ? static_cast<size_t>(cur) : 0;
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

    virtual size_t Position() const override
    {
        return m_pos;
    }

    virtual size_t Max() const override
    {
        return m_maxPos;
    }

    virtual void Skip(size_t amount) override
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

    virtual void Rewind(size_t amount) override
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

    virtual void Seek(size_t whereTo) override
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

    virtual size_t Read(void* ptr, size_t size) override
    {
        if (m_file == nullptr || size == 0)
        {
            return 0;
        }

        const size_t remaining = m_maxPos > m_pos ? (m_maxPos - m_pos) : 0;
        const size_t toRead = MathUtil::Min(size, remaining);

        if (toRead == 0)
        {
            return 0;
        }

        const size_t readBytes = std::fread(ptr, 1, toRead, m_file);
        m_pos += readBytes;

        return toRead;
    }

    virtual ByteBuffer Read(size_t size) override
    {
        if (m_file == nullptr)
        {
            return ByteBuffer();
        }

        const size_t remaining = m_maxPos > m_pos ? (m_maxPos - m_pos) : 0;
        const size_t toRead = MathUtil::Min(size, remaining);

        if (toRead == 0)
        {
            return ByteBuffer();
        }

        ByteBuffer byteBuffer;
        byteBuffer.SetSize(toRead);

        const size_t readBytes = std::fread(byteBuffer.Data(), 1, toRead, m_file);
        m_pos += readBytes;

        if (readBytes == toRead)
        {
            return byteBuffer;
        }

        return ByteBuffer(readBytes, byteBuffer.Data());
    }

    virtual void Close() override
    {
        if (m_file != nullptr)
        {
            std::fclose(m_file);
            m_file = nullptr;
        }
    }

protected:
    FILE* m_file;
    size_t m_pos;
    size_t m_maxPos;
    FilePath m_filepath;
};

class MemoryMappedByteReader final : public ByteReader
{
public:
    explicit MemoryMappedByteReader(
        MemoryMappedFile* mappedFile,
        size_t offset = 0,
        size_t size = 0,
        MemoryMappedFile::Mode mode = MemoryMappedFile::Mode::READ_ONLY)
        : m_mappedFile(mappedFile),
          m_pos(0),
          m_ownsFile(false)
    {
        HYP_CORE_ASSERT(mappedFile != nullptr);

        const bool opened = m_mappedFile->Open();
        HYP_CORE_ASSERT(opened, "Failed to open memory mapped file!");

        const bool mapped = m_mappedFile->MapRange(offset, 0, m_mappedView);
        HYP_CORE_ASSERT(mapped, "Failed to map memory range");
    }

    MemoryMappedByteReader(
        const FilePath& filepath,
        size_t offset = 0,
        MemoryMappedFile::Mode mode = MemoryMappedFile::Mode::READ_ONLY)
        : m_mappedFile(new MemoryMappedFile(filepath, mode)),
          m_pos(0),
          m_ownsFile(true)
    {
        const bool opened = m_mappedFile->Open();
        HYP_CORE_ASSERT(opened, "Failed to open memory mapped file: %s", filepath.Data());

        const bool mapped = m_mappedFile->MapRange(offset, 0, m_mappedView);
        HYP_CORE_ASSERT(mapped, "Failed to map memory range: %s", filepath.Data());
    }

    MemoryMappedByteReader(
        const FilePath& filepath,
        size_t offset,
        size_t size,
        MemoryMappedFile::Mode mode = MemoryMappedFile::Mode::READ_ONLY)
        : m_mappedFile(new MemoryMappedFile(filepath, mode)),
          m_pos(0),
          m_ownsFile(true)
    {
        const bool opened = m_mappedFile->Open();
        HYP_CORE_ASSERT(opened, "Failed to open memory mapped file: %s", filepath.Data());

        const bool mapped = m_mappedFile->MapRange(offset, size, m_mappedView);
        HYP_CORE_ASSERT(mapped, "Failed to map memory range: %s", filepath.Data());
    }

    virtual ~MemoryMappedByteReader() override
    {
        MemoryMappedByteReader::Close();
    }

    virtual size_t Position() const override
    {
        return m_pos;
    }

    virtual size_t Max() const override
    {
        return m_mappedView.Size();
    }

    virtual void Skip(size_t amount) override
    {
        m_pos = MathUtil::Min(m_pos + amount, Max());
    }

    virtual void Rewind(size_t amount) override
    {
        if (amount > m_pos)
        {
            m_pos = 0;
        }
        else
        {
            m_pos -= amount;
        }
    }

    virtual void Seek(size_t whereTo) override
    {
        if (whereTo > Max())
        {
            whereTo = Max();
        }

        m_pos = whereTo;
    }

    virtual void Close() override
    {
        if (IsOpen())
        {
            m_mappedView.Close();

            if (m_ownsFile)
            {
                m_mappedFile->Close();

                delete m_mappedFile;
            }
        }
            
        m_mappedFile = nullptr;
    }

    bool IsOpen() const
    {
        return m_mappedFile
            && m_mappedFile->IsOpen()
            && m_mappedView.IsOpen();
    }

    MemoryMappedFile::Mode GetMode() const
    {
        return m_mappedFile
            ? m_mappedFile->GetMode()
            : MemoryMappedFile::Mode::READ_ONLY;
    }
    
    virtual size_t Read(void* ptr, size_t size) override
    {
        if (size == 0)
        {
            return 0;
        }

        const size_t remaining = Max() > m_pos ? (Max() - m_pos) : 0;
        const size_t toRead = MathUtil::Min(size, remaining);

        if (toRead == 0)
        {
            return 0;
        }

        const auto* data = static_cast<const ubyte*>(m_mappedView.Data());

        if (data == nullptr)
        {
            return 0;
        }

        Memory::Copy(ptr, data + m_pos, toRead);
        m_pos += toRead;

        return toRead;
    }

    virtual ByteBuffer Read(size_t size) override
    {
        const size_t remaining = Max() > m_pos ? (Max() - m_pos) : 0;
        const size_t toRead = MathUtil::Min(size, remaining);

        if (toRead == 0)
        {
            return ByteBuffer();
        }

        const auto* data = static_cast<const ubyte*>(m_mappedView.Data());

        if (data == nullptr)
        {
            return ByteBuffer();
        }

        const size_t previousOffset = m_pos;
        m_pos += toRead;

        return ByteBuffer(toRead, data + previousOffset);
    }

protected:
    MemoryMappedFile* m_mappedFile;
    MemoryMappedFileView m_mappedView;

    size_t m_pos;

    bool m_ownsFile;
};
} // namespace Hyperion
