/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef BUFFERED_BYTE_READER_HPP
#define BUFFERED_BYTE_READER_HPP

#include <math/MathUtil.hpp>

#include <Types.hpp>

#include <core/Defines.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/String.hpp>
#include <core/memory/ByteBuffer.hpp>
#include <core/filesystem/FilePath.hpp>

#include <fstream>
#include <type_traits>

namespace hyperion {

class BufferedReaderSource
{
public:
    virtual ~BufferedReaderSource() = default;

    virtual bool IsOK() const = 0;
    virtual SizeType Size() const = 0;
    virtual SizeType Read(ubyte *ptr, SizeType count, SizeType offset) = 0;
};

class FileBufferedReaderSource : public BufferedReaderSource
{
public:
    FileBufferedReaderSource(const FilePath &filepath)
    {
        m_ifstream = new std::ifstream(filepath.Data(), std::ifstream::in | std::ifstream::ate | std::ifstream::binary);

        m_size = m_ifstream->tellg();
        m_ifstream->seekg(0);
    }

    virtual ~FileBufferedReaderSource() override
    {
        delete m_ifstream;
    }

    virtual bool IsOK() const override
        { return m_ifstream != nullptr && m_ifstream->good(); }

    virtual SizeType Size() const override
        { return m_size; }

    virtual SizeType Read(ubyte *ptr, SizeType count, SizeType offset) override
    {
        m_ifstream->seekg(offset);
        m_ifstream->read(reinterpret_cast<char *>(ptr), count);
        return m_ifstream->gcount();
    }

private:
    SizeType        m_size;
    std::ifstream   *m_ifstream;
};

class MemoryBufferedReaderSource : public BufferedReaderSource
{
public:
    MemoryBufferedReaderSource(ByteBuffer buffer)
        : m_buffer(std::move(buffer))
    {
    }

    virtual ~MemoryBufferedReaderSource() override = default;

    virtual bool IsOK() const override
        { return m_buffer.Any(); }

    virtual SizeType Size() const override
        { return m_buffer.Size(); }

    virtual SizeType Read(ubyte *ptr, SizeType count, SizeType offset) override
    {
        if (offset >= m_buffer.Size()) {
            return 0;
        }

        const SizeType num_bytes = MathUtil::Min(count, m_buffer.Size() - offset);
        Memory::MemCpy(ptr, m_buffer.Data() + offset, num_bytes);
        return num_bytes;
    }

private:
    ByteBuffer  m_buffer;
};

class BufferedReader
{
public:
    using Byte = ubyte;

    static constexpr SizeType buffer_size = 2048;
    static constexpr uint32 eof_pos = ~0u;

    BufferedReader()
        : pos(eof_pos)
    {
    }

    BufferedReader(RC<BufferedReaderSource> source)
        : source(std::move(source)),
          pos(eof_pos)
    {
        if (this->source->IsOK()) {
            Seek(0);
        }
    }

    BufferedReader(const FilePath &filepath)
        : filepath(filepath),
          source(new FileBufferedReaderSource(filepath)),
          pos(eof_pos)
    {
        if (source->IsOK()) {
            Seek(0);
        }
    }

    BufferedReader(const BufferedReader &other)             = delete;
    BufferedReader &operator=(const BufferedReader &other)  = delete;

    BufferedReader(BufferedReader &&other) noexcept
        : filepath(std::move(other.filepath)),
          source(std::move(other.source)),
          pos(other.pos),
          buffer(std::move(other.buffer))
    {
        other.pos = eof_pos;
    }

    BufferedReader &operator=(BufferedReader &&other) noexcept
    {
        if (&other == this) {
            return *this;
        }

        Close();

        filepath = std::move(other.filepath);
        source = std::move(other.source);
        pos = other.pos;
        buffer = std::move(other.buffer);
        
        other.pos = eof_pos;

        return *this;
    }

    ~BufferedReader()
    {
        Close();
    }

    /*! \brief Returns a boolean indicating whether or not the file could be opened without issue */
    explicit operator bool() const
        { return IsOpen(); }

