#ifndef BYTE_WRITER_H
#define BYTE_WRITER_H

#include <core/lib/CMemory.hpp>
#include <core/lib/String.hpp>
#include <Types.hpp>

#include <type_traits>
#include <fstream>
#include <string>

namespace hyperion {
class ByteWriter {
public:
    virtual ~ByteWriter() {}

    void Write(const void *ptr, SizeType size)
    {
        WriteBytes(reinterpret_cast<const char *>(ptr), size);
    }

    template <class T>
    void Write(const T &value)
    {
        static_assert(!std::is_pointer_v<T>, "Expected to choose other overload");

        WriteBytes(reinterpret_cast<const char *>(&value), sizeof(T));
    }

    void WriteString(const String &str)
    {
        const auto len = static_cast<UInt32>(str.Size());

        WriteBytes(reinterpret_cast<const char *>(&len), sizeof(UInt32));
        WriteBytes(str.Data(), len);
    }

    void WriteString(const char *str)
    {
        const auto len = Memory::StringLength(str);

        char *tmp = new char[len + 1];
        tmp[len] = '\0';
        Memory::Copy(tmp, str, len);
        WriteBytes(reinterpret_cast<const char *>(&len), sizeof(uint32_t));
        WriteBytes(tmp, len);
        delete[] tmp;
    }

    virtual std::streampos Position() const = 0;
    virtual void Close() = 0;

protected:
    virtual void WriteBytes(const char *ptr, SizeType size) = 0;
};

// TEMP
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

    void Close()
    {
    }

    const std::vector<char> &GetData() const { return m_data; }

private:
    std::vector<char> m_data;
    size_t m_pos;

    void WriteBytes(const char *ptr, SizeType size)
    {
        for (SizeType i = 0; i < size; i++) {
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

    void WriteBytes(const char *ptr, SizeType size)
    {
        file->write(ptr, size);
    }
};
}

#endif
