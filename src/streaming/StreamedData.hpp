/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STREAMED_DATA_HPP
#define HYPERION_STREAMED_DATA_HPP

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/utilities/Optional.hpp>

#include <core/functional/Proc.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/object/HypObject.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/Defines.hpp>

namespace hyperion {

namespace filesystem {
class DataStore;
} // namespace filesystem

using filesystem::DataStore;

HYP_DECLARE_LOG_CHANNEL(Streaming);

// @TODO Abstract RenderResources into IResource and RenderResource, and change StreamedData to StreamingResource (implementing IResource)
// so it can use memory pooling and a unified interface
class StreamedData;

enum class StreamedDataState : uint32
{
    NONE = 0,
    LOADED,
    UNPAGED
};

HYP_CLASS(Abstract)
class HYP_API StreamedData : public EnableRefCountedPtrFromThis<StreamedData>, public ResourceBase
{
    HYP_OBJECT_BODY(StreamedData);

    using PreInitSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;
    using LoadingSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;

protected:
    /*! \brief Construct the StreamedData with the given initial state. If the state is LOADED, \ref{out_resource_handle} will be set to a resource handle for this. */
    StreamedData(StreamedDataState initial_state, ResourceHandle &out_resource_handle);

public:
    StreamedData()                                      = default;
    StreamedData(const StreamedData &)                  = delete;
    StreamedData &operator=(const StreamedData &)       = delete;
    StreamedData(StreamedData &&) noexcept              = delete;
    StreamedData &operator=(StreamedData &&) noexcept   = delete;
    virtual ~StreamedData()                             = default;

    bool IsInMemory() const;

    void Unpage();
    const ByteBuffer &Load() const;

protected:
    virtual void Initialize() override final;
    virtual void Destroy() override final;
    virtual void Update() override final;

    virtual bool IsInMemory_Internal() const = 0;

    virtual const ByteBuffer &Load_Internal() const = 0;
    virtual void Unpage_Internal() = 0;

    virtual const ByteBuffer &GetByteBuffer() const;
};

HYP_CLASS()
class HYP_API NullStreamedData final : public StreamedData
{
    HYP_OBJECT_BODY(NullStreamedData);

public:
    NullStreamedData()                      = default;
    virtual ~NullStreamedData() override    = default;

protected:
    virtual bool IsInMemory_Internal() const override;

    virtual const ByteBuffer &Load_Internal() const override;
    virtual void Unpage_Internal() override;
};

HYP_CLASS()
class HYP_API MemoryStreamedData final : public StreamedData
{
    HYP_OBJECT_BODY(MemoryStreamedData);

public:
    MemoryStreamedData(HashCode hash_code, Proc<bool, HashCode, ByteBuffer &> &&load_from_memory_proc = {});
    MemoryStreamedData(HashCode hash_code, const ByteBuffer &byte_buffer, StreamedDataState initial_state, ResourceHandle &out_resource_handle);
    MemoryStreamedData(HashCode hash_code, ByteBuffer &&byte_buffer, StreamedDataState initial_state, ResourceHandle &out_resource_handle);
    MemoryStreamedData(HashCode hash_code, ConstByteView byte_view, StreamedDataState initial_state, ResourceHandle &out_resource_handle);

    MemoryStreamedData(const MemoryStreamedData &other)                 = delete;
    MemoryStreamedData &operator=(const MemoryStreamedData &other)      = delete;

    MemoryStreamedData(MemoryStreamedData &&other) noexcept             = delete;
    MemoryStreamedData &operator=(MemoryStreamedData &&other) noexcept  = delete;

    virtual ~MemoryStreamedData() override                              = default;

protected:
    virtual bool IsInMemory_Internal() const override;

    virtual const ByteBuffer &Load_Internal() const override;
    virtual void Unpage_Internal() override;

    virtual const ByteBuffer &GetByteBuffer() const override;
    
    HashCode                            m_hash_code;
    mutable Optional<ByteBuffer>        m_byte_buffer;
    Proc<bool, HashCode, ByteBuffer &>  m_load_from_memory_proc;

    DataStore                           *m_data_store;
    ResourceHandle                      m_data_store_resource_handle;
};

} // namespace hyperion

#endif