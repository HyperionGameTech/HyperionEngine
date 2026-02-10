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
      m_files(std::move(other.m_files)),
      m_validFiles(std::move(other.m_validFiles)),
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
    m_files = std::move(other.m_files);
    m_validFiles = std::move(other.m_validFiles);
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

bool BlobStorage::InitMappedFile(ChunkId chunkId, MemoryMappedFile*& outMappedFile)
{
    if (!m_validChunks.Test(uint32(chunkId)))
    {
        if (!m_validChunks.Test(uint32(chunkId)))
        {
            m_chunkIndices.Resize(uint32(chunkId) + 1);
            const uint32 chunkIndex = uint32(m_chunkIndices.Size()) - 1;

            m_chunkIndices[uint32(chunkId)] = chunkIndex;

            m_files.Resize(chunkIndex + 1);
            m_validChunks.Set(uint32(chunkId), true);
        }
    }

    MemoryMappedFile*& file = m_files[m_chunkIndices[uint32(chunkId)]];

    if (!m_validFiles.Test(uint32(chunkId)))
    {
        if (!m_validFiles.Test(uint32(chunkId)))
        {
            if ((file = callbacks.Open(callbacks.context, chunkId, m_readOnly)))
            {
                m_validFiles.Set(uint32(chunkId), true);
            }
            else
            {
                return false;
            }
        }
    }

    outMappedFile = file;

    return true;
}

HYP_NODISCARD BlobResource* BlobStorage::MapResource(ChunkId chunkId, SizeType offset, SizeType size)
{
    Mutex::Guard guard(m_mutex);

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

    if (!InitMappedFile(chunkId, file))
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
    Mutex::Guard guard(m_mutex);

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

void BlobStorage::Write(ChunkId chunkId, const void* src, SizeType size, SizeType alignment)
{
    Assert(!m_readOnly, "Cannot use Write() on read-only BlobStorage");

    if (size == 0)
    {
        return;
    }
    
    Assert(src != nullptr);

    if (alignment == 0)
    {
        // use default alignment (16)
        alignment = 16;
    }

    AssertDebug(alignment <= 16 && MathUtil::IsPowerOfTwo(alignment), "Invalid alignment specified for pointer");
    
    Mutex::Guard guard(m_mutex);

    // @TODO Optimize this to acquire chunks to write to and grab whatever we can find
    // (basically we'll implement TArena over mapped files, I am thinking)

    // write to tail of the file
    
    MemoryMappedFile* file = nullptr;

    if (!InitMappedFile(chunkId, file))
    {
        HYP_FAIL("Failed to initialize mapped file");
    }

    const SizeType fileOffset = ByteUtil::AlignAs(file->FileSize(), alignment);

    AssertDebug(fileOffset % alignment == 0);

    MemoryMappedFileView view;
    if (!file->MapRange(fileOffset, size, view))
    {
        HYP_FAIL("Failed to acquire mapped view of file for writing");
    }

    void* dst = view.Data();
    AssertDebug(dst != nullptr);

    Memory::Copy(dst, src, size);

    view.Close();
}

void BlobStorage::CopyTo(BlobStorage& other)
{
    Assert(this != &other);

    if (this == &other)
        return;

    Assert(!other.m_readOnly, "Cannot copy data to read-only BlobStorage!");
    
    Mutex::Guard guard(m_mutex);
    Mutex::Guard guard2(other.m_mutex);

    for (Bitset::BitIndex bitIndex : m_validChunks)
    {
        const ChunkId chunkId = ChunkId(bitIndex);

        Assert(m_chunkIndices.Size() > bitIndex);

        const uint32 srcChunkIndex = m_chunkIndices[bitIndex];

        MemoryMappedFile*& src = m_files[srcChunkIndex];

        if (!m_validFiles.Test(bitIndex))
        {
            if ((src = callbacks.Open(callbacks.context, chunkId, /* readOnly */ true)))
            {
                m_validFiles.Set(bitIndex, true);
            }
            else
            {
                HYP_LOG(Assets, Error, "Failed to open file for chunk id {}", chunkId);

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

            other.m_files.Resize(dstChunkIndex + 1);

            other.m_validChunks.Set(uint32(chunkId), true);
        }
            
        MemoryMappedFile*& dst = other.m_files[other.m_chunkIndices[uint32(chunkId)]];
    
        if (!other.m_validFiles.Test(uint32(chunkId)))
        {
            if ((dst = other.callbacks.Open(other.callbacks.context, chunkId, /* readOnly */ false)))
            {
                other.m_validFiles.Set(uint32(chunkId), true);
            }
            else
            {
                HYP_LOG(Assets, Error, "Failed to open file for chunk with id {}", chunkId);

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
    Mutex::Guard guard(m_mutex);

    for (auto& pair : m_resources)
    {
        delete pair.second;
    }

    m_resources.Clear();

    for (Bitset::BitIndex bitIndex : m_validFiles)
    {
        MemoryMappedFile*& file = m_files[bitIndex];
        if (file != nullptr)
        {
            callbacks.Close(callbacks.context, file);
            file = nullptr;
        }
    }

    m_validFiles.Clear();
}

} // namespace Hyperion
