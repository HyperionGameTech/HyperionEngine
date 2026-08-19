/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Utilities/StringView.hpp>
#include <Core/FileSystem/FilePath.hpp>

#include <Core/Memory/ByteBuffer.hpp>
#include <Core/Memory/Memory.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Types.hpp>

#include <Core/IO/MemoryMappedFile.hpp>

#include <type_traits>

#ifdef HYP_UNIX
#include <unistd.h>
#endif

namespace Hyperion {

using ByteWriterFlags = ubyte;

enum ByteWriterFlagBits : ByteWriterFlags
{
    BYTE_WRITER_FLAGS_NONE = 0x0,
    BYTE_WRITER_FLAGS_WRITE_NULL_CHAR = 0x1,
    BYTE_WRITER_FLAGS_WRITE_SIZE = 0x2,
    BYTE_WRITER_FLAGS_WRITE_STRING_TYPE = 0x4
};

class CORE_API ByteWriter
{
public:
    static constexpr uint32 stringLengthMask = uint32(-1) << 8;
    // number of bits needed to encode string type in string header
    static constexpr uint32 stringTypeMask = uint32(MathUtil::FastLog2(uint32(StringType::MAX)) + 1);

    ByteWriter() = default;
    
    ByteWriter(const ByteWriter& other) = delete;
    ByteWriter& operator=(const ByteWriter& other) = delete;

    ByteWriter(ByteWriter&& other) noexcept = delete;
    ByteWriter& operator=(ByteWriter&& other) noexcept = delete;

    virtual ~ByteWriter() = default;

    void Write(const void* ptr, size_t size)
    {
        WriteBytes(reinterpret_cast<const char*>(ptr), size);
    }

    template <class T>
    void Write(const T& value)
    {
        static_assert(!std::is_pointer_v<NormalizedType<T>>, "Expected to choose other overload");
        static_assert(is_pod_type_v<NormalizedType<T>>, "T must be a POD type to use this overload");

        WriteBytes(reinterpret_cast<const char*>(&value), sizeof(NormalizedType<T>));
    }

    void Write(const ByteBuffer& byteBuffer)
    {
        WriteBytes(reinterpret_cast<const char*>(byteBuffer.Data()), byteBuffer.Size());
    }

    void Write(ByteView byteView)
    {
        WriteBytes(reinterpret_cast<const char*>(byteView.Data()), byteView.Size());
    }

    void Write(ConstByteView byteView)
    {
        WriteBytes(reinterpret_cast<const char*>(byteView.Data()), byteView.Size());
    }

    template <int StringType>
    void WriteString(const StringView<StringType>& str, ByteWriterFlags flags = BYTE_WRITER_FLAGS_NONE)
    {
        uint32 stringHeader = (uint32(str.Size() + ((flags & BYTE_WRITER_FLAGS_WRITE_NULL_CHAR) ? 1 : 0)) << 8) & stringLengthMask;

        if (flags & BYTE_WRITER_FLAGS_WRITE_STRING_TYPE)
        {
            stringHeader |= (uint32(StringType)) & stringTypeMask;
        }

        if (flags & (BYTE_WRITER_FLAGS_WRITE_SIZE | BYTE_WRITER_FLAGS_WRITE_STRING_TYPE))
        {
            Write(&stringHeader, sizeof(uint32));
        }

        WriteBytes(str.Data(), str.Size() * sizeof(typename StringView<StringType>::CharType));

        if (flags & BYTE_WRITER_FLAGS_WRITE_NULL_CHAR)
        {
            WriteBytes("\0", 1);
        }
    }

    template <int StringType>
    void WriteString(const containers::String<StringType>& str, ByteWriterFlags flags = BYTE_WRITER_FLAGS_NONE)
    {
        WriteString(StringView<StringType>(str), flags);
    }

    void WriteString(const char* str)
    {
        WriteString(StringView<StringType::UTF8>(str));
    }

