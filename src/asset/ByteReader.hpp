#ifndef BYTE_READER_H
#define BYTE_READER_H

#include <math/MathUtil.hpp>
#include <core/lib/ByteBuffer.hpp>
#include <Types.hpp>
#include <Util.hpp>

#include <fstream>
#include <core/Core.hpp>

namespace hyperion {
class ByteReader
{
public:
    using Bytes = std::vector<UByte>;

    virtual ~ByteReader() {}

    template <typename T>
    void Read(T *ptr, SizeType size = sizeof(T))
    {
        if (size == 0) {
            return;
        }

        AssertThrow(Position() + std::streamoff(size) <= Max());

        ReadBytes(reinterpret_cast<char *>(ptr), size);
    }

    /*! \brief Reads from the current position, to current position + \ref{size}.
        If that position is greater than the maximum position, the number of bytes is truncated.
        Endianness is not taken into account
        @returns The number of bytes read */
    SizeType Read(SizeType size, ByteBuffer &out_byte_buffer)
    {
        if (Eof()) {
            return {};
        }
    
        const SizeType read_to_position = MathUtil::Min(
            SizeType(Position() + std::streamoff(size)),
            SizeType(Max())
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
            return {};
        }

        return ReadBytes(SizeType(Max()) - SizeType(Position()));
    }
    
    template <typename T>
    void Peek(T *ptr, SizeType size = sizeof(T))
    {
        Read(ptr, size);
        Rewind(size);
    }

    virtual std::streampos Position() const = 0;
    virtual std::streampos Max() const = 0;
    virtual void Skip(unsigned amount) = 0;
    virtual void Rewind(unsigned long amount) = 0;
    virtual void Seek(unsigned long where_to) = 0;

    bool Eof() const
        { return Position() >= Max(); }

protected:
    virtual void ReadBytes(void *ptr, SizeType size) = 0;
    virtual ByteBuffer ReadBytes(SizeType size) = 0;
};

class MemoryByteReader : public ByteReader
{
public:
    MemoryByteReader(size_t size, const char *data)
        : m_data(data),
          m_size(size),
          m_pos(0)
    {
    }

    virtual ~MemoryByteReader() override = default;

    virtual std::streampos Position() const override
    {
        return m_pos;
    }

    virtual std::streampos Max() const override
    {
        return m_size;
    }

    virtual void Skip(unsigned amount) override
    {
        m_pos += amount;
    }

    virtual void Rewind(unsigned long amount) override
    {
        m_pos -= amount;
    }

    virtual void Seek(unsigned long where_to) override
    {
        m_pos = where_to;
    }

protected:
    const char *m_data;
    SizeType m_size;
    SizeType m_pos;

    virtual void ReadBytes(void *ptr, SizeType size) override
    {
        Memory::Copy(ptr, m_data + m_pos, size);
        m_pos += size;
    }

    virtual ByteBuffer ReadBytes(SizeType size) override
    {
        const auto previous_offset = m_pos;
        m_pos += size;
        return ByteBuffer(size, m_data + previous_offset);
    }
};

class FileByteReader : public ByteReader
{
public:
    FileByteReader(const std::string &filepath, std::streampos begin = 0)
        : filepath(filepath)
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

    virtual ~FileByteReader()
    {
        delete file;
    }
    
    const std::string &GetFilepath() const
    {
        return filepath;
    }

    virtual std::streampos Position() const override
    {
        return pos;
    }

    virtual std::streampos Max() const override
    {
        return max_pos;
    }

    virtual void Skip(unsigned amount) override
    {
        file->seekg(pos += amount);
    }

    virtual void Rewind(unsigned long amount) override
    {
        file->seekg(pos -= amount);
    }

    virtual void Seek(unsigned long where_to) override
    {
        file->seekg(pos = where_to);
    }

protected:
    std::istream *file;
    std::streampos pos;
    std::streampos max_pos;
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
