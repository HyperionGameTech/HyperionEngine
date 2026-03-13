#include <Core/io/ByteReader.hpp>

namespace Hyperion {

#pragma region MemoryByteReader

size_t MemoryByteReader::Read(void* ptr, size_t size)
{
    if (!m_byteView)
    {
        return 0;
    }

    size_t toRead = MathUtil::Min(m_byteView.Size() - m_pos, size);

    Memory::Copy(ptr, m_byteView.Data() + m_pos, toRead);
    m_pos += toRead;

    return toRead;
}

ByteBuffer MemoryByteReader::Read(size_t size)
{
    if (!m_byteView)
    {
        return ByteBuffer();
    }

    const auto previousOffset = m_pos;
    m_pos += size;
    return ByteBuffer(size, m_byteView.Data() + previousOffset);
}

void MemoryByteReader::Close()
{
    // do nothing
}

#pragma endregion MemoryByteReader

#pragma region FileByteReader

FileByteReader::FileByteReader(const FilePath& filepath, size_t offset)
    : m_file(nullptr),
        m_pos(0),
        m_maxPos(0),
        m_filepath(filepath)
{
    m_file = std::fopen(filepath.Data(), "rb");

    if (m_file == nullptr)
    {
        m_maxPos = 0;
        m_pos = 0;

        return;
    }

    // determine file size
    if (std::fseek(m_file, 0, SEEK_END) == 0)
    {
        long endPos = std::ftell(m_file);

        if (endPos >= 0)
        {
            m_maxPos = static_cast<size_t>(endPos);
        }
        else
        {
            m_maxPos = 0;
        }
    }
    else
    {
        m_maxPos = 0;
    }

    // seek to beginning offset
    if (offset > m_maxPos)
    {
        offset = m_maxPos;
    }

    std::fseek(m_file, static_cast<long>(offset), SEEK_SET);

    long cur = std::ftell(m_file);
    m_pos = cur >= 0 ? static_cast<size_t>(cur) : 0;
}

FileByteReader::~FileByteReader()
{
    if (m_file != nullptr)
    {
        std::fclose(m_file);
        m_file = nullptr;
    }
}

void FileByteReader::Skip(size_t amount)
{
    if (m_file == nullptr)
    {
        return;
    }

    m_pos += amount;

    if (m_pos > m_maxPos)
    {
        m_pos = m_maxPos;
    }

    std::fseek(m_file, static_cast<long>(m_pos), SEEK_SET);
}

void FileByteReader::Rewind(size_t amount)
{
    if (m_file == nullptr)
    {
        return;
    }

    if (amount > m_pos)
    {
        m_pos = 0;
    }
    else
    {
        m_pos -= amount;
    }

    std::fseek(m_file, static_cast<long>(m_pos), SEEK_SET);
}

void FileByteReader::Seek(size_t whereTo)
{
    if (!m_file)
    {
        return;
    }

    if (whereTo > m_maxPos)
    {
        whereTo = m_maxPos;
    }

    m_pos = whereTo;
    std::fseek(m_file, static_cast<long>(m_pos), SEEK_SET);
}

size_t FileByteReader::Read(void* ptr, size_t size)
{
    if (m_file == nullptr || size == 0)
    {
        return 0;
    }

    const size_t remaining = m_maxPos > m_pos ? (m_maxPos - m_pos) : 0;
    const size_t toRead = MathUtil::Min(size, remaining);

    if (toRead == 0)
    {
        return 0;
    }

    const size_t readBytes = std::fread(ptr, 1, toRead, m_file);
    m_pos += readBytes;

    return toRead;
}

ByteBuffer FileByteReader::Read(size_t size)
{
    if (m_file == nullptr)
    {
        return ByteBuffer();
    }

    const size_t remaining = m_maxPos > m_pos ? (m_maxPos - m_pos) : 0;
    const size_t toRead = MathUtil::Min(size, remaining);

    if (toRead == 0)
    {
        return ByteBuffer();
    }

    ByteBuffer byteBuffer;
    byteBuffer.SetSize(toRead);

    const size_t readBytes = std::fread(byteBuffer.Data(), 1, toRead, m_file);
    m_pos += readBytes;

    if (readBytes == toRead)
    {
        return byteBuffer;
    }

    return ByteBuffer(readBytes, byteBuffer.Data());
}

void FileByteReader::Close()
{
    if (m_file != nullptr)
    {
        std::fclose(m_file);
        m_file = nullptr;
    }
}

#pragma endregion FileByteReader

#pragma region MemoryMappedByteReader

MemoryMappedByteReader::MemoryMappedByteReader(
    MemoryMappedFile* mappedFile,
    size_t offset,
    size_t size,
    MemoryMappedFile::Mode mode)
    : m_mappedFile(mappedFile),
      m_pos(0),
      m_ownsFile(false)
{
    HYP_CORE_ASSERT(mappedFile != nullptr);

    const bool opened = m_mappedFile->Open();
    HYP_CORE_ASSERT(opened, "Failed to open memory mapped file!");

    const bool mapped = m_mappedFile->MapRange(offset, 0, m_mappedView);
    HYP_CORE_ASSERT(mapped, "Failed to map memory range");
}

MemoryMappedByteReader::MemoryMappedByteReader(
    const FilePath& filepath,
    size_t offset,
    MemoryMappedFile::Mode mode)
    : m_mappedFile(new MemoryMappedFile(filepath, mode)),
      m_pos(0),
      m_ownsFile(true)
{
    const bool opened = m_mappedFile->Open();
    HYP_CORE_ASSERT(opened, "Failed to open memory mapped file: %s", filepath.Data());

    const bool mapped = m_mappedFile->MapRange(offset, 0, m_mappedView);
    HYP_CORE_ASSERT(mapped, "Failed to map memory range: %s", filepath.Data());
}

MemoryMappedByteReader::MemoryMappedByteReader(
    const FilePath& filepath,
    size_t offset,
    size_t size,
    MemoryMappedFile::Mode mode)
    : m_mappedFile(new MemoryMappedFile(filepath, mode)),
      m_pos(0),
      m_ownsFile(true)
{
    const bool opened = m_mappedFile->Open();
    HYP_CORE_ASSERT(opened, "Failed to open memory mapped file: %s", filepath.Data());

    const bool mapped = m_mappedFile->MapRange(offset, size, m_mappedView);
    HYP_CORE_ASSERT(mapped, "Failed to map memory range: %s", filepath.Data());
}

MemoryMappedByteReader::~MemoryMappedByteReader()
{
    MemoryMappedByteReader::Close();
}

size_t MemoryMappedByteReader::Position() const
{
    return m_pos;
}

size_t MemoryMappedByteReader::Max() const
{
    return m_mappedView.Size();
}

void MemoryMappedByteReader::Skip(size_t amount)
{
    m_pos = MathUtil::Min(m_pos + amount, Max());
}

void MemoryMappedByteReader::Rewind(size_t amount)
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

void MemoryMappedByteReader::Seek(size_t whereTo)
{
    if (whereTo > Max())
    {
        whereTo = Max();
    }

    m_pos = whereTo;
}

void MemoryMappedByteReader::Close()
{
    if (IsOpen())
    {
        m_mappedView.Close();

        if (m_ownsFile)
        {
            m_mappedFile->Close();

            delete m_mappedFile;
        }
    }
        
    m_mappedFile = nullptr;
}

bool MemoryMappedByteReader::IsOpen() const
{
    return m_mappedFile
        && m_mappedFile->IsOpen()
        && m_mappedView.IsOpen();
}

MemoryMappedFile::Mode MemoryMappedByteReader::GetMode() const
{
    return m_mappedFile
        ? m_mappedFile->GetMode()
        : MemoryMappedFile::Mode::READ_ONLY;
}

size_t MemoryMappedByteReader::Read(void* ptr, size_t size)
{
    if (size == 0)
    {
        return 0;
    }

    const size_t remaining = Max() > m_pos ? (Max() - m_pos) : 0;
    const size_t toRead = MathUtil::Min(size, remaining);

    if (toRead == 0)
    {
        return 0;
    }

    const auto* data = static_cast<const ubyte*>(m_mappedView.Data());

    if (data == nullptr)
    {
        return 0;
    }

    Memory::Copy(ptr, data + m_pos, toRead);
    m_pos += toRead;

    return toRead;
}

ByteBuffer MemoryMappedByteReader::Read(size_t size)
{
    const size_t remaining = Max() > m_pos ? (Max() - m_pos) : 0;
    const size_t toRead = MathUtil::Min(size, remaining);

    if (toRead == 0)
    {
        return ByteBuffer();
    }

    const auto* data = static_cast<const ubyte*>(m_mappedView.Data());

    if (data == nullptr)
    {
        return ByteBuffer();
    }

    const size_t previousOffset = m_pos;
    m_pos += toRead;

    return ByteBuffer(toRead, data + previousOffset);
}

#pragma endregion MemoryMappedByteReader

} // namespace Hyperion