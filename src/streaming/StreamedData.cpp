#include <streaming/StreamedData.hpp>
#include <streaming/DataStore.hpp>
#include <core/lib/StaticString.hpp>
#include <asset/BufferedByteReader.hpp>

namespace hyperion::v2 {

bool NullStreamingData::IsNull() const
{
    return true;
}

bool NullStreamingData::IsInMemory() const
{
    return false;
}

void NullStreamingData::Unpage()
{
    // Do nothing
}

ByteBuffer NullStreamingData::Load() const
{
    return ByteBuffer(0, nullptr);
}


MemoryStreamedData::MemoryStreamedData(const ByteBuffer &byte_buffer)
    : m_byte_buffer(byte_buffer),
      m_is_in_memory(true),
      m_hash_code { 0 }
{
    
}

MemoryStreamedData::MemoryStreamedData(ByteBuffer &&byte_buffer)
    : m_byte_buffer(std::move(byte_buffer)),
      m_is_in_memory(true),
      m_hash_code { 0 }
{
}

MemoryStreamedData::MemoryStreamedData(MemoryStreamedData &&other) noexcept
    : m_byte_buffer(std::move(other.m_byte_buffer)),
      m_is_in_memory(other.m_is_in_memory),
      m_hash_code(other.m_hash_code)
{
    other.m_is_in_memory = false;
    other.m_hash_code = { 0 };
}

MemoryStreamedData &MemoryStreamedData::operator=(MemoryStreamedData &&other) noexcept
{
    m_byte_buffer = std::move(other.m_byte_buffer);
    m_is_in_memory = other.m_is_in_memory;
    m_hash_code = other.m_hash_code;

    other.m_is_in_memory = false;
    other.m_hash_code = { 0 };

    return *this;
}

bool MemoryStreamedData::IsNull() const
{
    return false;
}

bool MemoryStreamedData::IsInMemory() const
{
    return m_is_in_memory;
}

void MemoryStreamedData::Unpage()
{
    if (!m_is_in_memory) {
        return;
    }

    m_hash_code = m_byte_buffer.GetHashCode();

    auto &data_store = GetDataStore<StaticString("streaming")>();
    data_store.Write(String::ToString(m_hash_code.Value()), m_byte_buffer);

    m_byte_buffer = ByteBuffer(0, nullptr);

    m_is_in_memory = false;
}

ByteBuffer MemoryStreamedData::Load() const
{
    if (!m_is_in_memory) {
        const auto &data_store = GetDataStore<StaticString("streaming")>();

        m_is_in_memory = data_store.Read(String::ToString(m_hash_code.Value()), m_byte_buffer);
    }

    return m_byte_buffer;
}


FileStreamedData::FileStreamedData(const FilePath &filepath)
    : m_filepath(filepath)
{
}


FileStreamedData::FileStreamedData(FileStreamedData &&other) noexcept
    : m_filepath(std::move(other.m_filepath)),
      m_byte_buffer(std::move(other.m_byte_buffer))
{
}

FileStreamedData &FileStreamedData::operator=(FileStreamedData &&other) noexcept
{
    m_filepath = std::move(other.m_filepath);
    m_byte_buffer = std::move(other.m_byte_buffer);

    return *this;
}

bool FileStreamedData::IsNull() const
{
    return false;
}

bool FileStreamedData::IsInMemory() const
{
    return m_byte_buffer.HasValue();
}

void FileStreamedData::Unpage()
{
    m_byte_buffer = ByteBuffer(0, nullptr);
}

ByteBuffer FileStreamedData::Load() const
{
    if (!m_byte_buffer.HasValue()) {
        BufferedByteReader reader(m_filepath);
        m_byte_buffer.Set(reader.ReadBytes());
    }
    
    return m_byte_buffer.Get();
}

} // namespace hyperion::v2