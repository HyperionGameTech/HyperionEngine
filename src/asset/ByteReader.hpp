#ifndef BYTE_READER_H
#define BYTE_READER_H

#include "../Types.hpp"
#include "../Util.hpp"

#include <fstream>
#include <cstring>

namespace hyperion {
class ByteReader {
public:
    using Bytes = std::vector<UByte>;

    virtual ~ByteReader() {}

    template <typename T>
    void Read(T *ptr, size_t size = sizeof(T))
    {
        if (size == 0) {
            return;
        }

        AssertThrow(Position() + std::streamoff(size) <= Max());

        ReadBytes(reinterpret_cast<char*>(ptr), size);
    }

    inline Bytes Read()
    {
        if (Eof()) {
            return {};
        }

        const size_t max(Max());

        Bytes data(max);

        Read(data.data(), data.size());

        return data;
    }
    
    template <typename T>
    void Peek(T *ptr, size_t size = sizeof(T))
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
    {
        return Position() >= Max();
    }

protected:
    virtual void ReadBytes(char *ptr, size_t size) = 0;
};

class MemoryByteReader : public ByteReader {
public:
    MemoryByteReader(size_t size, const char *data)
        : m_data(data),
          m_size(size),
          m_pos(0)
    {
    }

    ~MemoryByteReader()
    {
    }

    std::streampos Position() const
    {
        return m_pos;
    }

    std::streampos Max() const
    {
        return m_size;
    }

    void Skip(unsigned amount)
    {
        m_pos += amount;
    }

    void Rewind(unsigned long amount)
    {
        m_pos -= amount;
    }

    void Seek(unsigned long where_to)
    {
        m_pos = where_to;
    }

private:
    const char *m_data;
    size_t m_size;
    size_t m_pos;

    void ReadBytes(char *ptr, size_t size)
    {
        memcpy(ptr, m_data + m_pos, size);
        m_pos += size;
    }
};

class FileByteReader : public ByteReader {
public:
    FileByteReader(const std::string &filepath, std::streampos begin = 0)
    {
        file = new std::ifstream(filepath, std::ifstream::in |
            std::ifstream::binary |
            std::ifstream::ate);

        max_pos = file->tellg();
        file->seekg(begin);
        pos = file->tellg();
    }

    ~FileByteReader()
    {
        delete file;
    }

    std::streampos Position() const
    {
        return pos;
    }

    std::streampos Max() const
    {
        return max_pos;
    }

    void Skip(unsigned amount)
    {
        file->seekg(pos += amount);
    }

    void Rewind(unsigned long amount)
    {
        file->seekg(pos -= amount);
    }

    void Seek(unsigned long where_to)
    {
        file->seekg(pos = where_to);
    }

private:
    std::istream *file;
    std::streampos pos;
    std::streampos max_pos;

    void ReadBytes(char *ptr, size_t size)
    {
        file->read(ptr, size);
        pos += size;
    }
};
}

#endif
