/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/FixedArray.hpp>
#include <Core/containers/String.hpp>

#include <Core/memory/ByteBuffer.hpp>

#include <Core/filesystem/FilePath.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/Types.hpp>

#include <type_traits>

namespace Hyperion {

class CORE_API BufferedReaderSource
{
public:
    virtual ~BufferedReaderSource() = default;

    virtual bool IsOK() const = 0;
    virtual size_t Size() const = 0;
    virtual size_t Read(ubyte* ptr, size_t count, size_t offset) = 0;
};

class CORE_API FileBufferedReaderSource : public BufferedReaderSource
{
public:
    /*! \brief Takes ownership of the file pointer to use for reading */
    explicit FileBufferedReaderSource(FILE* file, int (*closeFunc)(FILE*) = nullptr)
        : m_size(0),
          m_file(file),
          m_closeFunc(closeFunc)
    {
        if (m_file)
        {
            std::fseek(m_file, 0, SEEK_END);

            m_size = std::ftell(m_file);

            std::fseek(m_file, 0, SEEK_SET);
        }
    }

    /*! \brief Opens the file at the given path for reading */
    explicit FileBufferedReaderSource(const FilePath& filepath)
        : FileBufferedReaderSource(std::fopen(filepath.Data(), "rb"), nullptr)
    {
    }

    virtual ~FileBufferedReaderSource() override
    {
        if (m_file)
        {
            if (m_closeFunc)
            {
                m_closeFunc(m_file);
            }
            else
            {
                std::fclose(m_file);
            }
        }
    }

    virtual bool IsOK() const override
    {
        return m_file != nullptr && !std::feof(m_file);
    }

    virtual size_t Size() const override
    {
        return m_size;
    }

    virtual size_t Read(ubyte* ptr, size_t count, size_t offset) override
    {
        if (!m_file)
        {
            return 0;
        }

        std::fseek(m_file, long(offset), SEEK_SET);
        return std::fread(ptr, 1, count, m_file);
    }

private:
    size_t m_size;
    FILE* m_file;
    int (*m_closeFunc)(FILE*);
};

class CORE_API MemoryBufferedReaderSource : public BufferedReaderSource
{
public:
    MemoryBufferedReaderSource() = default;

    MemoryBufferedReaderSource(ConstByteView byteView)
        : m_byteView(byteView)
    {
    }

    MemoryBufferedReaderSource(const ByteBuffer& byteBuffer)
        : m_byteView(byteBuffer.ToByteView())
    {
    }

    MemoryBufferedReaderSource(const MemoryBufferedReaderSource& other)
        : m_byteView(other.m_byteView)
    {
    }

    MemoryBufferedReaderSource(MemoryBufferedReaderSource&& other) noexcept
        : m_byteView(std::move(other.m_byteView))
    {
        other.m_byteView = ConstByteView();
    }

    MemoryBufferedReaderSource& operator=(const MemoryBufferedReaderSource& other)
    {
        if (this == &other)
        {
            return *this;
        }

        m_byteView = other.m_byteView;

        return *this;
    }

    MemoryBufferedReaderSource& operator=(MemoryBufferedReaderSource&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_byteView = std::move(other.m_byteView);
        other.m_byteView = ConstByteView();

        return *this;
    }

    virtual ~MemoryBufferedReaderSource() override = default;

    virtual bool IsOK() const override
    {
        return m_byteView.Size() != 0;
    }

    virtual size_t Size() const override
    {
        return m_byteView.Size();
    }

    virtual size_t Read(ubyte* ptr, size_t count, size_t offset) override
    {
        if (offset >= m_byteView.Size())
        {
            return 0;
        }

        const size_t numBytes = MathUtil::Min(count, m_byteView.Size() - offset);
        Memory::Copy(ptr, m_byteView.Data() + offset, numBytes);
        return numBytes;
    }

private:
    ConstByteView m_byteView;
};

class CORE_API BufferedReader
{
public:
    static constexpr size_t bufferSize = 2048;
    static constexpr size_t eofPos = ~0u;

    BufferedReader()
        : m_pos(eofPos),
          m_source(nullptr),
          m_buffer {}
    {
    }

    BufferedReader(BufferedReaderSource* source)
        : m_source(source),
          m_pos(eofPos),
          m_buffer {}
    {
        if (m_source != nullptr && m_source->IsOK())
        {
            Seek(0);
        }
    }

    BufferedReader(const BufferedReader& other) = delete;
    BufferedReader& operator=(const BufferedReader& other) = delete;

    BufferedReader(BufferedReader&& other) noexcept
        : m_filepath(std::move(other.m_filepath)),
          m_source(other.m_source),
          m_pos(other.m_pos),
          m_buffer(std::move(other.m_buffer))
    {
        other.m_pos = eofPos;
        other.m_source = nullptr;
    }

