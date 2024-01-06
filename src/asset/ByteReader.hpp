#ifndef BYTE_READER_H
#define BYTE_READER_H

#include <math/MathUtil.hpp>
#include <core/lib/ByteBuffer.hpp>
#include <core/Util.hpp>
#include <Types.hpp>

#include <fstream>
#include <core/Core.hpp>

namespace hyperion {
class ByteReader
{
public:
    using Bytes = std::vector<UByte>;

    virtual ~ByteReader() = default;

    template <typename T>
    void Read(T *ptr, SizeType size = sizeof(T))
    {
        if (size == 0) {
            return;
        }

        AssertThrow(Position() + size <= Max());

        ReadBytes(static_cast<void *>(ptr), size);
    }

    /*! \brief Reads from the current position, to current position + \ref{size}.
        If that position is greater than the maximum position, the number of bytes is truncated.
        Endianness is not taken into account
        @returns The number of bytes read */
    SizeType Read(SizeType size, ByteBuffer &out_byte_buffer)
    {
        if (Eof()) {
            return 0;
        }
    
        const SizeType read_to_position = MathUtil::Min(
            Position() + size,
            Max()
        );

        if (read_to_position <= SizeType(Position())) {
            return 0;
        }

        const SizeType num_to_read = read_to_position - SizeType(Position());

        out_byte_buffer = ReadBytes(num_to_read);

        return num_to_read;
    }

    /*! \brief Read from the current position to the end of the stream.
        @returns a ByteBuffer object, containing the data that was read. */
    ByteBuffer Read()
    {
        if (Eof()) {
            return ByteBuffer();
        }

        return ReadBytes(Max() - Position());
    }
    
    template <typename T>
    void Peek(T *ptr, SizeType size = sizeof(T))
    {
        Read(ptr, size);
        Rewind(size);
    }

    virtual SizeType Position() const = 0;
    virtual SizeType Max() const = 0;
    virtual void Skip(SizeType amount) = 0;
    virtual void Rewind(SizeType amount) = 0;
    virtual void Seek(SizeType where_to) = 0;

    bool Eof() const
        { return Position() >= Max(); }

protected:
    virtual void ReadBytes(void *ptr, SizeType size) = 0;
    virtual ByteBuffer ReadBytes(SizeType size) = 0;
};

class MemoryByteReader : public ByteReader
{
public:
    MemoryByteReader(ByteBuffer *byte_buffer)
        : m_byte_buffer(byte_buffer),
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
        return m_byte_buffer != nullptr ? m_byte_buffer->Size() : 0;
    }

    virtual void Skip(SizeType amount) override
    {
        m_pos += amount;
    }

    virtual void Rewind(SizeType amount) override
    {
        m_pos -= amount;
    }

    virtual void Seek(SizeType where_to) override
    {
        m_pos = where_to;
    }

protected:
    ByteBuffer *m_byte_buffer;
    SizeType m_pos;

    virtual void ReadBytes(void *ptr, SizeType size) override
    {
        if (m_byte_buffer == nullptr) {
            return;
        }

        Memory::MemCpy(ptr, m_byte_buffer->Data() + m_pos, size);
        m_pos += size;
    }

    virtual ByteBuffer ReadBytes(SizeType size) override
    {
        if (m_byte_buffer == nullptr) {
            return ByteBuffer();
        }

        const auto previous_offset = m_pos;
        m_pos += size;
        return ByteBuffer(size, m_byte_buffer->Data() + previous_offset);
    }
};

class FileByteReader : public ByteReader
{
public:
    FileByteReader(const std::string &filepath, std::streampos begin = 0)
        : filepath(filepath),
          pos(0),
          max_pos(0)
    {
        DebugLog(
            LogType::Debug,
            "Create FileByteReader instance with path %s\n",
            filepath.c_str()
        );

        file = new std::ifstream(
            filepath,
            std::ifstream::in
            | std::ifstream::binary
            | std::ifstream::ate
        );

        max_pos = file->tellg();
        file->seekg(begin);
        pos = file->tellg();

        if (Eof()) {
            DebugLog(
                LogType::Warn,
                "File could not be opened at path %s\n",
                filepath.c_str()
            );
        }
    }

    virtual ~FileByteReader() override
    {
        delete file;
    }
    
    const std::string &GetFilepath() const
    {
        return filepath;
    }

    virtual SizeType Position() const override
    {
        return pos;
    }

    virtual SizeType Max() const override
    {
        return max_pos;
    }

    virtual void Skip(SizeType amount) override
    {
        file->seekg(pos += amount);
    }

    virtual void Rewind(SizeType amount) override
    {
        file->seekg(pos -= amount);
    }

    virtual void Seek(SizeType where_to) override
    {
        file->seekg(pos = where_to);
    }

protected:
    std::istream *file;
    SizeType pos;
    SizeType max_pos;
    std::string filepath;

    virtual void ReadBytes(void *ptr, size_t size) override
    {
        file->read(static_cast<char *>(ptr), size);
        pos += size;
    }

    virtual ByteBuffer ReadBytes(SizeType size) override
    {
        pos += size;

        ByteBuffer byte_buffer(size);
        file->read(reinterpret_cast<char *>(byte_buffer.Data()), size);

        return byte_buffer;
    }
};
}

#endif