    virtual size_t Position() const = 0;
    virtual void Seek(size_t position, bool truncate = false) = 0;
    virtual void Close() = 0;
    virtual void Flush() = 0;

protected:
    virtual void WriteBytes(const char* ptr, size_t size) = 0;
};

template <class AllocatorType, size_t BufferAlignment = 16>
class MemoryByteWriter final : public ByteWriter
{
public:
    MemoryByteWriter()
        : m_buffer(new memory::ByteBuffer<AllocatorType, BufferAlignment>),
          m_pos(0),
          m_ownsBuffer(true)
    {
    }

    explicit MemoryByteWriter(memory::ByteBuffer<AllocatorType, BufferAlignment>* buffer)
        : m_buffer(buffer),
          m_pos(0),
          m_ownsBuffer(false)
    {
    }

    virtual ~MemoryByteWriter() override
    {
        if (m_ownsBuffer)
        {
            delete m_buffer;
            m_buffer = nullptr;
        }
    }

    virtual size_t Position() const override
    {
        return m_pos;
    }

    virtual void Seek(size_t position, bool truncate = false) override
    {
        m_pos = position;

        if (position >= m_buffer->Size() || truncate)
        {
            m_buffer->SetSize(position);
        }
    }

    virtual void Close() override
    {
        m_pos = 0;
        // fit buffer to size
        m_buffer->SetCapacity(m_buffer->Size());
    }

    virtual void Flush() override
    {
        // do nothing
    }

    HYP_FORCE_INLINE memory::ByteBuffer<AllocatorType, BufferAlignment>& GetBuffer()
    {
        return *m_buffer;
    }

    HYP_FORCE_INLINE const memory::ByteBuffer<AllocatorType, BufferAlignment>& GetBuffer() const
    {
        return *m_buffer;
    }

private:
    memory::ByteBuffer<AllocatorType, BufferAlignment>* m_buffer;
    size_t m_pos;
    bool m_ownsBuffer;

    virtual void WriteBytes(const char* ptr, size_t size) override
    {
        const size_t requiredCapacity = m_buffer->Size() + size;

        if (m_buffer->GetCapacity() < requiredCapacity)
        {
            // Add some padding to reduce number of allocations we need to do
            m_buffer->SetCapacity(size_t(double(requiredCapacity) * 1.5));
        }

        m_buffer->SetSize(m_buffer->Size() + size);
        m_buffer->Write(size, m_pos, ptr);

        m_pos += size;
    }
};

class CORE_API FileByteWriter final : public ByteWriter
{
public:
    explicit FileByteWriter(const FilePath& filepath)
        : m_filepath(filepath),
          m_file(fopen(filepath.Data(), "wb"))
    {
    }

    FileByteWriter(const FilePath& filepath, const char* mode)
        : m_filepath(filepath),
          m_file(fopen(filepath.Data(), mode))
    {
    }

    FileByteWriter(const FileByteWriter& other) = delete;
    FileByteWriter& operator=(const FileByteWriter& other) = delete;

    FileByteWriter(FileByteWriter&& other) noexcept
        : m_filepath(std::move(other.m_filepath)),
          m_file(other.m_file)
    {
        other.m_file = nullptr;
    }

    FileByteWriter& operator=(FileByteWriter&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (m_file != nullptr)
        {
            fclose(m_file);
        }

        m_filepath = std::move(other.m_filepath);

        m_file = other.m_file;
        other.m_file = nullptr;

        return *this;
    }

    virtual ~FileByteWriter() override
    {
        if (m_file != nullptr)
        {
            fclose(m_file);
        }
    }

    virtual size_t Position() const override
    {
        if (m_file == nullptr)
        {
            return 0;
        }

        return ftell(m_file);
    }

    virtual void Seek(size_t position, bool truncate = false) override
    {
        if (m_file == nullptr)
        {
            return;
        }

        fseek(m_file, static_cast<long>(position), SEEK_SET);

        if (truncate)
        {
#ifdef HYP_UNIX
            int result = ftruncate(fileno(m_file), static_cast<off_t>(position));
            (void)result;
#elif defined(HYP_WINDOWS)
            _chsize_s(_fileno(m_file), static_cast<long>(position));
#else
#error "Unsupported platform"
#endif
        }
    }

