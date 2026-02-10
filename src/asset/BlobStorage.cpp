/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <asset/BlobStorage.hpp>
#include <asset/BlobResource.hpp>

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
      m_streams(std::move(other.m_streams)),
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
    m_streams = std::move(other.m_streams);
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

bool BlobStorage::InitMappedFile(ChunkId chunkId, TSharedLock<SharedMutex>& sharedLock, MemoryMappedFile*& outMappedFile)
{
    if (!m_validChunks.Test(uint32(chunkId)))
    {
        sharedLock.Reset();

        TUniqueLock lock2(m_mutex);

        if (!m_validChunks.Test(uint32(chunkId)))
        {
            m_chunkIndices.Resize(uint32(chunkId) + 1);
            const uint32 chunkIndex = uint32(m_chunkIndices.Size()) - 1;

            m_chunkIndices[uint32(chunkId)] = chunkIndex;

            m_streams.Resize(uint32(chunkIndex) + 1);

            m_validChunks.Set(uint32(chunkId), true);
        }

        lock2.Reset();

        sharedLock.Reset(m_mutex);
    }

    MemoryMappedFile*& stream = m_streams[m_chunkIndices[uint32(chunkId)]];

    if (!m_validStreams.Test(uint32(chunkId) * 2))
    {
        sharedLock.Reset();

        TUniqueLock lock2(m_mutex);

        if (!m_validStreams.Test(uint32(chunkId) * 2))
        {
            if ((stream = callbacks.Open(callbacks.context, chunkId, m_readOnly)))
            {
                m_validStreams.Set(uint32(chunkId) * 2, true);
            }
            else
            {
                return false;
            }
        }

        lock2.Reset();

        sharedLock.Reset(m_mutex);
    }

    outMappedFile = stream;

    return true;
}

HYP_NODISCARD BlobResource* BlobStorage::MapResource(ChunkId chunkId, SizeType offset, SizeType size)
{
    TSharedLock lock(m_mutex); // @FIXME not thread safe to just use shared since we call Seek().

    BlobResourceKey key {};
    key.chunkId = chunkId;
    key.offset = offset;
    key.size = size;

    auto blobResourcesIt = m_resources.Find(key);
    if (blobResourcesIt != m_resources.End())
    {
        return blobResourcesIt->second;
    }

    MemoryMappedFile* file = nullptr;

    if (!InitMappedFile(chunkId, lock, file))
    {
        return 0;
    }

    auto insertResult = m_resources.Emplace(key, new BlobResource(file, key));

    BlobResource* resource = insertResult.first->second;
    if (m_readOnly)
    {
        resource->AddReader();
    }
    else
    {
        resource->AddWriter();
    }

    return resource;
}

void BlobStorage::UnmapResource(HYP_NOTNULL BlobResource* resource)
{
    TUniqueLock lock(m_mutex);

    if (m_resources.Contains(resource->GetKey()))
    {
        if (m_readOnly)
        {
            resource->ReleaseReader();
        }
        else
        {
            resource->ReleaseWriter();
        }

        int64 numReaders;
        int64 numWriters;

        resource->GetNumUsers(numReaders, numWriters);

        if (numReaders + numWriters == 0)
        {
            m_resources.Erase(resource->GetKey());

            delete resource;
        }
    }
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

        MemoryMappedFile*& src = m_streams[srcChunkIndex];

        if (!m_validStreams.Test(bitIndex * 2))
        {
            if ((src = callbacks.Open(callbacks.context, chunkId, /* readOnly */ true)))
            {
                m_validStreams.Set(bitIndex * 2, true);
            }
            else
            {
                HYP_LOG(Assets, Error, "Failed to open read stream for chunk id {}", chunkId);

                return;
            }
        }

        MemoryMappedByteReader readStream(src, 0);

        // init other write stream
        if (!other.m_validChunks.Test(uint32(chunkId)))
        {
            other.m_chunkIndices.Resize(uint32(chunkId) + 1);
            const uint32 dstChunkIndex = uint32(other.m_chunkIndices.Size()) - 1;

            other.m_chunkIndices[uint32(chunkId)] = dstChunkIndex;

            other.m_streams.Resize(uint32(dstChunkIndex) + 1);

            other.m_validChunks.Set(uint32(chunkId), true);
        }
            
        MemoryMappedFile*& dst = other.m_streams[other.m_chunkIndices[uint32(chunkId)]];
    
        if (!other.m_validStreams.Test(uint32(chunkId) * 2 + 1))
        {
            if ((dst = other.callbacks.Open(other.callbacks.context, chunkId, /* readOnly */ false)))
            {
                other.m_validStreams.Set(uint32(chunkId) * 2 + 1, true);
            }
            else
            {
                HYP_LOG(Assets, Error, "Failed to open write stream for chunk with id {}", chunkId);

                return;
            }
        }

        MemoryMappedByteWriter writeStream(dst, 0, src->FileSize());
        writeStream.Write(readStream.Read(src->FileSize()));
        writeStream.Close();

        HYP_LOG(Assets, Debug, "Copied {} bytes of blob storage (chunk index: {})", readStream.Position(), chunkId);

        readStream.Close();
    }
}

void BlobStorage::Close()
{
    TUniqueLock lock(m_mutex);

    for (auto& pair : m_resources)
    {
        delete pair.second;
    }

    m_resources.Clear();

    for (Bitset::BitIndex bitIndex : m_validStreams)
    {
        MemoryMappedFile*& file = m_streams[bitIndex];
        if (file != nullptr)
        {
            callbacks.Close(callbacks.context, file);
            file = nullptr;
        }
    }

    m_validStreams.Clear();
}

} // namespace Hyperion
