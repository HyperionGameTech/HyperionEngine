/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <asset/BlobStorage.hpp>
#include <asset/BlobResource.hpp>

#include <io/ByteReader.hpp>
#include <io/ByteWriter.hpp>

#include <BlobStorage.generated.inl>

namespace Hyperion {

BlobStorage::BlobStorage(const String& name, bool readOnly)
    : m_name(name),
      m_readOnly(readOnly),
      m_file(nullptr)
{
}

BlobStorage::BlobStorage(BlobStorage&& other) noexcept
    : callbacks(std::move(other.callbacks)),
      m_name(std::move(other.m_name)),
      m_file(other.m_file),
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

bool BlobStorage::InitMappedFile(MemoryMappedFile*& outMappedFile)
{
    if (m_file != nullptr)
    {
        outMappedFile = m_file;
        return true;
    }

    if ((m_file = callbacks.Open(callbacks.context, m_name.Data(), m_readOnly)))
    {
        outMappedFile = m_file;
        return true;
    }

    return false;
}

HYP_NODISCARD BlobResource* BlobStorage::MapResource(SizeType offset, SizeType size)
{
    Mutex::Guard guard(m_mutex);

    BlobResourceKey key {};
    key.offset = offset;
    key.size = size;

    auto blobResourcesIt = m_resources.Find(key);
    if (blobResourcesIt != m_resources.End())
    {
        return blobResourcesIt->second;
    }

    MemoryMappedFile* file = nullptr;

    if (!InitMappedFile(file))
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

void BlobStorage::Write(const void* src, SizeType size, SizeType alignment)
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

    if (!InitMappedFile(file))
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

    if (!m_file)
    {
        if (!(m_file = callbacks.Open(callbacks.context, m_name.Data(), /* readOnly */ true)))
        {
            HYP_LOG(Assets, Error, "Failed to open file for blob {}", m_name);

            return;
        }
    }

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
    Mutex::Guard guard(m_mutex);

    for (auto& pair : m_resources)
    {
        delete pair.second;
    }

    m_resources.Clear();

    MemoryMappedFile*& file = m_file;
    if (file != nullptr)
    {
        callbacks.Close(callbacks.context, file);
        file = nullptr;
    }
}

} // namespace Hyperion