    virtual void Close() override
    {
        if (m_file == nullptr)
        {
            return;
        }

        fclose(m_file);
        m_file = nullptr;
    }

    virtual void Flush() override
    {
        if (m_file == nullptr)
        {
            return;
        }

        fflush(m_file);
    }

    bool IsOpen() const
    {
        return m_file != nullptr;
    }

    HYP_FORCE_INLINE const FilePath& GetFilePath() const
    {
        return m_filepath;
    }

private:
    FilePath m_filepath;
    FILE* m_file;

    virtual void WriteBytes(const char* ptr, size_t size) override
    {
        if (m_file == nullptr)
        {
            return;
        }

        fwrite(ptr, 1, size, m_file);
    }
};

class CORE_API MemoryMappedByteWriter final : public ByteWriter
{
public:
    explicit MemoryMappedByteWriter(
        MemoryMappedFile* mappedFile,
        size_t offset = 0,
        size_t size = 0)
        : m_mappedFile(mappedFile),
          m_pos(0),
          m_baseOffset(0),
          m_ownsFile(false)
    {
        HYP_CORE_ASSERT(mappedFile != nullptr);

        const bool opened = m_mappedFile->Open();
        HYP_CORE_ASSERT(opened, "Failed to open memory mapped file!");

        HYP_CORE_ASSERT(m_mappedFile->GetMode() == MemoryMappedFile::Mode::READ_WRITE,
                        "MemoryMappedByteWriter requires a read/write mapping");

        const bool mapped = m_mappedFile->MapRange(offset, size, m_mappedView);
        HYP_CORE_ASSERT(mapped, "Failed to map memory range");

        m_baseOffset = m_mappedView.FileOffset();
    }

    MemoryMappedByteWriter(
        const FilePath& filepath,
        size_t offset = 0,
        size_t size = 0)
        : m_mappedFile(new MemoryMappedFile(filepath, MemoryMappedFile::Mode::READ_WRITE)),
          m_pos(0),
          m_baseOffset(0),
          m_ownsFile(true)
    {
        const bool opened = m_mappedFile->Open();
        HYP_CORE_ASSERT(opened, "Failed to open memory mapped file: %s", filepath.Data());

        HYP_CORE_ASSERT(m_mappedFile->GetMode() == MemoryMappedFile::Mode::READ_WRITE,
                        "MemoryMappedByteWriter requires a read/write mapping");

        const bool mapped = m_mappedFile->MapRange(offset, size, m_mappedView);
        HYP_CORE_ASSERT(mapped, "Failed to map memory range: %s", filepath.Data());

        m_baseOffset = m_mappedView.FileOffset();
    }

    virtual ~MemoryMappedByteWriter() override
    {
        MemoryMappedByteWriter::Close();
    }

    virtual size_t Position() const override
    {
        return m_pos;
    }

    virtual void Seek(size_t position, bool truncate = false) override
    {
        (void)truncate;

        if (position > Max())
        {
            position = Max();
        }

        m_pos = position;
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

    virtual void Flush() override
    {
        // do nothing
    }

    bool IsOpen() const
    {
        return m_mappedFile
            && m_mappedFile->IsOpen()
            && m_mappedView.IsOpen();
    }

private:
    MemoryMappedFile* m_mappedFile;
    MemoryMappedFileView m_mappedView;
    size_t m_pos;
    size_t m_baseOffset;
    bool m_ownsFile;

    static constexpr size_t GrowthGranularity = size_t(4) * 1024 * 1024;

    size_t Max() const
    {
        return m_mappedView.Size();
    }

    virtual void WriteBytes(const char* ptr, size_t size) override
    {
        if (size == 0)
        {
            return;
        }

        const auto* src = reinterpret_cast<const ubyte*>(ptr);
        auto* dst = static_cast<ubyte*>(m_mappedView.Data());

        HYP_CORE_ASSERT(m_pos + size <= m_mappedView.Size(),
                        "Attempting to write past the end of the mapped file");

        if (dst == nullptr)
        {
            return;
        }

        Memory::Copy(dst + m_pos, src, size);
        m_pos += size;
    }
};
} // namespace Hyperion
