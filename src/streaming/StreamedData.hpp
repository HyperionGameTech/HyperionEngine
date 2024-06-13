/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STREAMED_DATA_HPP
#define HYPERION_STREAMED_DATA_HPP

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/RefCountedPtr.hpp>
#include <core/utilities/Optional.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/filesystem/FilePath.hpp>
#include <core/logging/LoggerFwd.hpp>
#include <core/Defines.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Streaming);

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

class HYP_API StreamedData : public EnableRefCountedPtrFromThis<StreamedData>
{
protected:
    StreamedData(StreamedDataState initial_state);

public:
    friend class StreamedDataRefBase; // allow it to manipulate m_use_count

    StreamedData()                                      = default;
    StreamedData(const StreamedData &)                  = delete;
    StreamedData &operator=(const StreamedData &)       = delete;
    StreamedData(StreamedData &&) noexcept              = delete;
    StreamedData &operator=(StreamedData &&) noexcept   = delete;
    virtual ~StreamedData() = default;

    virtual bool IsNull() const = 0;
    virtual bool IsInMemory() const = 0;

    virtual void Unpage() final;
    virtual const ByteBuffer &Load() const final;

protected:
    virtual const ByteBuffer &Load_Internal() const = 0;
    virtual void Unpage_Internal() = 0;
    virtual const ByteBuffer &GetByteBuffer() const;

    mutable AtomicVar<int>  m_use_count { 0 };
};

class HYP_API NullStreamedData : public StreamedData
{
public:
    NullStreamedData()                                          = default;
    virtual ~NullStreamedData() override                        = default;

    StreamedDataRef<NullStreamedData> AcquireRef()
        { return { RefCountedPtrFromThis().CastUnsafe<NullStreamedData>() }; }

    virtual bool IsNull() const override;
    virtual bool IsInMemory() const override;

protected:
    virtual const ByteBuffer &Load_Internal() const override;
    virtual void Unpage_Internal() override;
};

class HYP_API MemoryStreamedData : public StreamedData
{
public:
    MemoryStreamedData(const ByteBuffer &byte_buffer, StreamedDataState initial_state = StreamedDataState::LOADED);
    MemoryStreamedData(ByteBuffer &&byte_buffer, StreamedDataState initial_state = StreamedDataState::LOADED);
    MemoryStreamedData(ConstByteView byte_view, StreamedDataState initial_state = StreamedDataState::LOADED);

    MemoryStreamedData(const MemoryStreamedData &other)             = delete;
    MemoryStreamedData &operator=(const MemoryStreamedData &other)  = delete;

    MemoryStreamedData(MemoryStreamedData &&other) noexcept;
    MemoryStreamedData &operator=(MemoryStreamedData &&other) noexcept;

    virtual ~MemoryStreamedData() override                          = default;

    StreamedDataRef<MemoryStreamedData> AcquireRef()
        { return { RefCountedPtrFromThis().CastUnsafe<MemoryStreamedData>() }; }

    virtual bool IsNull() const override;
    virtual bool IsInMemory() const override;

protected:
    virtual const ByteBuffer &Load_Internal() const override;
    virtual void Unpage_Internal() override;
    virtual const ByteBuffer &GetByteBuffer() const override;
    
    HashCode                        m_hash_code;
    mutable Optional<ByteBuffer>    m_byte_buffer;
};

class HYP_API FileStreamedData : public StreamedData
{
public:
    FileStreamedData(const FilePath &filepath);
    FileStreamedData(const FileStreamedData &other)             = delete;
    FileStreamedData &operator=(const FileStreamedData &other)  = delete;
    FileStreamedData(FileStreamedData &&other) noexcept;
    FileStreamedData &operator=(FileStreamedData &&other) noexcept;
    virtual ~FileStreamedData() override                        = default;

    const FilePath &GetFilePath() const
        { return m_filepath; }

    StreamedDataRef<FileStreamedData> AcquireRef()
        { return { RefCountedPtrFromThis().CastUnsafe<FileStreamedData>() }; }

    virtual bool IsNull() const override;
    virtual bool IsInMemory() const override;

protected:
    virtual const ByteBuffer &Load_Internal() const override;
    virtual void Unpage_Internal() override;
    virtual const ByteBuffer &GetByteBuffer() const override;

    FilePath                        m_filepath;
    mutable Optional<ByteBuffer>    m_byte_buffer;
};

} // namespace hyperion

#endif