    const FilePath &GetFilepath() const
        { return filepath; }

    /*! \brief Returns a boolean indicating whether or not the file could be opened without issue */
    bool IsOpen() const
        { return source != nullptr && source->IsOK(); }

    SizeType Position() const
        { return pos; }

    SizeType Max() const
        { return source != nullptr ? source->Size() : 0; }

    bool Eof() const
        { return source == nullptr || pos >= source->Size(); }

    void Rewind(unsigned long amount)
    {
        if (amount > pos) {
            pos = 0;
        } else {
            pos -= amount;
        }
    }

    void Skip(unsigned long amount)
    {
        if (Eof()) {
            return;
        }

        pos += amount;
    }

    void Seek(unsigned long where_to)
    {
        pos = where_to;
    }

    void Close()
    {
        pos = eof_pos;

        source.Reset();
    }

    /*! \brief Reads the next \ref{count} bytes from the file and returns a ByteBuffer.
        If position + count is greater than the number of remaining bytes, the ByteBuffer is truncated. */
    ByteBuffer ReadBytes(SizeType count)
    {
        if (Eof()) {
            return { };
        }

        const SizeType remaining = source->Size() - pos;
        const SizeType to_read = MathUtil::Min(remaining, count);

        ByteBuffer byte_buffer(to_read);
        source->Read(byte_buffer.Data(), to_read, pos);
        pos += to_read;

        return byte_buffer;
    }

    /*! \brief Reads the entirety of the remaining bytes in file and returns ByteBuffer.
     * Note that using this method to read the whole file in one call bypasses
     * the intention of having a buffered reader! */
    ByteBuffer ReadBytes()
    {
        if (Eof()) {
            return { };
        }

        const SizeType remaining = source->Size() - pos;

        ByteBuffer byte_buffer(remaining);
        source->Read(byte_buffer.Data(), remaining, pos);
        pos += remaining;

        return byte_buffer;
    }

    /*! \brief Attempts to read \ref{count} bytes from the file into
        \ref{ptr}. If size is greater than the number of remaining bytes,
        it is capped to (num remaining bytes).
        @returns The number of bytes read */
    SizeType ReadBytes(Byte *ptr, SizeType count)
    {
        if (Eof()) {
            return 0;
        }

        const SizeType remaining = source->Size() - pos;
        const SizeType to_read = MathUtil::Min(remaining, count);

        source->Read(ptr, to_read, pos);
        pos += to_read;

        return to_read;
    }

    /*! \brief Reads the entirety of the remaining bytes in file PER LINE and returns a vector of
     * string. Note that using this method to read the whole file in one call bypasses
     * the intention of having a buffered reader. */
    Array<String> ReadAllLines()
    {
        if (Eof()) {
            return { };
        }
        
        Array<String> lines;

        ReadLines([&lines](const String &line, bool *)
        {
            lines.PushBack(line);
        });

        return lines;
    }

    SizeType Read(ByteBuffer &byte_buffer)
    {
        return Read(byte_buffer.Data(), byte_buffer.Size(), [](void *ptr, const Byte *buffer, SizeType chunk_size)
        {
            Memory::MemCpy(ptr, buffer, chunk_size);
        });
    }

    SizeType Read(void *ptr, SizeType count)
    {
        return Read(ptr, count, [](void *ptr, const Byte *buffer, SizeType chunk_size)
        {
            Memory::MemCpy(ptr, buffer, chunk_size);
        });
    }

    /*! \returns The total number of bytes read */
    template <class Lambda = std::add_pointer_t<void(void *, const Byte *, SizeType)>>
    SizeType Read(void *ptr, SizeType count, Lambda &&func)
    {
        if (Eof()) {
            return 0;
        }

        SizeType total_read = 0;

        while (count) {
            const SizeType chunk_requested = MathUtil::Min(count, buffer_size);
            const SizeType chunk_returned = Read(chunk_requested);
            const SizeType offset = total_read;

            func(reinterpret_cast<void *>(uintptr_t(ptr) + offset), &buffer[0], chunk_returned);

            total_read += chunk_returned;

            if (chunk_returned < chunk_requested) {
                /* File ended */
                break;
            }

            count -= chunk_returned;
        }

        return total_read;
    }

