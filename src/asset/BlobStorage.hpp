/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/containers/Array.hpp>

#include <core/utilities/ValueStorage.hpp>

#include <core/threading/SharedMutex.hpp>

#include <core/reflection/ObjectBase.hpp>

namespace Hyperion {

enum class BlobId : uint32;

class BufferedReader;
class ByteWriter;

struct BlobStorageCallbacks
{
    void* context = nullptr;

    bool (*OpenReadStream)(void* context, BlobId blobId, BufferedReader*& outStream) = nullptr;
    void (*CloseReadStream)(void* context, BufferedReader* stream) = nullptr;

    bool (*OpenWriteStream)(void* context, BlobId blobId, ByteWriter*& outStream) = nullptr;
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
    SizeType Read(BlobId blobId, void* dstPtr, SizeType offset, SizeType count);

    void Put(BlobId blobId, void* srcPtr, SizeType count, SizeType& outOffset);

    void CopyTo(BlobStorage& other);

    void Close();

    void FlushWrites();
    
    // Needs to be set by impl
    BlobStorageCallbacks callbacks;

private:
    SharedMutex m_mutex;

    Array<uint32> m_blobIndices; // indexed by blob enum value
    Bitset m_validBlobs;

    // indexed by indices
    Array<BufferedReader*> m_readStreams;
    Array<ByteWriter*> m_writeStreams;
    Bitset m_validStreams;

};

} // namespace Hyperion
