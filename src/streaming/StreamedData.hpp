#ifndef HYPERION_V2_STREAMED_DATA_HPP
#define HYPERION_V2_STREAMED_DATA_HPP

#include <core/lib/ByteBuffer.hpp>
#include <core/lib/UniqueID.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

class StreamedData;

namespace detail {

struct StreamedDataRefCount
{
    StreamedData    *owner;
    uint            ref_count;

    StreamedDataRefCount(StreamedData *owner);
    StreamedDataRefCount(const StreamedDataRefCount &other)                 = delete;
    StreamedDataRefCount &operator=(const StreamedDataRefCount &other)      = delete;
    StreamedDataRefCount(StreamedDataRefCount &&other) noexcept             = delete;
    StreamedDataRefCount &operator=(StreamedDataRefCount &&other) noexcept  = delete;
    ~StreamedDataRefCount();

    void IncRef();
    void DecRef();
};

} // namespace detail

class StreamedDataRefBase
{
protected:
    StreamedData    *m_owner { nullptr };

public:
    StreamedDataRefBase(StreamedData *owner);
    StreamedDataRefBase(const StreamedDataRefBase &other);
    StreamedDataRefBase &operator=(const StreamedDataRefBase &other);
    StreamedDataRefBase(StreamedDataRefBase &&other) noexcept;
    StreamedDataRefBase &operator=(StreamedDataRefBase &&other) noexcept;
    ~StreamedDataRefBase();
};

template <class T>
class StreamedDataRef : public StreamedDataRefBase
{
public:
    StreamedDataRef(T *owner)
        : StreamedDataRefBase(owner)
    {
    }

    StreamedDataRef(const StreamedDataRef &other)
        : StreamedDataRefBase(static_cast<const StreamedDataRefBase &>(other))
    {
    }

    StreamedDataRef &operator=(const StreamedDataRefBase &other)
    {
        StreamedDataRefBase::operator=(static_cast<const StreamedDataRefBase &>(other));
        return *this;
    }

    StreamedDataRef(StreamedDataRef &&other) noexcept
        : StreamedDataRefBase(static_cast<StreamedDataRefBase &&>(other))
    {
        
    }

    StreamedDataRef &operator=(StreamedDataRefBase &&other) noexcept
    {
        StreamedDataRefBase::operator=(static_cast<StreamedDataRefBase &&>(other));
        return *this;
    }

    ~StreamedDataRef() = default;

    T *operator->()
        { return static_cast<T *>(m_owner); }

    const T *operator->() const
        { return static_cast<const T *>(m_owner); }
};

class StreamedData
{
public:
    friend class StreamedDataRefBase;

    virtual ~StreamedData() = default;

    virtual bool IsNull() const = 0;
    virtual bool IsInMemory() const = 0;
    virtual void Unpage() = 0;
    virtual const ByteBuffer &Load() const = 0;

private:
    uint32  m_ref_count = 0;
};

class NullStreamedData : public StreamedData
{
public:
    virtual ~NullStreamedData() override = default;

    StreamedDataRef<NullStreamedData> AcquireRef()
        { return { this }; }

    virtual bool IsNull() const override;
    virtual bool IsInMemory() const override;
    virtual void Unpage() override;
    virtual const ByteBuffer &Load() const override;

private:
    ByteBuffer  m_byte_buffer;
};

class MemoryStreamedData : public StreamedData
{
public:
    MemoryStreamedData(const ByteBuffer &byte_buffer);
    MemoryStreamedData(ByteBuffer &&byte_buffer);
    MemoryStreamedData(const MemoryStreamedData &other) = delete;
    MemoryStreamedData &operator=(const MemoryStreamedData &other) = delete;
    MemoryStreamedData(MemoryStreamedData &&other) noexcept;
    MemoryStreamedData &operator=(MemoryStreamedData &&other) noexcept;
    virtual ~MemoryStreamedData() override = default;

    StreamedDataRef<MemoryStreamedData> AcquireRef()
        { return { this }; }

    virtual bool IsNull() const override;
    virtual bool IsInMemory() const override;
    virtual void Unpage() override;
    virtual const ByteBuffer &Load() const override;

protected:
    HashCode            m_hash_code;
    mutable bool        m_is_in_memory;
    mutable ByteBuffer  m_byte_buffer;
};

class FileStreamedData : public StreamedData
{
public:
    FileStreamedData(const FilePath &filepath);
    FileStreamedData(const FileStreamedData &other) = delete;
    FileStreamedData &operator=(const FileStreamedData &other) = delete;
    FileStreamedData(FileStreamedData &&other) noexcept;
    FileStreamedData &operator=(FileStreamedData &&other) noexcept;
    virtual ~FileStreamedData() override = default;

    const FilePath &GetFilePath() const
        { return m_filepath; }

    StreamedDataRef<FileStreamedData> AcquireRef()
        { return { this }; }

    virtual bool IsNull() const override;
    virtual bool IsInMemory() const override;
    virtual void Unpage() override;
    virtual const ByteBuffer &Load() const override;

protected:
    FilePath                        m_filepath;
    mutable Optional<ByteBuffer>    m_byte_buffer;
};

} // namespace hyperion::v2

#endif