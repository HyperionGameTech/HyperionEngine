/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/math/MathUtil.hpp>
#include <Core/memory/ByteBuffer.hpp>
#include <Core/Types.hpp>

#include <Core/filesystem/FilePath.hpp>
#include <Core/io/MemoryMappedFile.hpp>

#include <cstdio>

#if HYP_ANDROID
struct AAsset;
#endif

namespace Hyperion {

class CORE_API ByteReader
{
protected:
    ByteReader() = default;

public:
    virtual ~ByteReader() = default;

    bool Eof() const
    {
        return Position() >= Max();
    }

    template <typename T, typename = std::enable_if_t<!std::is_same_v<T*, void*>>>
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
};

class CORE_API MemoryByteReader final : public ByteReader
{
public:
    explicit MemoryByteReader(ConstByteView byteView)
        : m_byteView(byteView),
          m_pos(0)
    {
    }

    ~MemoryByteReader() override = default;

    size_t Position() const override
    {
        return m_pos;
    }

    size_t Max() const override
    {
        return m_byteView.Size();
    }

    void Skip(size_t amount) override
    {
        m_pos += amount;
    }

    void Rewind(size_t amount) override
    {
        m_pos -= amount;
    }

    void Seek(size_t whereTo) override
    {
        m_pos = whereTo;
    }

    size_t Read(void* ptr, size_t size) override;

    HYP_FORCE_INLINE ByteBuffer Read()
    {
        return ByteReader::Read();
    }

    ByteBuffer Read(size_t size) override;

    void Close() override;

protected:
    ConstByteView m_byteView;
    size_t m_pos;
};

class CORE_API FileByteReader final : public ByteReader
{
public:
    explicit FileByteReader(const FilePath& filepath, size_t offset = 0);
    ~FileByteReader() override;

    const FilePath& GetFilepath() const
    {
        return m_filepath;
    }

    size_t Position() const override
    {
        return m_pos;
    }

    size_t Max() const override
    {
        return m_maxPos;
    }

    void Skip(size_t amount) override;

    void Rewind(size_t amount) override;

    void Seek(size_t whereTo) override;

    size_t Read(void* ptr, size_t size) override;
    
    HYP_FORCE_INLINE ByteBuffer Read()
    {
        return ByteReader::Read();
    }
    
    ByteBuffer Read(size_t size) override;

    void Close() override;

protected:
    FILE* m_file;
    size_t m_pos;
    size_t m_maxPos;
    FilePath m_filepath;
    
#if HYP_ANDROID
    AAsset* m_asset;
#endif
};

class CORE_API MemoryMappedByteReader final : public ByteReader
{
public:
    explicit MemoryMappedByteReader(
        MemoryMappedFile* mappedFile,
        size_t offset = 0,
        size_t size = 0,
        MemoryMappedFile::Mode mode = MemoryMappedFile::Mode::READ_ONLY);

    MemoryMappedByteReader(
        const FilePath& filepath,
        size_t offset = 0,
        MemoryMappedFile::Mode mode = MemoryMappedFile::Mode::READ_ONLY);

    MemoryMappedByteReader(
        const FilePath& filepath,
        size_t offset,
        size_t size,
        MemoryMappedFile::Mode mode = MemoryMappedFile::Mode::READ_ONLY);

    ~MemoryMappedByteReader() override;

    bool IsOpen() const;

    MemoryMappedFile::Mode GetMode() const;

    size_t Position() const override;
    size_t Max() const override;

    void Skip(size_t amount) override;

    void Rewind(size_t amount) override;

    void Seek(size_t whereTo) override;

    void Close() override;
    
    size_t Read(void* ptr, size_t size) override;
    
    HYP_FORCE_INLINE ByteBuffer Read()
    {
        return ByteReader::Read();
    }
    
    ByteBuffer Read(size_t size) override;

protected:
    MemoryMappedFile* m_mappedFile;
    MemoryMappedFileView m_mappedView;

    size_t m_pos;

    bool m_ownsFile;
};
} // namespace Hyperion
