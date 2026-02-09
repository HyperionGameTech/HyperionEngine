/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/containers/Array.hpp>

#include <core/utilities/ValueStorage.hpp>

#include <core/threading/SharedMutex.hpp>

#include <core/reflection/ObjectBase.hpp>

namespace Hyperion {

enum class ChunkId : uint32;

class BufferedReader;
class ByteWriter;

struct BlobDesc
{
    ChunkId chunkId;
    SizeType offset;
    SizeType size;
};

struct BlobStorageCallbacks
{
    void* context = nullptr;

    bool (*OpenReadStream)(void* context, ChunkId chunkId, BufferedReader*& outStream) = nullptr;
    void (*CloseReadStream)(void* context, BufferedReader* stream) = nullptr;

    bool (*OpenWriteStream)(void* context, ChunkId chunkId, ByteWriter*& outStream) = nullptr;
    void (*CloseWriteStream)(void* context, ByteWriter* stream) = nullptr;

    void (*Destroy)(void* context) = nullptr;
};

HYP_CLASS()
class BlobStorage : public ObjectBase
{
    HYP_OBJECT_BODY(BlobStorage);

public:
    BlobStorage();

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
    Array<BufferedReader*> m_readStreams;
    Array<ByteWriter*> m_writeStreams;
    Bitset m_validStreams;

};

} // namespace Hyperion
