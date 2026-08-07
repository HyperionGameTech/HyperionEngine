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

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/IO/MemoryMappedFile.hpp>

#include <Asset/BlobStorageStructs.hpp>
#include <Asset/AssetBucket.hpp>

namespace Hyperion {

class ByteReader;
class ByteWriter;

class BlobTableOfContents;

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
    explicit BlobStorage(const FilePath& baseDir, bool readOnly = true);

    BlobStorage(const BlobStorage& other) = delete;
    BlobStorage& operator=(const BlobStorage& other) = delete;

    BlobStorage(BlobStorage&& other) noexcept = delete;
    BlobStorage& operator=(BlobStorage&& other) noexcept = delete;

    ~BlobStorage();

    HYP_FORCE_INLINE const FilePath& GetBaseDirectory() const
    {
        return m_baseDir;
    }

    void Initialize();
    void Shutdown();

    ByteWriter* GetWriteStream(uint32 bucketIndex);
    ByteReader* GetReadStream(uint32 bucketIndex);

    bool GetData(StringHash key, size_t size, void*& outRawData);

    /*! \brief Begins a cook pass: creates (or truncates) one block file per entry in \p blocks, each
     *  resized to its exact final size, ready to be filled via PutData. Must be called before any
     *  PutData call and cannot be combined with reading an already-cooked, read-only BlobStorage. */
    Result BeginCook(const Array<BlobBlockInfo>& blocks);

    /*! \brief Appends one blob to the block belonging to \p bucketIndex, which must have been
     *  reserved via BeginCook with enough space for it. */
    bool PutData(uint32 bucketIndex, StringHash key, const BlobHeader& header, const void* rawData);

    /*! \brief Ends a cook pass, saving the table of contents and manifest and closing write streams. */
    Result FinishCook();

    Result SaveManifest();
    Result SaveTOC();

    bool IsDirty() const;

    Result SaveIfDirty();

    // Needs to be set by impl
    BlobStorageCallbacks callbacks;

private:
    bool InitMappedFile(MemoryMappedFile*& outMappedFile, uint32 bucketIndex);

    void CloseBlock(uint32 bucketIndex);

    Result LoadTOC();

    Result SaveTOC_Internal();

    FilePath m_baseDir;

    // Indexed directly by AssetBucket::GetIndex(); entry 0 (AssetBucket::None) is unused.
    HYP_FIELD()
    Array<BlobBlockData> m_blockData;

    BlobTableOfContents* m_toc;

    mutable Mutex m_mutex;

    bool m_isReadOnly;
    bool m_isInitialized;
};

} // namespace Hyperion
