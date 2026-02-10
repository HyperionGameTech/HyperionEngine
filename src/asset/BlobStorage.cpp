/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <asset/BlobStorage.hpp>

#include <io/ByteReader.hpp>
#include <io/ByteWriter.hpp>

#include <BlobStorage.generated.inl>

namespace Hyperion {

BlobStorage::BlobStorage(bool readOnly)
    : m_readOnly(readOnly)
{
}

BlobStorage::BlobStorage(BlobStorage&& other) noexcept
    : callbacks(std::move(other.callbacks)),
      m_chunkIndices(std::move(other.m_chunkIndices)),
      m_validChunks(std::move(other.m_validChunks)),
      m_readStreams(std::move(other.m_readStreams)),
      m_writeStreams(std::move(other.m_writeStreams)),
      m_validStreams(std::move(other.m_validStreams)),
      m_readOnly(other.m_readOnly)
{
    other.callbacks = {};
}

BlobStorage& BlobStorage::operator=(BlobStorage&& other) noexcept
{
    if (&other == this)
    {
        return *this;
    }

    Close();

    if (callbacks.Destroy)
    {
        callbacks.Destroy(callbacks.context);
    }
    
    callbacks = std::move(other.callbacks);

    m_chunkIndices = std::move(other.m_chunkIndices);
    m_validChunks = std::move(other.m_validChunks);
    m_readStreams = std::move(other.m_readStreams);
    m_writeStreams = std::move(other.m_writeStreams);
    m_validStreams = std::move(other.m_validStreams);
    m_readOnly = other.m_readOnly;

    other.callbacks = {};

    return *this;
}

BlobStorage::~BlobStorage()
{
    Close();

    if (callbacks.Destroy)
    {
        callbacks.Destroy(callbacks.context);
    }
}

SizeType BlobStorage::Read(ChunkId chunkId, void* dstPtr, SizeType offset, SizeType count)
{
    TSharedLock lock(m_mutex); // @FIXME not thread safe to just use shared since we call Seek().

    if (!m_validChunks.Test(uint32(chunkId)))
    {
        lock.Reset();

        TUniqueLock lock2(m_mutex);

        if (!m_validChunks.Test(uint32(chunkId)))
        {
            m_chunkIndices.Resize(uint32(chunkId) + 1);
            const uint32 chunkIndex = uint32(m_chunkIndices.Size()) - 1;

            m_chunkIndices[uint32(chunkId)] = chunkIndex;

            m_readStreams.Resize(uint32(chunkIndex) + 1);
            m_writeStreams.Resize(uint32(chunkIndex) + 1);

            m_validChunks.Set(uint32(chunkId), true);
        }

        lock2.Reset();

        lock.Reset(m_mutex);
    }

    auto*& readStream = m_readStreams[m_chunkIndices[uint32(chunkId)]];

    if (!m_validStreams.Test(uint32(chunkId) * 2))
    {
        lock.Reset();

        TUniqueLock lock2(m_mutex);

        if (!m_validStreams.Test(uint32(chunkId) * 2))
        {
            if (callbacks.OpenReadStream(callbacks.context, chunkId, readStream))
            {
                Assert(readStream != nullptr);

                m_validStreams.Set(uint32(chunkId) * 2, true);
            }
            else
            {
                return 0;
            }
        }

        lock2.Reset();

        lock.Reset(m_mutex);
    }

    readStream->Seek(offset);
    return readStream->Read(dstPtr, count);
}

SizeType BlobStorage::Read(const BlobDesc& desc, void* dstPtr)
{
    return Read(desc.chunkId, dstPtr, desc.offset, desc.size);
}

void BlobStorage::Put(ChunkId chunkId, void* srcPtr, SizeType count, SizeType& outOffset)
{
    Assert(!m_readOnly, "Cannot write to read-only BlobStorage!");

    if (m_readOnly)
    {
        return;
    }

    TUniqueLock lock(m_mutex);

    if (!m_validChunks.Test(uint32(chunkId)))
    {
        m_chunkIndices.Resize(uint32(chunkId) + 1);
        const uint32 chunkIndex = uint32(m_chunkIndices.Size()) - 1;

        m_chunkIndices[uint32(chunkId)] = chunkIndex;

        m_readStreams.Resize(uint32(chunkIndex) + 1);
        m_writeStreams.Resize(uint32(chunkIndex) + 1);

        m_validChunks.Set(uint32(chunkId), true);
    }

    auto*& writeStream = m_writeStreams[m_chunkIndices[uint32(chunkId)]];
    
    if (!m_validStreams.Test(uint32(chunkId) * 2 + 1))
    {
        if (callbacks.OpenWriteStream(callbacks.context, chunkId, writeStream))
        {
            Assert(writeStream != nullptr);

            m_validStreams.Set(uint32(chunkId) * 2 + 1, true);
        }
        else
        {
            return;
        }
    }

    const SizeType startOffset = writeStream->Position();
    outOffset = startOffset;

    writeStream->Write(srcPtr, count);
}

void BlobStorage::CopyTo(BlobStorage& other)
{
    Assert(this != &other);

    if (this == &other)
        return;

    Assert(!other.m_readOnly, "Cannot copy data to read-only BlobStorage!");

    TUniqueLock lock(m_mutex);
    TUniqueLock otherLock(other.m_mutex);

    for (Bitset::BitIndex bitIndex : m_validChunks)
    {
        const ChunkId chunkId = ChunkId(bitIndex);

        Assert(m_chunkIndices.Size() > bitIndex);

        const uint32 srcChunkIndex = m_chunkIndices[bitIndex];

        {
            auto* writeStream = m_writeStreams[srcChunkIndex];
            if (writeStream && m_validStreams.Test(bitIndex * 2 + 1))
            {
                writeStream->Flush(); // flush pending writes before opening read
            }
        }

        auto*& readStream = m_readStreams[srcChunkIndex];

        if (!m_validStreams.Test(bitIndex * 2))
        {
            if (callbacks.OpenReadStream(callbacks.context, chunkId, readStream))
            {
                Assert(readStream != nullptr);

                m_validStreams.Set(bitIndex * 2, true);
            }
            else
            {
                HYP_LOG(Assets, Error, "Failed to open read stream for chunk id {}", chunkId);

                return;
            }
        }

        if (readStream != nullptr)
        {
            readStream->Seek(0);

            // init other write stream
            if (!other.m_validChunks.Test(uint32(chunkId)))
            {
                other.m_chunkIndices.Resize(uint32(chunkId) + 1);
                const uint32 dstChunkIndex = uint32(other.m_chunkIndices.Size()) - 1;

                other.m_chunkIndices[uint32(chunkId)] = dstChunkIndex;

                other.m_readStreams.Resize(uint32(dstChunkIndex) + 1);
                other.m_writeStreams.Resize(uint32(dstChunkIndex) + 1);

                other.m_validChunks.Set(uint32(chunkId), true);
            }
            
            auto*& writeStream = other.m_writeStreams[other.m_chunkIndices[uint32(chunkId)]];
    
            if (!other.m_validStreams.Test(uint32(chunkId) * 2 + 1))
            {
                if (other.callbacks.OpenWriteStream(other.callbacks.context, chunkId, writeStream))
                {
                    Assert(writeStream != nullptr);

                    other.m_validStreams.Set(uint32(chunkId) * 2 + 1, true);
                }
                else
                {
                    HYP_LOG(Assets, Error, "Failed to open write stream for chunk with id {}", chunkId);

                    return;
                }
            }

            // write all from the src stream
            writeStream->Seek(0, /* truncate */ true);
            writeStream->Write(readStream->Read());

            writeStream->Flush();

            writeStream->Close();

            HYP_LOG(Assets, Debug, "Copied {} bytes of blob storage (chunk index: {})", readStream->Position(), chunkId);

            readStream->Seek(0); // roll back to start
        }
    }
}

void BlobStorage::Close()
{
    TUniqueLock lock(m_mutex);

    for (Bitset::BitIndex bitIndex : m_validStreams)
    {
        const bool isReadStream = bitIndex % 2 == 0;

        if (isReadStream)
        {
            auto*& readStream = m_readStreams[bitIndex / 2];

            callbacks.CloseReadStream(callbacks.context, readStream);

            readStream = nullptr;
        }
        else
        {
            auto*& writeStream = m_writeStreams[bitIndex / 2];

            callbacks.CloseWriteStream(callbacks.context, writeStream);

            writeStream = nullptr;
        }
    }

    m_validStreams.Clear();
}

void BlobStorage::FlushWrites()
{
    TUniqueLock lock(m_mutex);
    
    for (Bitset::BitIndex bitIndex : m_validStreams)
    {
        const bool isWriteStream = bitIndex % 2 != 0;

        if (isWriteStream)
        {
            auto*& writeStream = m_writeStreams[bitIndex / 2];
            writeStream->Flush();
        }
    }
}

} // namespace Hyperion
