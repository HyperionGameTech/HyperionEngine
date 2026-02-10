/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/containers/Array.hpp>

#include <core/utilities/ValueStorage.hpp>

#include <core/threading/SharedMutex.hpp>

#include <core/reflection/ObjectBase.hpp>

#include <core/io/MemoryMappedFile.hpp>

#include <asset/BlobStorageStructs.hpp>

namespace Hyperion {

class ByteReader;
class ByteWriter;

struct BlobStorageCallbacks
{
    void* context = nullptr;

    bool (*OpenReadStream)(void* context, ChunkId chunkId, ByteReader*& outStream) = nullptr;
    void (*CloseReadStream)(void* context, ByteReader* stream) = nullptr;

    bool (*OpenWriteStream)(void* context, ChunkId chunkId, ByteWriter*& outStream) = nullptr;
    void (*CloseWriteStream)(void* context, ByteWriter* stream) = nullptr;

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
    
    /*! \brief Returns number of bytes read */
    SizeType Read(ChunkId chunkId, void* dstPtr, SizeType offset, SizeType count);
    SizeType Read(const BlobDesc& desc, void* dstPtr);

    void Put(ChunkId chunkId, void* srcPtr, SizeType count, SizeType& outOffset);

    void CopyTo(BlobStorage& other);

    void Close();

    void FlushWrites();
    
    // Needs to be set by impl
    BlobStorageCallbacks callbacks;

private:
    SharedMutex m_mutex;

    Array<uint32> m_chunkIndices;
    Bitset m_validChunks;

    // indexed by indices
    Array<ByteReader*> m_readStreams;
    Array<ByteWriter*> m_writeStreams;
    Bitset m_validStreams;
    
    bool m_readOnly;
};

} // namespace Hyperion
