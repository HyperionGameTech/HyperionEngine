#ifndef BYTE_WRITER_H
#define BYTE_WRITER_H

#include <core/lib/CMemory.hpp>
#include <core/lib/String.hpp>
#include <core/lib/DynArray.hpp>
#include <Types.hpp>

#include <type_traits>
#include <fstream>
#include <string>

namespace hyperion {
class ByteWriter
{
public:
    ByteWriter() = default;
    ByteWriter(const ByteWriter &other) = delete;
    ByteWriter &operator=(const ByteWriter &other) = delete;
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
        const auto len = Memory::StrLen(str);

        WriteBytes(reinterpret_cast<const char *>(&len), sizeof(uint32_t));
        WriteBytes(str, len);
    }

    virtual std::streampos Position() const = 0;
    virtual void Close() = 0;

protected:
    virtual void WriteBytes(const char *ptr, SizeType size) = 0;
};

// TEMP
class MemoryByteWriter : public ByteWriter
{
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

    const Array<UByte> &GetData() const { return m_data; }

private:
    Array<UByte> m_data;
    SizeType m_pos;

    void WriteBytes(const char *ptr, SizeType size)
    {
        m_data.Reserve(m_data.Size() + size);

        for (SizeType i = 0; i < size; i++) {
            m_data.PushBack(ptr[i]);
            m_pos++;
        }
    }
};

class FileByteWriter : public ByteWriter
{
public:
    FileByteWriter(const std::string &filepath, std::streampos begin = 0)
    {
        file = new std::ofstream(filepath, std::ofstream::out | std::ofstream::binary);
    }

    FileByteWriter(const FileByteWriter &other) = delete;
    FileByteWriter &operator=(const FileByteWriter &other) = delete;

    FileByteWriter(FileByteWriter &&other) noexcept
        : file(other.file)
    {
        other.file = nullptr;
    }

    FileByteWriter &operator=(FileByteWriter &&other) noexcept
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        if (file) {
            delete file;
        }

        file = other.file;
        other.file = nullptr;

        return *this;
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

    bool IsOpen() const
        { return file->is_open(); }

private:
    std::ofstream *file;

    void WriteBytes(const char *ptr, SizeType size)
    {
        file->write(ptr, size);
    }
};
}

#endif
