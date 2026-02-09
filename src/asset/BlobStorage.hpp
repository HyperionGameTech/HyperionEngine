/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>

#include <core/utilities/ValueStorage.hpp>
#include <core/utilities/Range.hpp>

#include <core/memory/allocator/ArenaAllocator.hpp>

#include <core/threading/SharedMutex.hpp>

#include <core/reflection/ObjectBase.hpp>

#include <core/io/MemoryMappedFile.hpp>

#include <asset/BlobStorageStructs.hpp>

namespace Hyperion {

class ByteReader;
class ByteWriter;
struct BlobAllocation;

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

public:
    BlobStorage();

    explicit BlobStorage(const ANSIString& name, bool readOnly = true);

    BlobStorage(const BlobStorage& other) = delete;
    BlobStorage& operator=(const BlobStorage& other) = delete;

    BlobStorage(BlobStorage&& other) noexcept;
    BlobStorage& operator=(BlobStorage&& other) noexcept;

    ~BlobStorage();

    ByteWriter* GetWriteStream();
    ByteReader* GetReadStream();

    void EnsureCapacity(SizeType capacity);
    
    HYP_NODISCARD void* Map(SizeType offset, SizeType size);
    void Unmap(SizeType offset, SizeType size);

    bool AllocateBlob(const BlobHeader& header, BlobResourceKey& outKey);

    void CopyTo(BlobStorage& other);

    void Close();
    
    // Needs to be set by impl
    BlobStorageCallbacks callbacks;

    // For allocating memory for read/write data - before data is serialized to disk
    Arena transientAllocator;

private:
    bool InitMappedFile(MemoryMappedFile*& outMappedFile, SizeType minRequiredSize = 0);

    HYP_FIELD()
    ANSIString m_name;

    HYP_FIELD()
    uint64 m_cursor;

    HYP_FIELD()
    Array<BlobMappingRange> m_freeRanges; // <---- TODO make use of this
    
    bool m_readOnly;

    HashMap<BlobResourceKey, void*> m_allocations;

    MemoryMappedFile* m_file;
    MemoryMappedFileView m_view;

    ByteWriter* m_writeStream;
    ByteReader* m_readStream;
};

} // namespace Hyperion