    BufferedReader& operator=(BufferedReader&& other) noexcept
    {
        if (&other == this)
        {
            return *this;
        }

        Close();

        m_filepath = std::move(other.m_filepath);
        m_source = other.m_source;
        m_pos = other.m_pos;
        m_buffer = std::move(other.m_buffer);

        other.m_pos = eofPos;
        other.m_source = nullptr;

        return *this;
    }

    ~BufferedReader()
    {
        Close();
    }

    HYP_FORCE_INLINE BufferedReaderSource* GetSource() const
    {
        return m_source;
    }

    /*! \brief Returns a boolean indicating whether or not the file could be opened without issue */
    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsOpen();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !IsOpen();
    }

    HYP_FORCE_INLINE const FilePath& GetFilepath() const
    {
        return m_filepath;
    }

    /*! \brief Returns a boolean indicating whether or not the file could be opened without issue */
    HYP_FORCE_INLINE bool IsOpen() const
    {
        return m_source != nullptr && m_source->IsOK();
    }

    HYP_FORCE_INLINE size_t Position() const
    {
        return m_pos;
    }

    HYP_FORCE_INLINE size_t Max() const
    {
        return m_source != nullptr ? m_source->Size() : 0;
    }

    HYP_FORCE_INLINE bool Eof() const
    {
        return m_source == nullptr || m_pos >= m_source->Size();
    }

    HYP_FORCE_INLINE void Rewind(unsigned long amount)
    {
        if (amount > m_pos)
        {
            m_pos = 0;
        }
        else
        {
            m_pos -= amount;
        }
    }

    HYP_FORCE_INLINE void Skip(unsigned long amount)
    {
        if (Eof())
        {
            return;
        }

        m_pos += amount;
    }

    HYP_FORCE_INLINE void Seek(unsigned long whereTo)
    {
        m_pos = whereTo;
    }

    HYP_FORCE_INLINE void Close()
    {
        m_pos = eofPos;
        m_source = nullptr;
    }

    /*! \brief Reads the next \p count bytes from the file and returns a ByteBuffer.
        If position + count is greater than the number of remaining bytes, the ByteBuffer is truncated. */
    ByteBuffer ReadBytes(size_t count)
    {
        if (Eof())
        {
            return {};
        }

        const size_t remaining = m_source->Size() - m_pos;
        const size_t toRead = MathUtil::Min(remaining, count);

        ByteBuffer byteBuffer(toRead);
        m_source->Read(byteBuffer.Data(), toRead, m_pos);
        m_pos += toRead;

        return byteBuffer;
    }

    /*! \brief Reads the entirety of the remaining bytes in file and returns ByteBuffer.
     * Note that using this method to read the whole file in one call bypasses
     * the intention of having a buffered reader! */
    ByteBuffer ReadBytes()
    {
        if (Eof())
        {
            return {};
        }

        const size_t remaining = m_source->Size() - m_pos;

        ByteBuffer byteBuffer(remaining);
        m_source->Read(byteBuffer.Data(), remaining, m_pos);
        m_pos += remaining;

        return byteBuffer;
    }

    /*! \brief Attempts to read \p count bytes from the file into
        \p ptr. If size is greater than the number of remaining bytes,
        it is capped to (num remaining bytes).
        @returns The number of bytes read */
    size_t ReadBytes(ubyte* ptr, size_t count)
    {
        if (Eof())
        {
            return 0;
        }

        const size_t remaining = m_source->Size() - m_pos;
        const size_t toRead = MathUtil::Min(remaining, count);

        m_source->Read(ptr, toRead, m_pos);
        m_pos += toRead;

        return toRead;
    }

    /*! \brief Reads the entirety of the remaining bytes in file PER LINE and returns a vector of
     * string. Note that using this method to read the whole file in one call bypasses
     * the intention of having a buffered reader. */
    Array<String> ReadAllLines()
    {
        if (Eof())
        {
            return {};
        }

        Array<String> lines;

        ReadLines([&lines](const String& line, bool*)
            {
                lines.PushBack(line);
            });

        return lines;
    }

    size_t Read(ByteBuffer& byteBuffer)
    {
        return Read(byteBuffer.Data(), byteBuffer.Size(), [](void* ptr, const ubyte* buffer, size_t chunkSize)
            {
                Memory::Copy(ptr, buffer, chunkSize);
            });
    }

    size_t Read(void* ptr, size_t count)
    {
        return Read(ptr, count, [](void* ptr, const ubyte* buffer, size_t chunkSize)
            {
                Memory::Copy(ptr, buffer, chunkSize);
            });
    }

