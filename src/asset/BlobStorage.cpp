/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <asset/BlobStorage.hpp>
#include <asset/BlobResource.hpp>

#include <io/ByteReader.hpp>
#include <io/ByteWriter.hpp>

#include <BlobStorage.generated.inl>

namespace Hyperion {

BlobStorage::BlobStorage()
    : m_name("INVALID_BLOB_STORAGE"),
      m_readOnly(true),
      m_file(nullptr)
{
}

BlobStorage::BlobStorage(const ANSIString& name, bool readOnly)
    : m_name(name),
      m_readOnly(readOnly),
      m_file(nullptr)
{
}

BlobStorage::BlobStorage(BlobStorage&& other) noexcept
    : callbacks(std::move(other.callbacks)),
      m_name(std::move(other.m_name)),
      m_file(other.m_file),
      m_view(std::move(other.m_view)),
      m_allocations(std::move(other.m_allocations)),
      m_readOnly(other.m_readOnly)
{
    other.m_file = nullptr;
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

    m_name = std::move(other.m_name);
    m_file = other.m_file;
    m_view = std::move(other.m_view);
    m_allocations = std::move(other.m_allocations);
    m_readOnly = other.m_readOnly;

    other.m_file = nullptr;
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

    if (m_file != nullptr && (minRequiredSize == 0 || minRequiredSize <= m_file->FileSize()))
    {
        outMappedFile = m_file;
        return true;
    }

    /*if (m_file != nullptr)
    {
        m_view.Close();

        m_file->Close();
        m_file = nullptr;
    }*/

    if ((m_file = callbacks.Open(callbacks.context, m_name.Data(), m_readOnly)))
    {
        outMappedFile = m_file;

        if (!m_file->MapRange(0, minRequiredSize, m_view))
        {
            Assert(false, "Failed to map file to view!");

            m_file->Close();
            m_file = nullptr;

            return false;
        }

        return true;
    }

    return false;
}

bool BlobStorage::Map(SizeType offset, SizeType size, BlobAllocation& outAllocation)
{
    BlobResourceKey key {};
    key.offset = offset;
    key.size = size;

    auto blobResourcesIt = m_allocations.Find(key);
    if (blobResourcesIt != m_allocations.End())
    { // @TODO we need ref count so Unmap() doesnt unmap other
        outAllocation = { key, blobResourcesIt->second };
        return true;
    }

    MemoryMappedFile* file = nullptr;
    if (!InitMappedFile(file, offset + size))
    {
        return false;
    }

    void* address = reinterpret_cast<void*>(reinterpret_cast<UIntPtr>(m_view.Data()) + offset);
    AssertDebug(reinterpret_cast<UIntPtr>(address) - reinterpret_cast<UIntPtr>(m_view.Data()) + size <= m_file->FileSize());

    m_allocations.Set(key, address);

    outAllocation = { key, address };

    return true;
}

void BlobStorage::Unmap(const BlobAllocation& allocation)
{
    auto it = m_allocations.Find(allocation.key);
    if (it != m_allocations.End())
    {
        m_allocations.Erase(it);

        return;
    }

    HYP_FAIL("Cannot unmap allocation - not found in active allocations set");
}

void BlobStorage::Write(SizeType offset, SizeType size, const void* src)
{
    Assert(!m_readOnly, "Cannot write to read-only BlobStorage!");
    
    MemoryMappedFile* file = nullptr;
    if (!InitMappedFile(file, offset + size))
    {
        HYP_FAIL("Failed to initialize mapped file for writing!");
    }

    Assert(offset + size <= m_file->FileSize(), "Write range exceeds file size!");

    Memory::Copy(
        reinterpret_cast<void*>(reinterpret_cast<UIntPtr>(m_view.Data()) + offset),
        src, size);
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
}

void BlobStorage::Close()
{
    m_allocations.Clear();

    m_view.Close();
    m_view = {};

    MemoryMappedFile*& file = m_file;
    if (file != nullptr)
    {
        callbacks.Close(callbacks.context, file);
        file = nullptr;
    }
}

} // namespace Hyperion
