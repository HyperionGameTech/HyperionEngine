/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <asset/BlobStorage.hpp>

#include <io/BufferedByteReader.hpp>
#include <io/ByteWriter.hpp>

namespace Hyperion {

BlobStorage::BlobStorage()
{
}

BlobStorage::BlobStorage(BlobStorage&& other) noexcept
    : m_blobIndices(std::move(other.m_blobIndices)),
      m_validBlobs(std::move(other.m_validBlobs)),
      m_readStreams(std::move(other.m_readStreams)),
      m_writeStreams(std::move(other.m_writeStreams)),
      m_validStreams(std::move(other.m_validStreams)),
      callbacks(std::move(other.callbacks))
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

    m_blobIndices = std::move(other.m_blobIndices);
    m_validBlobs = std::move(other.m_validBlobs);
    m_readStreams = std::move(other.m_readStreams);
    m_writeStreams = std::move(other.m_writeStreams);
    m_validStreams = std::move(other.m_validStreams);
    callbacks = std::move(other.callbacks);

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

SizeType BlobStorage::Read(BlobId blobId, void* dstPtr, SizeType offset, SizeType count)
{
    TSharedLock lock(m_mutex); // @FIXME not thread safe to just use shared since we call Seek().

    if (!m_validBlobs.Test(uint32(blobId)))
    {
        lock.Reset();

        TUniqueLock lock2(m_mutex);

        if (!m_validBlobs.Test(uint32(blobId)))
        {
            m_blobIndices.Resize(uint32(blobId) + 1);
            const uint32 blobIndex = uint32(m_blobIndices.Size()) - 1;

            m_blobIndices[uint32(blobId)] = blobIndex;

            m_readStreams.Resize(uint32(blobIndex) + 1);
            m_writeStreams.Resize(uint32(blobIndex) + 1);

            m_validBlobs.Set(uint32(blobId), true);
        }

        lock2.Reset();

        lock.Reset(m_mutex);
    }

    auto*& readStream = m_readStreams[m_blobIndices[uint32(blobId)]];

    if (!m_validStreams.Test(uint32(blobId) * 2))
    {
        lock.Reset();

        TUniqueLock lock2(m_mutex);

        if (!m_validStreams.Test(uint32(blobId) * 2))
        {
            if (callbacks.OpenReadStream(callbacks.context, blobId, readStream))
            {
                Assert(readStream != nullptr);

                m_validStreams.Set(uint32(blobId) * 2, true);
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

void BlobStorage::Put(BlobId blobId, void* srcPtr, SizeType count, SizeType& outOffset)
{
    TUniqueLock lock(m_mutex);

    if (!m_validBlobs.Test(uint32(blobId)))
    {
        m_blobIndices.Resize(uint32(blobId) + 1);
        const uint32 blobIndex = uint32(m_blobIndices.Size()) - 1;

        m_blobIndices[uint32(blobId)] = blobIndex;

        m_readStreams.Resize(uint32(blobIndex) + 1);
        m_writeStreams.Resize(uint32(blobIndex) + 1);

        m_validBlobs.Set(uint32(blobId), true);
    }

    auto*& writeStream = m_writeStreams[m_blobIndices[uint32(blobId)]];
    
    if (!m_validStreams.Test(uint32(blobId) * 2 + 1))
    {
        if (callbacks.OpenWriteStream(callbacks.context, blobId, writeStream))
        {
            Assert(writeStream != nullptr);

            m_validStreams.Set(uint32(blobId) * 2 + 1, true);
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

    TUniqueLock lock(m_mutex);
    TUniqueLock otherLock(other.m_mutex);

    for (Bitset::BitIndex bitIndex : m_validBlobs)
    {
        const BlobId blobId = BlobId(bitIndex);

        Assert(m_blobIndices.Size() > bitIndex);

        const uint32 thisBlobIndex = m_blobIndices[bitIndex];

        {
            auto* writeStream = m_writeStreams[thisBlobIndex];
            if (writeStream && m_validStreams.Test(bitIndex * 2 + 1))
            {
                writeStream->Flush(); // flush pending writes before opening read
            }
        }

        auto*& readStream = m_readStreams[thisBlobIndex];

        if (!m_validStreams.Test(bitIndex * 2))
        {
            if (callbacks.OpenReadStream(callbacks.context, blobId, readStream))
            {
                Assert(readStream != nullptr);

                m_validStreams.Set(bitIndex * 2, true);
            }
            else
            {
                HYP_LOG(Assets, Error, "Failed to open read stream for blob id {}", blobId);
            }
        }

        if (readStream != nullptr)
        {
            readStream->Seek(0);

            // init other write stream
            if (!other.m_validBlobs.Test(uint32(blobId)))
            {
                other.m_blobIndices.Resize(uint32(blobId) + 1);
                const uint32 otherBlobIndex = uint32(other.m_blobIndices.Size()) - 1;

                other.m_blobIndices[uint32(blobId)] = otherBlobIndex;

                other.m_readStreams.Resize(uint32(otherBlobIndex) + 1);
                other.m_writeStreams.Resize(uint32(otherBlobIndex) + 1);

                other.m_validBlobs.Set(uint32(blobId), true);
            }
            
            auto*& writeStream = other.m_writeStreams[other.m_blobIndices[uint32(blobId)]];
    
            if (!other.m_validStreams.Test(uint32(blobId) * 2 + 1))
            {
                if (other.callbacks.OpenWriteStream(other.callbacks.context, blobId, writeStream))
                {
                    Assert(writeStream != nullptr);

                    other.m_validStreams.Set(uint32(blobId) * 2 + 1, true);
                }
                else
                {
                    HYP_LOG(Assets, Error, "Failed to open write stream for blob id {}", blobId);
                }
            }

            // write all from the src stream
            writeStream->Seek(0, /* truncate */ true);
            writeStream->Write(readStream->ReadBytes());

            writeStream->Flush();

            HYP_LOG(Assets, Debug, "Copied {} bytes of blob storage (blob index: {})", readStream->Position(), blobId);

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
