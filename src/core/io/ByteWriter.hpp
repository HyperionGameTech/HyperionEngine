/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BYTE_WRITER_HPP
#define HYPERION_BYTE_WRITER_HPP

#include <core/utilities/StringView.hpp>
#include <core/filesystem/FilePath.hpp>

#include <core/math/MathUtil.hpp>

#include <Types.hpp>

#include <type_traits>

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
    static constexpr uint32 string_length_mask = uint32(-1) << 8;
    // number of bits needed to encode string type in string header
    static constexpr uint32 string_type_mask = uint32(MathUtil::FastLog2(uint32(StringType::MAX)) + 1);

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
        static_assert(is_pod_type<NormalizedType<T>>, "T must be a POD type to use this overload");

        WriteBytes(reinterpret_cast<const char*>(&value), sizeof(NormalizedType<T>));
    }

    void Write(const ByteBuffer& byte_buffer)
    {
        WriteBytes(reinterpret_cast<const char*>(byte_buffer.Data()), byte_buffer.Size());
    }

    void Write(ByteView byte_view)
    {
        WriteBytes(reinterpret_cast<const char*>(byte_view.Data()), byte_view.Size());
    }

    void Write(ConstByteView byte_view)
    {
        WriteBytes(reinterpret_cast<const char*>(byte_view.Data()), byte_view.Size());
    }

    template <int StringType>
    void WriteString(const StringView<StringType>& str, ByteWriterFlags flags = BYTE_WRITER_FLAGS_NONE)
    {
        uint32 string_header = (uint32(str.Size() + ((flags & BYTE_WRITER_FLAGS_WRITE_NULL_CHAR) ? 1 : 0)) << 8) & string_length_mask;

        if (flags & BYTE_WRITER_FLAGS_WRITE_STRING_TYPE)
        {
            string_header |= (uint32(StringType)) & string_type_mask;
        }

        if (flags & (BYTE_WRITER_FLAGS_WRITE_SIZE | BYTE_WRITER_FLAGS_WRITE_STRING_TYPE))
        {
            Write(&string_header, sizeof(uint32));
        }

        WriteBytes(str.Data(), str.Size() * sizeof(typename StringView<StringType>::CharType));

        if (flags & BYTE_WRITER_FLAGS_WRITE_NULL_CHAR)
        {
            WriteBytes("\0", 1);
        }
    }

    template <int StringType>
    void WriteString(const containers::detail::String<StringType>& str, ByteWriterFlags flags = BYTE_WRITER_FLAGS_NONE)
    {
        WriteString(StringView<StringType>(str), flags);
    }

    void WriteString(const char* str)
    {
        WriteString(StringView<StringType::UTF8>(str));
    }

    virtual SizeType Position() const = 0;
    virtual void Close() = 0;

protected:
    virtual void WriteBytes(const char* ptr, SizeType size) = 0;
};

class MemoryByteWriter : public ByteWriter
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

    virtual void Close() override
    {
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
        const SizeType required_capacity = m_buffer.Size() + size;

        if (m_buffer.GetCapacity() < required_capacity)
        {
            // Add some padding to reduce number of allocations we need to do
            m_buffer.SetCapacity(SizeType(double(required_capacity) * 1.5));
        }

        m_buffer.SetSize(m_buffer.Size() + size);
        m_buffer.Write(size, m_pos, ptr);
        m_pos += size;
    }
};

class FileByteWriter : public ByteWriter
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

#endif
