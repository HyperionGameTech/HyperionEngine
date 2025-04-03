/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <streaming/StreamedData.hpp>
#include <streaming/StreamingThread.hpp>

#include <core/filesystem/DataStore.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/containers/StaticString.hpp>

#include <core/object/HypClass.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region StreamedData

StreamedData::StreamedData(StreamedDataState initial_state, ResourceHandle &out_resource_handle)
{
    switch (initial_state) {
    case StreamedDataState::LOADED:
        // Setup resource handle so the data stays loaded
        // Initialize can't be called since it is a virtual function and the object is not fully constructed yet
        out_resource_handle = ResourceHandle(*this, /* should_initialize */ false);

        break;
    default:
        // Do nothing
        break;
    }
}

void StreamedData::Initialize()
{
    Load_Internal();
}

void StreamedData::Destroy()
{
    HYP_NAMED_SCOPE_FMT("Unpage {} (ResourceBase::Destroy())", InstanceClass()->GetName());

    Unpage_Internal();
}

void StreamedData::Update()
{
}

void StreamedData::Load() const
{
    const_cast<StreamedData *>(this)->Execute([this]()
    {
        if (IsInMemory_Internal()) {
            return;
        }

        HYP_NAMED_SCOPE("Load {}", InstanceClass()->GetName());

        Load_Internal();
    });
}

void StreamedData::Unpage()
{
    Execute([this]()
    {
        HYP_NAMED_SCOPE("Unpage {} (manual)", InstanceClass()->GetName());

        Unpage_Internal();
    });
}

const ByteBuffer &StreamedData::GetByteBuffer() const
{
    static const ByteBuffer default_value { };

    return default_value;
}

IThread *StreamedData::GetOwnerThread() const
{
    return GetGlobalStreamingThread().Get();
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

void NullStreamedData::Load_Internal() const
{
    // Do nothing
}

#pragma endregion NullStreamedData

#pragma region MemoryStreamedData

MemoryStreamedData::MemoryStreamedData(HashCode hash_code, Proc<bool(HashCode, ByteBuffer &)> &&load_from_memory_proc)
    : StreamedData(),
      m_hash_code(hash_code),
      m_load_from_memory_proc(std::move(load_from_memory_proc)),
      m_data_store(&GetDataStore<HYP_STATIC_STRING("streaming"), DSF_RW>()),
      m_data_store_resource_handle(*m_data_store)
{
    const bool should_load_unpaged = !m_data_store->Exists(String::ToString(hash_code.Value())) && m_load_from_memory_proc.IsValid();

    if (should_load_unpaged) {
        MemoryStreamedData::Load_Internal();
        MemoryStreamedData::Unpage_Internal();
    }
}

MemoryStreamedData::MemoryStreamedData(HashCode hash_code, const ByteBuffer &byte_buffer, StreamedDataState initial_state, ResourceHandle &out_resource_handle)
    : StreamedData(initial_state, out_resource_handle),
      m_hash_code(hash_code)
{
    if (initial_state == StreamedDataState::LOADED) {
        m_byte_buffer.Set(byte_buffer);
    }
}

MemoryStreamedData::MemoryStreamedData(HashCode hash_code, ByteBuffer &&byte_buffer, StreamedDataState initial_state, ResourceHandle &out_resource_handle)
    : StreamedData(initial_state, out_resource_handle),
      m_hash_code(hash_code)
{
    if (initial_state == StreamedDataState::LOADED) {
        m_byte_buffer.Set(std::move(byte_buffer));
    }
}

MemoryStreamedData::MemoryStreamedData(HashCode hash_code, ConstByteView byte_view, StreamedDataState initial_state, ResourceHandle &out_resource_handle)
    : StreamedData(initial_state, out_resource_handle),
      m_hash_code(hash_code)
{
    if (initial_state == StreamedDataState::LOADED) {
        m_byte_buffer.Set(ByteBuffer(byte_view));
    }
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

    ByteBuffer byte_buffer = std::move(*m_byte_buffer);
    m_byte_buffer.Unset();

    if (byte_buffer.Empty()) {
        return;
    }

    m_data_store->Write(String::ToString(m_hash_code.Value()), byte_buffer);
}

void MemoryStreamedData::Load_Internal() const
{
    if (!m_byte_buffer.HasValue()) {
        m_byte_buffer.Emplace();

        if (!m_data_store->Read(String::ToString(m_hash_code.Value()), *m_byte_buffer)) {
            if (m_load_from_memory_proc.IsValid()) {
                if (m_load_from_memory_proc(m_hash_code, *m_byte_buffer)) {
                    return;
                }

                HYP_LOG(Streaming, Warning, "Failed to load streamed data with hash code {} from memory", m_hash_code.Value());
            }
            
            m_byte_buffer.Unset();
        }
    }
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