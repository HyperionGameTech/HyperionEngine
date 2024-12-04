/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STREAMED_DATA_HPP
#define HYPERION_STREAMED_DATA_HPP

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/utilities/Optional.hpp>

#include <core/functional/Proc.hpp>

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/object/HypObject.hpp>

#include <core/logging/LoggerFwd.hpp>

#include <core/Defines.hpp>

namespace hyperion {

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

/*! \brief Interface for a reference to a streamed data object.

    Used sort of like an RC pointer, but with some additional functionality.
    Keeps track of the use count of the underlying streamed data object and
    ensures that the data is loaded into memory when the reference is created.

    When the reference is destroyed, the use count of the underlying streamed
    data object is decremented. When the use count reaches zero, the data is
    unpaged from memory.
 */
class HYP_API StreamedDataRefBase
{
protected:
    RC<StreamedData>    m_owner { nullptr };

public:
    StreamedDataRefBase(RC<StreamedData> &&owner);
    StreamedDataRefBase(const StreamedDataRefBase &other);
    StreamedDataRefBase &operator=(const StreamedDataRefBase &other);
    StreamedDataRefBase(StreamedDataRefBase &&other) noexcept;
    StreamedDataRefBase &operator=(StreamedDataRefBase &&other) noexcept;
    virtual ~StreamedDataRefBase();

protected:
    void LoadStreamedData();
    void UnpageStreamedData();
};

template <class T>
class StreamedDataRef : public StreamedDataRefBase
{
public:
    StreamedDataRef()
        : StreamedDataRefBase(nullptr)
    {
    }

    StreamedDataRef(RC<T> &&owner)
        : StreamedDataRefBase(std::move(owner))
    {
    }

    StreamedDataRef(const StreamedDataRef &other)
        : StreamedDataRefBase(static_cast<const StreamedDataRefBase &>(other))
    {
    }

    StreamedDataRef &operator=(const StreamedDataRef &other)
    {
        StreamedDataRefBase::operator=(static_cast<const StreamedDataRefBase &>(other));
        return *this;
    }

    StreamedDataRef(StreamedDataRef &&other) noexcept
        : StreamedDataRefBase(static_cast<StreamedDataRefBase &&>(other))
    {
    }

    StreamedDataRef &operator=(StreamedDataRef &&other) noexcept
    {
        StreamedDataRefBase::operator=(static_cast<StreamedDataRefBase &&>(other));
        return *this;
    }

    virtual ~StreamedDataRef() override = default;

    T *operator->()
        { return static_cast<T *>(m_owner.Get()); }

    const T *operator->() const
        { return static_cast<const T *>(m_owner.Get()); }
};

HYP_CLASS(Abstract)
class HYP_API StreamedData : public EnableRefCountedPtrFromThis<StreamedData>
{
    HYP_OBJECT_BODY(StreamedData);

    using PreInitSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;
    using LoadingSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;

protected:
    StreamedData(StreamedDataState initial_state);

public:
    friend class StreamedDataRefBase; // allow it to manipulate m_use_count

    StreamedData()                                      = default;
    StreamedData(const StreamedData &)                  = delete;
    StreamedData &operator=(const StreamedData &)       = delete;
    StreamedData(StreamedData &&) noexcept              = delete;
    StreamedData &operator=(StreamedData &&) noexcept   = delete;
    virtual ~StreamedData()                             = default;

    bool IsInMemory() const;
    bool IsNull() const;

    void Unpage();
    const ByteBuffer &Load() const;

protected:
    virtual bool IsInMemory_Internal() const = 0;
    virtual bool IsNull_Internal() const = 0;

    virtual const ByteBuffer &Load_Internal() const = 0;
    virtual void Unpage_Internal() = 0;

    virtual const ByteBuffer &GetByteBuffer() const;

    mutable AtomicVar<int>      m_use_count { 0 };
    mutable PreInitSemaphore    m_pre_init_semaphore;
    mutable LoadingSemaphore    m_loading_semaphore;
};

HYP_CLASS()
class HYP_API NullStreamedData final : public StreamedData
{
    HYP_OBJECT_BODY(NullStreamedData);

public:
    NullStreamedData()                                          = default;
    virtual ~NullStreamedData() override                        = default;

    StreamedDataRef<NullStreamedData> AcquireRef()
        { return { RefCountedPtrFromThis().CastUnsafe<NullStreamedData>() }; }

protected:
    virtual bool IsNull_Internal() const override;
    virtual bool IsInMemory_Internal() const override;

    virtual const ByteBuffer &Load_Internal() const override;
    virtual void Unpage_Internal() override;
};

HYP_CLASS()
class HYP_API MemoryStreamedData final : public StreamedData
{
    HYP_OBJECT_BODY(MemoryStreamedData);

public:
    MemoryStreamedData(HashCode hash_code, StreamedDataState initial_state = StreamedDataState::UNPAGED, Proc<bool, HashCode, ByteBuffer &> &&load_from_memory_proc = {});
    MemoryStreamedData(HashCode hash_code, const ByteBuffer &byte_buffer, StreamedDataState initial_state = StreamedDataState::LOADED);
    MemoryStreamedData(HashCode hash_code, ByteBuffer &&byte_buffer, StreamedDataState initial_state = StreamedDataState::LOADED);
    MemoryStreamedData(HashCode hash_code, ConstByteView byte_view, StreamedDataState initial_state = StreamedDataState::LOADED);

    MemoryStreamedData(const MemoryStreamedData &other)             = delete;
    MemoryStreamedData &operator=(const MemoryStreamedData &other)  = delete;

    MemoryStreamedData(MemoryStreamedData &&other) noexcept;
    MemoryStreamedData &operator=(MemoryStreamedData &&other) noexcept;

    virtual ~MemoryStreamedData() override                          = default;

    StreamedDataRef<MemoryStreamedData> AcquireRef()
        { return { RefCountedPtrFromThis().CastUnsafe<MemoryStreamedData>() }; }

protected:
    virtual bool IsNull_Internal() const override;
    virtual bool IsInMemory_Internal() const override;

    virtual const ByteBuffer &Load_Internal() const override;
    virtual void Unpage_Internal() override;

    virtual const ByteBuffer &GetByteBuffer() const override;
    
    HashCode                            m_hash_code;
    mutable Optional<ByteBuffer>        m_byte_buffer;
    Proc<bool, HashCode, ByteBuffer &>  m_load_from_memory_proc;
};

} // namespace hyperion

#endif