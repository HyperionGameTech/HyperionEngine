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

    MemoryMappedFile* (*Open)(void* context, const char* name, bool readOnly) = nullptr;
    void (*Close)(void* context, MemoryMappedFile* file) = nullptr;

    void (*Destroy)(void* context) = nullptr;
};

HYP_CLASS()
class BlobStorage : public ObjectBase
{
    HYP_OBJECT_BODY(BlobStorage);

    struct ChunkHeader
    {
        BlobStorage* blobStorage;
        uint32 refCount;
        MemoryMappedFileView view;
    };

public:
    BlobStorage();

    explicit BlobStorage(const ANSIString& name, bool readOnly = true);

    BlobStorage(const BlobStorage& other) = delete;
    BlobStorage& operator=(const BlobStorage& other) = delete;

    BlobStorage(BlobStorage&& other) noexcept;
    BlobStorage& operator=(BlobStorage&& other) noexcept;

    ~BlobStorage();
    
    /*! \brief Create a new BlobResource mapped to the given range */
    HYP_NODISCARD BlobResource* MapResource(SizeType offset, SizeType size);
    void UnmapResource(HYP_NOTNULL BlobResource* resource);

    void Write(const void* src, SizeType size, SizeType alignment);

    void CopyTo(BlobStorage& other);

    void Close();
    
    // Needs to be set by impl
    BlobStorageCallbacks callbacks;

private:
    bool InitMappedFile(MemoryMappedFile*& outMappedFile);

    ANSIString m_name;

    mutable Mutex m_mutex;

    HashMap<BlobResourceKey, BlobResource*> m_resources;

    // indexed by indices
    MemoryMappedFile* m_file;
    
    bool m_readOnly;
};

} // namespace Hyperion