    /*! \brief Read one instance of the given template type, using sizeof(T).
     * @param ptr The pointer to T, where the read memory will be written.
     * @returns The number of bytes read
     */
    template <class T>
    SizeType Read(T *ptr)
    {
        return Read(static_cast<void *>(ptr), sizeof(T));
    }

    template <class T>
    SizeType Peek(T *ptr) const
    {
        return Peek(static_cast<void *>(ptr), sizeof(T));
    }

    template <class LambdaFunction>
    void ReadLines(LambdaFunction &&func, bool buffered = true)
    {
        if (Eof()) {
            return;
        }

        bool stop = false;
        uint32 total_read = 0;
        uint32 total_processed = 0;

        if (!buffered) { // not buffered, do it in one pass
            const ByteBuffer all_bytes = ReadBytes();
            total_read = all_bytes.Size();

            String accum;
            accum.Reserve(buffer_size);
            
            for (uint32 i = 0; i < all_bytes.Size(); i++) {
                if (all_bytes[i] == '\n') {
                    func(accum, &stop);
                    total_processed += accum.Size() + 1;

                    if (stop) {
                        const uint32 amount_remaining = total_read - total_processed;

                        if (amount_remaining != 0) {
                            Rewind(amount_remaining);
                        }

                        return;
                    }

                    accum.Clear();
                    
                    continue;
                }

                accum.Append(static_cast<char>(all_bytes[i]));
            }

            if (accum.Any()) {
                func(accum, &stop);
                total_processed += accum.Size();
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
        accum.Reserve(buffer_size);

        ByteBuffer byte_buffer;

        while ((byte_buffer = ReadBytes(buffer_size)).Any()) {
            total_read += byte_buffer.Size();

            for (uint32 i = 0; i < byte_buffer.Size(); i++) {
                if (byte_buffer[i] == '\n') {
                    func(accum, &stop);
                    total_processed += accum.Size() + 1;

                    if (stop) {
                        const uint32 amount_remaining = total_read - total_processed;

                        if (amount_remaining != 0) {
                            Rewind(amount_remaining);
                        }

                        return;
                    }

                    accum.Clear();
                    
                    continue;
                }

                accum.Append(static_cast<char>(byte_buffer[i]));
            }
        }

        if (accum.Any()) {
            func(accum, &stop);
            total_processed += accum.Size();
        }
    }

    template <class LambdaFunction>
    void ReadChars(LambdaFunction func)
    {
        while (SizeType count = Read()) {
            for (SizeType i = 0; i < count; i++) {
                func(static_cast<char>(buffer[i]));
            }
        }
    }

private:
    FilePath                        filepath;
    RC<BufferedReaderSource>        source;
    SizeType                        pos;
    FixedArray<Byte, buffer_size>   buffer{};

    SizeType Read()
    {
        if (Eof()) {
            return 0;
        }

        const SizeType count = source->Read(&buffer[0], buffer_size, pos);
        AssertThrow(count <= buffer_size);

        pos += count;

        return count;
    }

    SizeType Read(SizeType sz)
    {
        AssertThrow(sz <= buffer_size);

        if (Eof()) {
            return 0;
        }

        const SizeType count = source->Read(&buffer[0], sz, pos);
        pos += count;

        return count;
    }

    SizeType Peek(void *dest, SizeType sz) const
    {
        if (Eof()) {
            return 0;
        }

        AssertThrowMsg(pos + sz <= source->Size(),
             "Attempt to read past end of file: %llu + %llu > %llu",
              pos,
              sz,
              source->Size());

        return source->Read(static_cast<ubyte *>(dest), sz, pos);
    }
};

using BufferedByteReader = BufferedReader;

} // namespace hyperion

#endif
