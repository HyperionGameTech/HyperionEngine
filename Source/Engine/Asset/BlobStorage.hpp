/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Utilities/ValueStorage.hpp>
#include <Core/Utilities/Range.hpp>
#include <Core/Utilities/Result.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Memory/Allocator/ArenaAllocator.hpp>

#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/SharedMutex.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/IO/MemoryMappedFile.hpp>

#include <Asset/BlobStorageStructs.hpp>
#include <Asset/AssetBucket.hpp>

namespace Hyperion {

class ByteReader;
class ByteWriter;

class BlobTableOfContents;

struct BlobStorageReadScope;
struct BlobStorageWriteScope;

struct BlobStorageCallbacks
{
    void* context = nullptr;

    MemoryMappedFile* (*Open)(void* context, const char* name) = nullptr;
    void (*Close)(void* context, MemoryMappedFile* file) = nullptr;

    void (*Destroy)(void* context) = nullptr;
};

/*! \brief One entry of the cook plan passed to BlobStorage::BeginCook: the exact final size of the
 *  block file for a given asset bucket, computed up front from the full set of blob data being cooked. */
struct BlobBlockInfo
{
    uint32 bucketIndex = AssetBucket::InvalidIndex;
    uint64 totalSize = 0;
};

HYP_STRUCT()
struct BlobBlockData
{
    HYP_STRUCT_BODY(BlobBlockData)

    HYP_FIELD()
    uint64 cursor = 0;

    MemoryMappedFile* file = nullptr;
    MemoryMappedFileView* view = nullptr;
    ByteWriter* writeStream = nullptr;
    ByteReader* readStream = nullptr;
};

/*! \brief Cooked, per-bucket blob cache - Cooked by BlobStorageCookCommandlet */
HYP_CLASS()
class ENGINE_API BlobStorage : public ObjectBase
{
    HYP_OBJECT_BODY(BlobStorage);

public:
    BlobStorage();

    BlobStorage(const BlobStorage& other) = delete;
    BlobStorage& operator=(const BlobStorage& other) = delete;

    BlobStorage(BlobStorage&& other) noexcept = delete;
    BlobStorage& operator=(BlobStorage&& other) noexcept = delete;

    ~BlobStorage();

    HYP_FORCE_INLINE const FilePath& GetBaseDirectory() const
    {
        return m_baseDir;
    }

    void Lock(const FilePath& baseDir, bool readOnly);
    void Unlock();

    ByteWriter* GetWriteStream(uint32 bucketIndex);
    ByteReader* GetReadStream(uint32 bucketIndex);
    
    bool HasData(StringHash key, size_t size);
    bool GetData(StringHash key, size_t size, void*& outRawData);
    bool PutData(uint32 bucketIndex, StringHash key, const BlobHeader& header, const void* rawData);

    Result BeginCook(const Array<BlobBlockInfo>& blocks, bool zeroize = true, Array<uint32>* outResetBuckets = nullptr);

    /*! \brief Ends a cook pass, saving the table of contents and manifest and closing write streams. */
    Result FinishCook();

    Result SaveManifest();
    Result SaveTOC();

    bool IsDirty() const;

    Result SaveIfDirty();

    // Needs to be set by impl
    BlobStorageCallbacks callbacks;

private:
    void Initialize(const FilePath& baseDir, bool readOnly);
    void Shutdown();

    bool InitMappedFile(MemoryMappedFile*& outMappedFile, uint32 bucketIndex);

    /*! \brief Shared body of GetData()/HasData(). \p outRawData may be null to only test presence,
     *  and \p logErrors distinguishes a lookup whose failure is a real error from one that expects misses. */
    bool GetData_Internal(StringHash key, size_t size, void** outRawData, bool logErrors);

    void CloseBlock(uint32 bucketIndex);

    /*! \brief Unmaps, grows and remaps a block file so it can hold at least \p requiredSize bytes.
     *  Invalidates every pointer previously handed out by GetData() for that bucket, so it may only
     *  be called while the storage is write locked. */
    bool GrowBlock(uint32 bucketIndex, size_t requiredSize);

    Result LoadTOC();

    Result LoadTOC_Internal();
    Result SaveTOC_Internal();

    FilePath m_baseDir;

    // Indexed directly by AssetBucket::GetIndex(); entry 0 (AssetBucket::None) is unused.
    HYP_FIELD()
    Array<BlobBlockData> m_blockData;

    BlobTableOfContents* m_toc;

    SharedMutex m_lockState;

    mutable Mutex m_mutex;

    bool m_isReadOnly;
    bool m_isInitialized;
};

struct BlobStorageReadScope
{
    BlobStorage* blobStorage;

    BlobStorageReadScope(BlobStorage& blobStorage, const FilePath& baseDir)
        : blobStorage(&blobStorage)
    {
        blobStorage.Lock(baseDir, true);
    }

    ~BlobStorageReadScope()
    {
        blobStorage->Unlock();
    }

    bool Read(StringHash key, size_t size, void*& outRawData)
    {
        return blobStorage->GetData(key, size, outRawData);
    }
};

struct BlobStorageWriteScope
{
    BlobStorage* blobStorage;

    BlobStorageWriteScope(BlobStorage& blobStorage, const FilePath& baseDir)
        : blobStorage(&blobStorage)
    {
        blobStorage.Lock(baseDir, false);
    }

    ~BlobStorageWriteScope()
    {
        blobStorage->Unlock();
    }

    bool Put(uint32 bucketIndex, StringHash key, const BlobHeader& header, const void* rawData)
    {
        return blobStorage->PutData(bucketIndex, key, header, rawData);
    }
};

} // namespace Hyperion
