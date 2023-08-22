#ifndef HYPERION_V2_STREAMED_DATA_HPP
#define HYPERION_V2_STREAMED_DATA_HPP

#include <core/lib/ByteBuffer.hpp>
#include <core/lib/UniqueID.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

class StreamedData
{
public:
    virtual ~StreamedData() = default;

    virtual bool IsNull() const = 0;

    virtual bool IsInMemory() const = 0;

    virtual void Unpage() = 0;

    virtual ByteBuffer Load() const = 0;
};

class NullStreamingData : public StreamedData
{
public:
    virtual ~NullStreamingData() override = default;

    virtual bool IsNull() const override;

    virtual bool IsInMemory() const override;

    virtual void Unpage() override;

    virtual ByteBuffer Load() const override;
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

    virtual bool IsNull() const override;

    virtual bool IsInMemory() const override;

    virtual void Unpage() override;

    virtual ByteBuffer Load() const override;

protected:
    HashCode m_hash_code;
    mutable bool m_is_in_memory;
    mutable ByteBuffer m_byte_buffer;
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

    virtual bool IsNull() const override;

    virtual bool IsInMemory() const override;

    virtual void Unpage() override;

    virtual ByteBuffer Load() const override;

protected:
    FilePath m_filepath;
    mutable Optional<ByteBuffer> m_byte_buffer;
};

} // namespace hyperion::v2

#endif