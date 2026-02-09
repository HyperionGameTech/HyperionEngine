/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <asset/BlobStorage.hpp>

#include <io/ByteReader.hpp>
#include <io/ByteWriter.hpp>

#include <BlobStorage.generated.inl>

namespace Hyperion {

HYP_API extern Pool* g_assetPool;

static constexpr SizeType BlobStorageTransientBlockSize = 4 * 1024 * 1024;

BlobStorage::BlobStorage()
    : m_name("INVALID_BLOB_STORAGE"),
      m_cursor(0),
      m_readOnly(true),
      m_file(nullptr),
      m_writeStream(nullptr),
      m_readStream(nullptr),
      transientAllocator(BlobStorageTransientBlockSize)
{
}

BlobStorage::BlobStorage(const ANSIString& name, bool readOnly)
    : m_name(name),
      m_cursor(0),
      m_readOnly(readOnly),
      m_file(nullptr),
      m_writeStream(nullptr),
      m_readStream(nullptr),
      transientAllocator(BlobStorageTransientBlockSize)
{
}

BlobStorage::BlobStorage(BlobStorage&& other) noexcept
    : callbacks(std::move(other.callbacks)),
      m_cursor(other.m_cursor),
      m_name(std::move(other.m_name)),
      m_readOnly(other.m_readOnly),
      m_freeRanges(std::move(other.m_freeRanges)),
      m_file(other.m_file),
      m_view(std::move(other.m_view)),
      m_allocations(std::move(other.m_allocations)),
      m_writeStream(other.m_writeStream),
      m_readStream(other.m_readStream),
      transientAllocator(BlobStorageTransientBlockSize)
{
    other.callbacks = {};
    other.m_file = nullptr;
    other.m_writeStream = nullptr;
    other.m_readStream = nullptr;
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

    m_name = std::move(other.m_name);
    m_cursor = other.m_cursor;
    m_readOnly = other.m_readOnly;
    m_freeRanges = std::move(other.m_freeRanges);
    m_file = other.m_file;
    m_view = std::move(other.m_view);
    m_allocations = std::move(other.m_allocations);
    m_writeStream = other.m_writeStream;
    m_readStream = other.m_readStream;

    other.callbacks = {};
    other.m_file = nullptr;
    other.m_writeStream = nullptr;
    other.m_readStream = nullptr;

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

ByteWriter* BlobStorage::GetWriteStream()
{
    if (m_writeStream != nullptr)
    {
        return m_writeStream;
    }

    Assert(!m_readOnly, "Cannot get a write stream for read-only BlobStorage");
    
    MemoryMappedFile* file;
    Assert(InitMappedFile(file));

    m_writeStream = new MemoryMappedByteWriter(file);
    m_writeStream->Seek(m_cursor);

    return m_writeStream;
}

ByteReader* BlobStorage::GetReadStream()
{
    if (m_readStream != nullptr)
    {
        return m_readStream;
    }

    MemoryMappedFile* file;
    Assert(InitMappedFile(file));

    m_readStream = new MemoryMappedByteReader(file);
    m_readStream->Seek(0);

    return m_readStream;
}

void BlobStorage::EnsureCapacity(SizeType capacity)
{
    MemoryMappedFile* file;
    if (!InitMappedFile(file, capacity))
    {
        HYP_FAIL("Failed to set initial capacity");
    }
}

bool BlobStorage::InitMappedFile(MemoryMappedFile*& outMappedFile, SizeType minRequiredSize)
{
    Assert(m_name != "INVALID_BLOB_STORAGE");

    if (minRequiredSize != 0 && (m_file != nullptr && minRequiredSize > m_file->FileSize()))
    {
        Assert(!m_readOnly && m_allocations.Empty(),
            "Cannot request larger required size in read-only mode or active mappings exist!");
    }

    if (m_file != nullptr)
    {
        if (minRequiredSize == 0 || minRequiredSize <= m_file->FileSize())
        {
            // fine, size is enough
            outMappedFile = m_file;
            return true;
        }
    }

    const SizeType previousFileSize = m_file ? m_file->FileSize() : 0;

    Close();

    if ((m_file = callbacks.Open(callbacks.context, m_name.Data(), m_readOnly)))
    {
        Assert(m_file->IsOpen());

        if (minRequiredSize > 0)
        {
            if (!m_file->EnsureCapacity(minRequiredSize))
            {
                Assert(false, "Failed to ensure capacity for file (requested size: {}, current size: {})",
                    minRequiredSize, m_file->FileSize());

                return false;
            }
        }

        outMappedFile = m_file;

        // map entire file
        if (!m_file->MapRange(0, 0, m_view))
        {
            Assert(false, "Failed to map file to view!");

            m_file->Close();
            m_file = nullptr;

            return false;
        }

        // Register newly available capacity as a free range
        const SizeType mappedSize = MathUtil::Max(minRequiredSize, m_file->FileSize());

        if (mappedSize > previousFileSize)
        {
            if (!m_freeRanges.Empty() && m_freeRanges.Back().end == previousFileSize)
            {
                m_freeRanges.Back().end = mappedSize;
            }
            else
            {
                BlobMappingRange newRange;
                newRange.start = previousFileSize;
                newRange.end = mappedSize;

                m_freeRanges.PushBack(std::move(newRange));
            }
        }

        return true;
    }

    return false;
}

void* BlobStorage::Map(SizeType offset, SizeType size)
{
    BlobResourceKey key {};
    key.offset = offset;
    key.size = size;

    auto blobResourcesIt = m_allocations.Find(key);
    if (blobResourcesIt != m_allocations.End())
    { // @TODO we need ref count so Unmap() doesnt unmap other
        return blobResourcesIt->second;
    }

    MemoryMappedFile* file = nullptr;
    if (!InitMappedFile(file, m_readOnly ? 0 : (key.offset + key.size)))
    {
        return nullptr;
    }

    void* address = reinterpret_cast<void*>(reinterpret_cast<UIntPtr>(m_view.Data()) + key.offset);
    AssertDebug(reinterpret_cast<UIntPtr>(address) - reinterpret_cast<UIntPtr>(m_view.Data()) + key.size <= m_file->FileSize());

    m_allocations.Set(key, address);

    return address;
}

void BlobStorage::Unmap(SizeType offset, SizeType size)
{
    BlobResourceKey key {};
    key.offset = offset;
    key.size = size;

    auto it = m_allocations.Find(key);
    if (it != m_allocations.End())
    {
        m_allocations.Erase(it);

        return;
    }

    HYP_FAIL("Cannot unmap allocation - not found in active allocations set");
}

bool BlobStorage::AllocateBlob(const BlobHeader& header, BlobResourceKey& outKey)
{
    Assert(!m_readOnly, "Cannot allocate from read-only BlobStorage!");
    
    const SizeType headerOffset = ByteUtil::AlignAs(m_cursor, alignof(BlobHeader));
    const SizeType totalBlobSizePlusHeader = sizeof(BlobHeader) + header.payloadOffset + header.payloadSize;

    EnsureCapacity(headerOffset + totalBlobSizePlusHeader);

    ByteWriter* writeStream = GetWriteStream();
    Assert(writeStream != nullptr);

    writeStream->Seek(headerOffset);

    // @TODO if we go with having the methods on AssetObject, we won't need to serialize header here (metadata will be in manifest)
    // or we could keep just a 4 byte header + version?
    writeStream->Write(header);

    writeStream->Seek(writeStream->Position() + header.payloadOffset);

    const SizeType offset = writeStream->Position();

    // fill with empty data:
    writeStream->Seek(offset + header.payloadSize);

    outKey = BlobResourceKey {};
    outKey.offset = offset;
    outKey.size = header.payloadSize;

    m_cursor = writeStream->Position();

    return true;
}

void BlobStorage::CopyTo(BlobStorage& other)
{
    Assert(this != &other);

    if (this == &other)
        return;

    Assert(!other.m_readOnly, "Cannot copy data to read-only BlobStorage!");
    
    if (!m_file)
    {
        if (!(m_file = callbacks.Open(callbacks.context, m_name.Data(), /* readOnly */ true)))
        {
            HYP_LOG(Assets, Error, "Failed to open file for blob {}", m_name);

            return;
        }
    }

    other.EnsureCapacity(m_file->FileSize());

    MemoryMappedByteReader readStream(m_file, 0);

    MemoryMappedFile*& dst = other.m_file;
    
    if (!dst)
    {
        if (!(dst = other.callbacks.Open(other.callbacks.context, other.m_name.Data(), /* readOnly */ false)))
        {
            HYP_LOG(Assets, Error, "Failed to open file for blob {}", other.m_name);

            return;
        }
    }

    MemoryMappedByteWriter writeStream(dst, 0, m_file->FileSize());
    writeStream.Write(readStream.Read(m_file->FileSize()));
    writeStream.Close();

    HYP_LOG(Assets, Debug, "Copied {} bytes of blob storage {} -> {}", readStream.Position(), m_name, other.m_name);

    readStream.Close();
    
    other.m_cursor = m_cursor;
    other.m_freeRanges = m_freeRanges;
}

void BlobStorage::Close()
{
    Assert(m_allocations.Empty(), "Closing BlobStorage with dangling allocations, may lead to a crash");

    transientAllocator.Reset();

    if (m_writeStream != nullptr)
    {
        m_writeStream->Close();

        delete m_writeStream;
        m_writeStream = nullptr;
    }

    if (m_readStream != nullptr)
    {
        m_readStream->Close();

        delete m_readStream;
        m_readStream = nullptr;
    }

    m_view.Close();

    MemoryMappedFile*& file = m_file;
    if (file != nullptr)
    {
        callbacks.Close(callbacks.context, file);
        file = nullptr;
    }
}

} // namespace Hyperion