    /*! \returns The total number of bytes read */
    template <class Lambda = std::add_pointer_t<void(void*, const ubyte*, size_t)>>
    size_t Read(void* ptr, size_t count, Lambda&& func)
    {
        if (Eof())
        {
            return 0;
        }

        size_t totalRead = 0;

        while (count)
        {
            const size_t chunkRequested = MathUtil::Min(count, bufferSize);
            const size_t chunkReturned = Read(chunkRequested);
            const size_t offset = totalRead;

            func(static_cast<ubyte*>(ptr) + offset, &m_buffer[0], chunkReturned);

            totalRead += chunkReturned;

            if (chunkReturned < chunkRequested)
            {
                /* File ended */
                break;
            }

            count -= chunkReturned;
        }

        return totalRead;
    }

    /*! \brief Read one instance of the given template type, using sizeof(T).
     *  \param ptr The pointer to T, where the read memory will be written.
     *  \returns The number of bytes read */
    template <class T>
    size_t Read(T* ptr)
    {
        return Read(static_cast<void*>(ptr), sizeof(T));
    }

    template <class T>
    size_t Peek(T* ptr) const
    {
        return Peek(static_cast<void*>(ptr), sizeof(T));
    }

    template <class LambdaFunction>
    void ReadLines(LambdaFunction&& func, bool buffered = true)
    {
        if (Eof())
        {
            return;
        }

        bool stop = false;
        size_t totalRead = 0;
        size_t totalProcessed = 0;

        if (!buffered)
        { // not buffered, do it in one pass
            const ByteBuffer allBytes = ReadBytes();
            totalRead = allBytes.Size();

            String accum;
            accum.Reserve(bufferSize);

            for (size_t i = 0; i < allBytes.Size(); i++)
            {
                if (allBytes[i] == '\n')
                {
                    func(accum, &stop);
                    totalProcessed += accum.Size() + 1;

                    if (stop)
                    {
                        const size_t amountRemaining = totalRead - totalProcessed;

                        if (amountRemaining != 0)
                        {
                            Rewind(amountRemaining);
                        }

                        return;
                    }

                    accum.Clear();

                    continue;
                }

                accum.Append(char(allBytes[i]));
            }

            if (accum.Any())
            {
                func(accum, &stop);
                totalProcessed += accum.Size();
            }

            return;
        }

        // if there is no newline in buffer:
        // accumulate chars until a newline is found, then
        // call func with the accumulated line
        // if there is at least one newline in buffer:
        // call func for each line in the buffer,
        // holding onto the last one (assuming no newline at the end)
        // keep that last line and continue accumulating until a newline is found
        String accum;
        accum.Reserve(bufferSize);

        ByteBuffer byteBuffer;

        while ((byteBuffer = ReadBytes(bufferSize)).Any())
        {
            totalRead += byteBuffer.Size();

            for (size_t i = 0; i < byteBuffer.Size(); i++)
            {
                if (byteBuffer[i] == '\n')
                {
                    func(accum, &stop);
                    totalProcessed += accum.Size() + 1;

                    if (stop)
                    {
                        const size_t amountRemaining = totalRead - totalProcessed;

                        if (amountRemaining != 0)
                        {
                            Rewind(amountRemaining);
                        }

                        return;
                    }

                    accum.Clear();

                    continue;
                }

                accum.Append(char(byteBuffer[i]));
            }
        }

        if (accum.Any())
        {
            func(accum, &stop);
            totalProcessed += accum.Size();
        }
    }

    template <class LambdaFunction>
    void ReadChars(LambdaFunction func)
    {
        while (size_t count = Read())
        {
            for (size_t i = 0; i < count; i++)
            {
                func(char(m_buffer[i]));
            }
        }
    }

private:
    FilePath m_filepath;
    BufferedReaderSource* m_source;
    size_t m_pos;
    FixedArray<ubyte, bufferSize> m_buffer;

    size_t Read()
    {
        if (Eof())
        {
            return 0;
        }

        const size_t count = m_source->Read(&m_buffer[0], bufferSize, m_pos);

        m_pos += count;

        return count;
    }

    size_t Read(size_t sz)
    {
        HYP_CORE_ASSERT(sz <= bufferSize);

        if (Eof())
        {
            return 0;
        }

        const size_t count = m_source->Read(&m_buffer[0], sz, m_pos);
        m_pos += count;

        return count;
    }

    size_t Peek(void* dest, size_t sz) const
    {
        if (Eof())
        {
            return 0;
        }

        HYP_CORE_ASSERT(m_pos + sz <= m_source->Size(),
            "Attempt to read past end of file: %zu + %zu > %zu",
            m_pos, sz, m_source->Size());

        return m_source->Read(static_cast<ubyte*>(dest), sz, m_pos);
    }
};

using BufferedByteReader = BufferedReader;

} // namespace Hyperion
