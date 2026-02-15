/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>

#include <core/utilities/ValueStorage.hpp>
#include <core/utilities/Range.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/memory/allocator/ArenaAllocator.hpp>

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

    MemoryMappedFile* (*Open)(void* context, const char* name) = nullptr;
    void (*Close)(void* context, MemoryMappedFile* file) = nullptr;

    void (*Destroy)(void* context) = nullptr;
};

HYP_STRUCT()
struct BlobMappingRange
{
    HYP_STRUCT_BODY(BlobMappingRange)

    HYP_FIELD()
    uint32 start = 0;

    HYP_FIELD()
    uint32 end = 0;
};

struct BlobAllocationInfo
{
    void* ptr = nullptr;
    uint32 count = 0;
};

struct BlobAllocationDesc
{
    SizeType offset = 0;
    SizeType size = 0;

    HYP_FORCE_INLINE constexpr bool operator==(const BlobAllocationDesc& other) const = default;

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(offset)
            .Combine(size);
    }
};

HYP_STRUCT()
struct BlobPageData
{
    HYP_STRUCT_BODY(BlobPageData)

    HYP_FIELD()
    uint64 cursor = 0;

    MemoryMappedFile* file = nullptr;
    MemoryMappedFileView* view = nullptr;
    ByteWriter* writeStream = nullptr;
    ByteReader* readStream = nullptr;
};

HYP_CLASS()
class BlobStorage : public ObjectBase
{
    HYP_OBJECT_BODY(BlobStorage);

public:
    static constexpr uint64 DefaultPageSize = 256 * 1024 * 1024;

    BlobStorage();

    explicit BlobStorage(const FilePath& baseDirectory, uint64 pageSize);

    BlobStorage(const BlobStorage& other) = delete;
    BlobStorage& operator=(const BlobStorage& other) = delete;

    BlobStorage(BlobStorage&& other) noexcept;
    BlobStorage& operator=(BlobStorage&& other) noexcept;

    ~BlobStorage();

    ByteWriter* GetWriteStream(uint32 page);
    ByteReader* GetReadStream(uint32 page);
    
    HYP_NODISCARD void* GetData(StringHash key, SizeType size);
    bool PutData(StringHash key, const BlobHeader& header, const void* rawData);
    
    Result SaveManifest();
    Result SaveTOC();

    // Needs to be set by impl
    BlobStorageCallbacks callbacks;

private:
    bool InitMappedFile(MemoryMappedFile*& outMappedFile, uint32 page);

    void ClosePage(uint32 page);

    Result LoadManifest();
    Result LoadTOC();

    HYP_FIELD()
    FilePath m_baseDirectory;

    HYP_FIELD()
    uint64 m_pageSize;

    HYP_FIELD()
    Array<BlobMappingRange> m_freeRanges; // <---- TODO make use of this

    HYP_FIELD()
    Array<BlobPageData> m_pageData;

    BlobTableOfContents m_toc;

    mutable Mutex m_mutex;
};

} // namespace Hyperion
