/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamedData.hpp>

#include <core/filesystem/DataStore.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/containers/StaticString.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

#pragma region StreamedData

StreamedData::StreamedData(StreamedDataState initial_state)
{
    switch (initial_state) {
    case StreamedDataState::LOADED:
        ClaimWithoutInitialize();

        break;
    default:
        // Do nothing
        break;
    }
}

void StreamedData::Initialize()
{
    (void)Load_Internal();
}

void StreamedData::Destroy()
{
    Unpage_Internal();
}

void StreamedData::Update()
{
}

bool StreamedData::IsInMemory() const
{
    bool result = false;

    const_cast<StreamedData *>(this)->Execute([this, &result]()
    {
        result = IsInMemory_Internal();
    });

    return result;
}

const ByteBuffer &StreamedData::Load() const
{
    const ByteBuffer *buffer = nullptr;

    const_cast<StreamedData *>(this)->Execute([this, &buffer]()
    {
        if (IsInMemory_Internal()) {
            buffer = &GetByteBuffer();

            return;
        }

        HYP_NAMED_SCOPE("Load streamed data");
        
        buffer = &Load_Internal();
    });

    return *buffer;
}

void StreamedData::Unpage()
{
    Execute([this]()
    {
        HYP_NAMED_SCOPE("Unpage streamed data");

        Unpage_Internal();
    });
}

const ByteBuffer &StreamedData::GetByteBuffer() const
{
    static const ByteBuffer default_value { };

    return default_value;
}

#pragma endregion StreamedData

#pragma region NullStreamedData

bool NullStreamedData::IsInMemory_Internal() const
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

MemoryStreamedData::MemoryStreamedData(HashCode hash_code, StreamedDataState initial_state, Proc<bool, HashCode, ByteBuffer &> &&load_from_memory_proc)
    : StreamedData(initial_state),
      m_hash_code(hash_code),
      m_load_from_memory_proc(std::move(load_from_memory_proc)),
      m_data_store(&GetDataStore<HYP_STATIC_STRING("streaming"), DSF_RW>()),
      m_data_store_resource_handle(*m_data_store)
{
    const bool should_load_unpaged = initial_state == StreamedDataState::UNPAGED
        && !m_data_store->Exists(String::ToString(hash_code.Value()))
        && m_load_from_memory_proc.IsValid();

    if (should_load_unpaged) {
        HYP_LOG(Streaming, Info, "StreamedData with hash code {} is not in data store, loading from memory before unpaging", hash_code.Value());
    }

    if (initial_state == StreamedDataState::LOADED || should_load_unpaged) {
        // @NOTE: Calling virtual function, but this class is marked final so it will be last in the constructor chain
        (void)StreamedData::Load();
    }
    
    if (should_load_unpaged) {
        Unpage();
    }
}

MemoryStreamedData::MemoryStreamedData(HashCode hash_code, const ByteBuffer &byte_buffer, StreamedDataState initial_state)
    : StreamedData(initial_state),
      m_hash_code(hash_code)
{
    if (initial_state == StreamedDataState::LOADED) {
        m_byte_buffer.Set(byte_buffer);
    }
}

MemoryStreamedData::MemoryStreamedData(HashCode hash_code, ByteBuffer &&byte_buffer, StreamedDataState initial_state)
    : StreamedData(initial_state),
      m_hash_code(hash_code)
{
    if (initial_state == StreamedDataState::LOADED) {
        m_byte_buffer.Set(std::move(byte_buffer));
    }
}

MemoryStreamedData::MemoryStreamedData(HashCode hash_code, ConstByteView byte_view, StreamedDataState initial_state)
    : StreamedData(initial_state),
      m_hash_code(hash_code)
{
    if (initial_state == StreamedDataState::LOADED) {
        m_byte_buffer.Set(ByteBuffer(byte_view));
    }
}

MemoryStreamedData::MemoryStreamedData(MemoryStreamedData &&other) noexcept
    : StreamedData(other.IsInMemory() ? StreamedDataState::LOADED : StreamedDataState::NONE)
{
    m_byte_buffer = std::move(other.m_byte_buffer);

    m_hash_code = other.m_hash_code;
    other.m_hash_code = HashCode();

    m_load_from_memory_proc = std::move(other.m_load_from_memory_proc);
}

MemoryStreamedData &MemoryStreamedData::operator=(MemoryStreamedData &&other) noexcept
{
    m_byte_buffer = std::move(other.m_byte_buffer);

    m_hash_code = other.m_hash_code;
    other.m_hash_code = HashCode();

    m_load_from_memory_proc = std::move(other.m_load_from_memory_proc);

    return *this;
}

bool MemoryStreamedData::IsInMemory_Internal() const
{
    return m_byte_buffer.HasValue();
}

void MemoryStreamedData::Unpage_Internal()
{
    if (!m_byte_buffer.HasValue()) {
        return;
    }

    // Enqueue task to write file to disk
    TaskSystem::GetInstance().Enqueue(
        HYP_STATIC_MESSAGE("Write streamed data to disk"),
        [byte_buffer = std::move(*m_byte_buffer), hash_code = m_hash_code, data_store_resource_handle = TResourceHandle<DataStore>(*m_data_store)]
        {
            data_store_resource_handle->Write(String::ToString(hash_code.Value()), byte_buffer);
        },
        TaskThreadPoolName::THREAD_POOL_BACKGROUND,
        TaskEnqueueFlags::FIRE_AND_FORGET
    );

    m_byte_buffer.Unset();
}

const ByteBuffer &MemoryStreamedData::Load_Internal() const
{
    if (!m_byte_buffer.HasValue()) {
        m_byte_buffer.Emplace();

        if (!m_data_store->Read(String::ToString(m_hash_code.Value()), *m_byte_buffer)) {
            if (m_load_from_memory_proc.IsValid()) {
                if (m_load_from_memory_proc(m_hash_code, *m_byte_buffer)) {
                    return *m_byte_buffer;
                }

                HYP_LOG(Streaming, Warning, "Failed to load streamed data with hash code {} from memory", m_hash_code.Value());
            }
            
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

} // namespace hyperion