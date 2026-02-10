/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>

#include <core/utilities/ValueStorage.hpp>

#include <core/threading/SharedMutex.hpp>

#include <core/reflection/ObjectBase.hpp>

#include <core/io/MemoryMappedFile.hpp>

#include <asset/BlobStorageStructs.hpp>

namespace Hyperion {

class ByteReader;
class ByteWriter;
class BlobResource;

struct ResourceGuard;

struct BlobStorageCallbacks
{
    void* context = nullptr;

    MemoryMappedFile* (*Open)(void* context, ChunkId chunkId, bool readOnly) = nullptr;
    void (*Close)(void* context, MemoryMappedFile* file) = nullptr;

    void (*Destroy)(void* context) = nullptr;
};

HYP_CLASS()
class BlobStorage : public ObjectBase
{
    HYP_OBJECT_BODY(BlobStorage);

public:
    explicit BlobStorage(bool readOnly = true);

    BlobStorage(const BlobStorage& other) = delete;
    BlobStorage& operator=(const BlobStorage& other) = delete;

    BlobStorage(BlobStorage&& other) noexcept;
    BlobStorage& operator=(BlobStorage&& other) noexcept;

    ~BlobStorage();
    
    /*! \brief Create a new BlobResource mapped to the given range */
    HYP_NODISCARD BlobResource* MapResource(ChunkId chunkId, SizeType offset, SizeType size);
    void UnmapResource(HYP_NOTNULL BlobResource* resource);

    void Write(ChunkId chunkId, const void* src, SizeType size, SizeType alignment);

    void CopyTo(BlobStorage& other);

    void Close();
    
    // Needs to be set by impl
    BlobStorageCallbacks callbacks;

private:
    bool InitMappedFile(ChunkId chunkId, MemoryMappedFile*& outMappedFile);

    mutable Mutex m_mutex;

    HashMap<BlobResourceKey, BlobResource*> m_resources;

    Array<uint32> m_chunkIndices;
    Bitset m_validChunks;

    // indexed by indices
    Array<MemoryMappedFile*> m_files;
    Bitset m_validFiles;
    
    bool m_readOnly;
};

} // namespace Hyperion
