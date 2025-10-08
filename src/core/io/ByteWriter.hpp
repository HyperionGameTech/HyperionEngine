/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/StringView.hpp>
#include <core/filesystem/FilePath.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/math/MathUtil.hpp>

#include <core/Types.hpp>

#include <type_traits>

#ifdef HYP_UNIX
#include <unistd.h>
#endif

namespace hyperion {

using ByteWriterFlags = ubyte;

enum ByteWriterFlagBits : ByteWriterFlags
{
    BYTE_WRITER_FLAGS_NONE = 0x0,
    BYTE_WRITER_FLAGS_WRITE_NULL_CHAR = 0x1,
    BYTE_WRITER_FLAGS_WRITE_SIZE = 0x2,
    BYTE_WRITER_FLAGS_WRITE_STRING_TYPE = 0x4
};

class ByteWriter
{
public:
    static constexpr uint32 stringLengthMask = uint32(-1) << 8;
    // number of bits needed to encode string type in string header
    static constexpr uint32 stringTypeMask = uint32(MathUtil::FastLog2(uint32(StringType::MAX)) + 1);

    ByteWriter() = default;
    ByteWriter(const ByteWriter& other) = delete;
    ByteWriter& operator=(const ByteWriter& other) = delete;
    virtual ~ByteWriter() = default;

    void Write(const void* ptr, SizeType size)
    {
        WriteBytes(reinterpret_cast<const char*>(ptr), size);
    }

    template <class T>
    void Write(const T& value)
    {
        static_assert(!std::is_pointer_v<NormalizedType<T>>, "Expected to choose other overload");
        static_assert(IsPodTypeV<NormalizedType<T>>, "T must be a POD type to use this overload");

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

    virtual SizeType Position() const = 0;
    virtual void Seek(SizeType position, bool truncate = false) = 0;
    virtual void Close() = 0;

protected:
    virtual void WriteBytes(const char* ptr, SizeType size) = 0;
};

class MemoryByteWriter final : public ByteWriter
{
public:
    MemoryByteWriter()
        : m_pos(0)
    {
    }

    virtual ~MemoryByteWriter() override = default;

    virtual SizeType Position() const override
    {
        return m_pos;
    }

    virtual void Seek(SizeType position, bool truncate = false) override
    {
        if (position < m_buffer.Size())
        {
            m_pos = position;

            if (truncate)
            {
                m_buffer.SetSize(m_pos);
            }
        }
        else
        {
            m_buffer.SetSize(position + 1);
            m_pos = position;
        }
    }

    virtual void Close() override
    {
        m_pos = 0;
        // fit buffer to size
        m_buffer.SetCapacity(m_buffer.Size());
    }

    HYP_FORCE_INLINE ByteBuffer& GetBuffer()
    {
        return m_buffer;
    }

    HYP_FORCE_INLINE const ByteBuffer& GetBuffer() const
    {
        return m_buffer;
    }

private:
    ByteBuffer m_buffer;
    SizeType m_pos;

    virtual void WriteBytes(const char* ptr, SizeType size) override
    {
        const SizeType requiredCapacity = m_buffer.Size() + size;

        if (m_buffer.GetCapacity() < requiredCapacity)
        {
            // Add some padding to reduce number of allocations we need to do
            m_buffer.SetCapacity(SizeType(double(requiredCapacity) * 1.5));
        }

        m_buffer.SetSize(m_buffer.Size() + size);
        m_buffer.Write(size, m_pos, ptr);
        m_pos += size;
    }
};

class FileByteWriter final : public ByteWriter
{
public:
    FileByteWriter(const FilePath& filepath)
        : m_filepath(filepath),
          m_file(fopen(filepath.Data(), "wb"))
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
        if (std::addressof(other) == this)
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

    virtual SizeType Position() const override
    {
        if (m_file == nullptr)
        {
            return 0;
        }

        return ftell(m_file);
    }

    virtual void Seek(SizeType position, bool truncate = false) override
    {
        if (m_file == nullptr)
        {
            return;
        }

        fseek(m_file, static_cast<long>(position), SEEK_SET);

        if (truncate)
        {
#ifdef HYP_UNIX
            ftruncate(fileno(m_file), static_cast<off_t>(position));
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

    virtual void WriteBytes(const char* ptr, SizeType size) override
    {
        if (m_file == nullptr)
        {
            return;
        }

        fwrite(ptr, 1, size, m_file);
    }
};
} // namespace hyperion
