/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamedData.hpp>
#include <streaming/DataStore.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/containers/StaticString.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <util/profiling/ProfileScope.hpp>

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
    (void)m_owner->Load();
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

bool StreamedData::IsInMemory() const
{
    m_loading_semaphore.Acquire();
    m_pre_init_semaphore.Produce(1);

    bool result = IsInMemory_Internal();

    m_pre_init_semaphore.Release(1);

    return result;
}

bool StreamedData::IsNull() const
{
    m_loading_semaphore.Acquire();
    m_pre_init_semaphore.Produce(1);

    bool result = IsNull_Internal();

    m_pre_init_semaphore.Release(1);

    return result;
}

const ByteBuffer &StreamedData::Load() const
{
    HYP_NAMED_SCOPE("Load streamed data");

    m_pre_init_semaphore.Acquire();
    m_loading_semaphore.Acquire();

    const ByteBuffer *buffer_ptr = &GetByteBuffer();

    m_loading_semaphore.Produce(1, [this, &buffer_ptr]()
    {
        buffer_ptr = &Load_Internal();
    });

    m_loading_semaphore.Release(1);

    AssertDebug(buffer_ptr != nullptr);

    return *buffer_ptr;
}

void StreamedData::Unpage()
{
    if (!IsInMemory()) {
        return;
    }

    HYP_NAMED_SCOPE("Unpage streamed data");

    m_pre_init_semaphore.Acquire();
    m_loading_semaphore.Acquire();
    
    m_loading_semaphore.Produce(1, [this]()
    {
        Unpage_Internal();
    });

    m_loading_semaphore.Release(1);
}

const ByteBuffer &StreamedData::GetByteBuffer() const
{
    static const ByteBuffer default_value { };

    return default_value;
}

#pragma endregion StreamedData

#pragma region NullStreamedData

bool NullStreamedData::IsNull_Internal() const
{
    return true;
}

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
      m_load_from_memory_proc(std::move(load_from_memory_proc))
{
    auto &data_store = GetDataStore<StaticString("streaming"), DSF_RW>();

    const bool should_load_unpaged = initial_state == StreamedDataState::UNPAGED
        && !data_store.Exists(String::ToString(hash_code.Value()))
        && m_load_from_memory_proc.IsValid();

    if (should_load_unpaged) {
        HYP_LOG(Streaming, LogLevel::INFO, "StreamedData with hash code {} is not in data store, loading from memory before unpaging", hash_code.Value());
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

bool MemoryStreamedData::IsNull_Internal() const
{
    return false;
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
    TaskSystem::GetInstance().Enqueue([byte_buffer = std::move(*m_byte_buffer), hash_code = m_hash_code]
    {
        auto &data_store = GetDataStore<StaticString("streaming"), DSF_RW>();
        data_store.Write(String::ToString(hash_code.Value()), byte_buffer);
    }, TaskEnqueueFlags::FIRE_AND_FORGET);

    m_byte_buffer.Unset();
}

const ByteBuffer &MemoryStreamedData::Load_Internal() const
{
    if (!m_byte_buffer.HasValue()) {
        m_byte_buffer.Emplace();

        const auto &data_store = GetDataStore<StaticString("streaming"), DSF_RW>();

        if (!data_store.Read(String::ToString(m_hash_code.Value()), *m_byte_buffer)) {
            if (m_load_from_memory_proc.IsValid()) {
                if (m_load_from_memory_proc(m_hash_code, *m_byte_buffer)) {
                    return *m_byte_buffer;
                }

                HYP_LOG(Streaming, LogLevel::WARNING, "Failed to load streamed data with hash code {} from memory", m_hash_code.Value());
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