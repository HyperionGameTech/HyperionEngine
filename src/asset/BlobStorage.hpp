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

    void EnsureCapacity(SizeType capacity);
    
    bool Map(SizeType offset, SizeType size, BlobAllocation& outAllocation);
    void Unmap(const BlobAllocation& allocation);

    void Write(SizeType offset, SizeType size, const void* src);

    void CopyTo(BlobStorage& other);

    void Close();
    
    // Needs to be set by impl
    BlobStorageCallbacks callbacks;

private:
    bool InitMappedFile(MemoryMappedFile*& outMappedFile, SizeType minRequiredSize = 0);

    void RemapAddresses(UIntPtr newBase, UIntPtr oldBase);

    ANSIString m_name;

    HashMap<BlobResourceKey, void*> m_allocations;

    MemoryMappedFile* m_file;
    MemoryMappedFileView m_view;
    
    bool m_readOnly;
};

} // namespace Hyperion
