/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamedData.hpp>
#include <streaming/DataStore.hpp>

#include <core/containers/StaticString.hpp>

#include <asset/BufferedByteReader.hpp>

namespace hyperion {

#pragma region StreamedDataRefBase

StreamedDataRefBase::StreamedDataRefBase(RC<StreamedData> &&owner)
    : m_owner(std::move(owner))
{
    if (m_owner != nullptr) {
        m_owner->m_use_count.Increment(1u, MemoryOrder::RELAXED);

        if (!m_owner->IsInMemory()) {
            (void)m_owner->Load();
        }
    }
}

StreamedDataRefBase::StreamedDataRefBase(const StreamedDataRefBase &other)
    : m_owner(other.m_owner)
{
    if (m_owner != nullptr) {
        m_owner->m_use_count.Increment(1u, MemoryOrder::RELAXED);

        if (!m_owner->IsInMemory()) {
            (void)m_owner->Load();
        }
    }
}

StreamedDataRefBase &StreamedDataRefBase::operator=(const StreamedDataRefBase &other)
{
    if (this == &other) {
        return *this;
    }

    if (m_owner != nullptr) {
        m_owner->Unpage();
    }

    m_owner = other.m_owner;

    if (m_owner != nullptr) {
        (void)m_owner->Load();
    }

    return *this;
}

StreamedDataRefBase::StreamedDataRefBase(StreamedDataRefBase &&other) noexcept
    : m_owner(other.m_owner)
{
    other.m_owner = nullptr;
}

StreamedDataRefBase &StreamedDataRefBase::operator=(StreamedDataRefBase &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    if (m_owner != nullptr) {
        m_owner->Unpage();
    }

    m_owner = other.m_owner;
    other.m_owner = nullptr;

    return *this;
}

StreamedDataRefBase::~StreamedDataRefBase()
{
    if (m_owner != nullptr) {
        m_owner->Unpage();
    }
}

#pragma endregion StreamedDataRefBase

#pragma region StreamedData

const ByteBuffer &StreamedData::Load() const
{
    m_use_count.Increment(1, MemoryOrder::RELAXED);

    return Load_Internal();
}

void StreamedData::Unpage()
{
    const int use_count = StreamedData::m_use_count.Get(MemoryOrder::SEQUENTIAL);

    if (use_count <= 0) {
        return;
    }

    StreamedData::m_use_count.Decrement(1, MemoryOrder::RELAXED);

    if (use_count == 1) {
        Unpage_Internal();
    }
}

#pragma endregion StreamedData

#pragma region NullStreamedData

bool NullStreamedData::IsNull() const
{
    return true;
}

bool NullStreamedData::IsInMemory() const
{
    return false;
}

void NullStreamedData::Unpage_Internal()
{
    // Do nothing
}

const ByteBuffer &NullStreamedData::Load_Internal() const
{
    return m_byte_buffer;
}

#pragma endregion NullStreamedData

#pragma region MemoryStreamedData

MemoryStreamedData::MemoryStreamedData(const ByteBuffer &byte_buffer)
    : m_byte_buffer(byte_buffer),
      m_is_in_memory(true),
      m_hash_code { 0 }
{
    m_hash_code = m_byte_buffer.GetHashCode();
}

MemoryStreamedData::MemoryStreamedData(ByteBuffer &&byte_buffer)
    : m_byte_buffer(std::move(byte_buffer)),
      m_is_in_memory(true),
      m_hash_code { 0 }
{
    m_hash_code = m_byte_buffer.GetHashCode();
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

void MemoryStreamedData::Unpage_Internal()
{
    if (!m_is_in_memory) {
        return;
    }

    m_hash_code = m_byte_buffer.GetHashCode();

    auto &data_store = GetDataStore<StaticString("streaming"), DSF_RW>();
    data_store.Write(String::ToString(m_hash_code.Value()), m_byte_buffer);

    m_byte_buffer = ByteBuffer(0, nullptr);

    m_is_in_memory = false;
}

const ByteBuffer &MemoryStreamedData::Load_Internal() const
{
    StreamedData::m_use_count.Increment(1, MemoryOrder::RELAXED);

    if (!m_is_in_memory) {
        const auto &data_store = GetDataStore<StaticString("streaming"), DSF_RW>();

        m_is_in_memory = data_store.Read(String::ToString(m_hash_code.Value()), m_byte_buffer);
    }

    return m_byte_buffer;
}

#pragma endregion MemoryStreamedData

#pragma region FileStreamedData

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

void FileStreamedData::Unpage_Internal()
{
    m_byte_buffer = { };
}

const ByteBuffer &FileStreamedData::Load_Internal() const
{
    StreamedData::m_use_count.Increment(1, MemoryOrder::RELAXED);

    if (!m_byte_buffer.HasValue()) {
        BufferedByteReader reader(m_filepath);
        m_byte_buffer.Set(reader.ReadBytes());
    }
    
    return m_byte_buffer.Get();
}

#pragma endregion FileStreamedData

} // namespace hyperion