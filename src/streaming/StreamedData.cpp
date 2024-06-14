/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamedData.hpp>
#include <streaming/DataStore.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/containers/StaticString.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <asset/BufferedByteReader.hpp>

namespace hyperion {

#pragma region StreamedDataRefBase

StreamedDataRefBase::StreamedDataRefBase(RC<StreamedData> &&owner)
    : m_owner(std::move(owner))
{
    LoadStreamedData();
}

StreamedDataRefBase::StreamedDataRefBase(const StreamedDataRefBase &other)
    : m_owner(other.m_owner)
{
    LoadStreamedData();
}

StreamedDataRefBase &StreamedDataRefBase::operator=(const StreamedDataRefBase &other)
{
    if (this == &other) {
        return *this;
    }

    UnpageStreamedData();

    m_owner = other.m_owner;

    LoadStreamedData();

    return *this;
}

StreamedDataRefBase::StreamedDataRefBase(StreamedDataRefBase &&other) noexcept
    : m_owner(std::move(other.m_owner))
{
}

StreamedDataRefBase &StreamedDataRefBase::operator=(StreamedDataRefBase &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    UnpageStreamedData();

    m_owner = std::move(other.m_owner);

    return *this;
}

StreamedDataRefBase::~StreamedDataRefBase()
{
    UnpageStreamedData();
}

void StreamedDataRefBase::LoadStreamedData()
{
    if (!m_owner) {
        return;
    }

    m_owner->m_use_count.Increment(1, MemoryOrder::RELAXED);

    if (!m_owner->IsInMemory()) {
        (void)m_owner->Load();
    }
}

void StreamedDataRefBase::UnpageStreamedData()
{
    if (!m_owner) {
        return;
    }

    const int use_count = m_owner->m_use_count.Decrement(1, MemoryOrder::ACQUIRE_RELEASE);

#ifdef HYP_DEBUG_MODE
    AssertThrowMsg(use_count > 0, "Use count should never be less than 1");
#endif

    if (use_count == 1) {
        m_owner->Unpage();
    }
}

#pragma endregion StreamedDataRefBase

#pragma region StreamedData

StreamedData::StreamedData(StreamedDataState initial_state)
{
    switch (initial_state) {
    case StreamedDataState::LOADED:
        m_use_count.Set(1, MemoryOrder::RELAXED);

        break;
    default:
        // Do nothing
        break;
    }
}

const ByteBuffer &StreamedData::Load() const
{
    if (IsInMemory()) {
        return GetByteBuffer();
    }
    
    return Load_Internal();
}

void StreamedData::Unpage()
{
    if (!IsInMemory()) {
        return;
    }

    Unpage_Internal();
}

const ByteBuffer &StreamedData::GetByteBuffer() const
{
    static const ByteBuffer default_value { };

    return default_value;
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
    return GetByteBuffer();
}

#pragma endregion NullStreamedData

#pragma region MemoryStreamedData

MemoryStreamedData::MemoryStreamedData(const ByteBuffer &byte_buffer, StreamedDataState initial_state)
    : StreamedData(initial_state),
      m_hash_code { 0 }
{
    m_hash_code = byte_buffer.GetHashCode();

    // if (initial_state == StreamedDataState::LOADED) {
        m_byte_buffer.Set(byte_buffer);
    // }
}

MemoryStreamedData::MemoryStreamedData(ByteBuffer &&byte_buffer, StreamedDataState initial_state)
    : StreamedData(initial_state),
      m_hash_code { 0 }
{
    m_hash_code = byte_buffer.GetHashCode();

    // if (initial_state == StreamedDataState::LOADED) {
        m_byte_buffer.Set(std::move(byte_buffer));
    // }
}

MemoryStreamedData::MemoryStreamedData(ConstByteView byte_view, StreamedDataState initial_state)
    : StreamedData(initial_state),
      m_hash_code { 0 }
{
    m_hash_code = byte_view.GetHashCode();

    // if (initial_state == StreamedDataState::LOADED) {
        m_byte_buffer.Set(ByteBuffer(byte_view));
    // }
}

MemoryStreamedData::MemoryStreamedData(MemoryStreamedData &&other) noexcept
    : StreamedData(other.IsInMemory() ? StreamedDataState::LOADED : StreamedDataState::NONE)
{
    m_byte_buffer = std::move(other.m_byte_buffer);

    m_hash_code = other.m_hash_code;
    other.m_hash_code = { 0 };
}

MemoryStreamedData &MemoryStreamedData::operator=(MemoryStreamedData &&other) noexcept
{
    m_byte_buffer = std::move(other.m_byte_buffer);
    m_hash_code = other.m_hash_code;

    other.m_hash_code = { 0 };

    return *this;
}

bool MemoryStreamedData::IsNull() const
{
    return false;
}

bool MemoryStreamedData::IsInMemory() const
{
    return m_byte_buffer.HasValue();
}

void MemoryStreamedData::Unpage_Internal()
{
    if (!m_byte_buffer.HasValue()) {
        return;
    }

    m_hash_code = m_byte_buffer->GetHashCode();

    // Enqueue task to write file to disk
    TaskSystem::GetInstance().ScheduleTask([byte_buffer = std::move(*m_byte_buffer), hash_code = m_hash_code]
    {
        auto &data_store = GetDataStore<StaticString("streaming"), DSF_RW>();
        data_store.Write(String::ToString(hash_code.Value()), byte_buffer);
    });

    m_byte_buffer.Unset();
}

const ByteBuffer &MemoryStreamedData::Load_Internal() const
{
    if (!m_byte_buffer.HasValue()) {
        m_byte_buffer.Emplace();

        const auto &data_store = GetDataStore<StaticString("streaming"), DSF_RW>();

        if (!data_store.Read(String::ToString(m_hash_code.Value()), *m_byte_buffer)) {
            m_byte_buffer.Unset();
        }
    }

    return GetByteBuffer();
}

const ByteBuffer &MemoryStreamedData::GetByteBuffer() const
{
    if (m_byte_buffer.HasValue()) {
        return *m_byte_buffer;
    }

    return StreamedData::GetByteBuffer();
}

#pragma endregion MemoryStreamedData

#pragma region FileStreamedData

FileStreamedData::FileStreamedData(const FilePath &filepath)
    : StreamedData(StreamedDataState::NONE),
      m_filepath(filepath)
{
}


FileStreamedData::FileStreamedData(FileStreamedData &&other) noexcept
    : StreamedData(other.IsInMemory() ? StreamedDataState::LOADED : StreamedDataState::NONE)
{
    m_filepath = std::move(other.m_filepath);
    m_byte_buffer = std::move(other.m_byte_buffer);
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
    if (!m_byte_buffer.HasValue()) {
        BufferedByteReader reader(m_filepath);
        m_byte_buffer.Set(reader.ReadBytes());
    }
    
    return *m_byte_buffer;
}

const ByteBuffer &FileStreamedData::GetByteBuffer() const
{
    if (m_byte_buffer.HasValue()) {
        return *m_byte_buffer;
    }

    return StreamedData::GetByteBuffer();
}

#pragma endregion FileStreamedData

} // namespace hyperion