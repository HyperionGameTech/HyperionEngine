#ifndef BYTE_WRITER_H
#define BYTE_WRITER_H

#include <fstream>
#include <string>

namespace hyperion {
class ByteWriter {
public:
    virtual ~ByteWriter() {}

    template <typename T>
    void Write(T *ptr, unsigned size = sizeof(T))
    {
        WriteBytes(reinterpret_cast<char*>(ptr), size);
    }

    template <typename T>
    void Write(T value, unsigned size = sizeof(T))
    {
        T *ptr = &value;

        WriteBytes(reinterpret_cast<char*>(ptr), size);
    }

    void WriteString(const std::string &str)
    {
        uint32_t len = str.size() + 1;
        char *tmp = new char[len];
        memset(tmp, 0, len);
        memcpy(tmp, str.c_str(), str.size());
        WriteBytes(reinterpret_cast<char*>(&len), sizeof(uint32_t));
        WriteBytes(tmp, len);
        delete[] tmp;
    }

    virtual std::streampos Position() const = 0;
    virtual void Close() = 0;

protected:
    virtual void WriteBytes(char *ptr, unsigned size) = 0;
};

class MemoryByteWriter : public ByteWriter {
public:
    MemoryByteWriter()
        : m_pos(0)
    {
        
    }

    ~MemoryByteWriter()
    {
    }

    std::streampos Position() const
    {
        return std::streampos(m_pos);
    }

    void Close() {}

    inline const std::vector<char> &GetData() const { return m_data; }

private:
    std::vector<char> m_data;
    size_t m_pos;

    void WriteBytes(char *ptr, unsigned size)
    {
        for (size_t i = 0; i < size; i++) {
            m_data.push_back(ptr[i]);
            m_pos++;
        }
    }
};

class FileByteWriter : public ByteWriter {
public:
    FileByteWriter(const std::string &filepath, std::streampos begin = 0)
    {
        file = new std::ofstream(filepath, std::ifstream::out | std::ifstream::binary);
    }

    ~FileByteWriter()
    {
        delete file;
    }

    std::streampos Position() const
    {
        return file->tellp();
    }

    void Close()
    {
        file->close();
    }

private:
    std::ofstream *file;

    void WriteBytes(char *ptr, unsigned size)
    {
        file->write(ptr, size);
    }
};
}

#endif
