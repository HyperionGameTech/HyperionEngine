/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <AssetPch.hpp>

#include <Asset/BlobStorage.hpp>
#include <Asset/BlobStorageViews.hpp>
#include <Asset/SerializationUtils.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Core/IO/ByteReader.hpp>
#include <Core/IO/ByteWriter.hpp>

#include <BlobStorage.generated.inl>

namespace Hyperion {

ENGINE_API extern Pool* g_assetPool;

static void InitBlobStorage(BlobStorage& outStorage, const FilePath& baseDirectory, bool readOnly)
{
    MappedBlobStorage* mappedBlobStorage = HYP_POOL_NEW(g_assetPool, MappedBlobStorage, baseDirectory, readOnly);

    outStorage.callbacks.context = mappedBlobStorage;

    outStorage.callbacks.Destroy = [](void* context)
    {
        MappedBlobStorage* mappedBlobStorage = static_cast<MappedBlobStorage*>(context);
        mappedBlobStorage->Clear();

        PoolDelete(*g_assetPool, mappedBlobStorage);
    };

    outStorage.callbacks.Open = [](void* context, const char* name) -> MemoryMappedFile*
    {
        MappedBlobStorage* mappedBlobStorage = static_cast<MappedBlobStorage*>(context);

        MemoryMappedFile* file = mappedBlobStorage->Get(ANSIStringView(name));
        AssertDebug(file != nullptr, "Failed to open mapped file {} ({})", name, mappedBlobStorage->GetBaseDirectory());

        if (!file || !file->Open())
        {
            return nullptr;
        }

        return file;
    };

    outStorage.callbacks.Close = [](void* context, MemoryMappedFile* file)
    {
        file->Close();
    };
}

#pragma region BlobTableOfContents

// @TODO Refactor + fix...
// AssetPaths are now 12-byte structs that could be bitwise copied,
// no longer needing string keys.

class BlobTableOfContents
{
    enum class SlotState : uint8
    {
        Empty = 0,
        Occupied = 1,
        Deleted = 2
    };

public:
    HYP_DEF_POOL_NEW_DELETE(g_assetPool);

    static constexpr uint32 MaxKeyLength = 512;

    struct Value
    {
        uint32 bucketIndex;
        uint64 offset;
        uint64 size;
    };

    struct Entry
    {
        StringHash key;
        Value value;
        SlotState state;
    };

private:
    struct MapHeader
    {
        uint64 capacity;
        uint64 size;
        uint64 deleted;
    };

    ByteBuffer mem;
    bool dirty;

    MapHeader* GetHeader()
    {
        return reinterpret_cast<MapHeader*>(mem.Data());
    }

    const MapHeader* GetHeader() const
    {
        return reinterpret_cast<const MapHeader*>(mem.Data());
    }

    Entry* GetBuckets()
    {
        return reinterpret_cast<Entry*>(mem.Data() + sizeof(MapHeader));
    }

public:
    explicit BlobTableOfContents(size_t capacity = 1024)
        : dirty(false)
    {
        AllocateNew(capacity);
    }

    bool Get(StringHash key, Value& outValue) const
    {
        const MapHeader* header = GetHeader();
        const Entry* entries = reinterpret_cast<const Entry*>(mem.Data() + sizeof(MapHeader));

        size_t idx = key.GetHashCode().Value() % header->capacity;
        size_t startIdx = idx;

        while (entries[idx].state != SlotState::Empty)
        {
            if (entries[idx].state == SlotState::Occupied && key == entries[idx].key)
            {
                outValue = entries[idx].value;

                return true;
            }

            idx = (idx + 1) % header->capacity;

            if (idx == startIdx)
                return false;
        }

        return false;
    }

    void Put(StringHash key, const Value& value)
    {
        MapHeader* header = GetHeader();

        // resize if (occupied + deleted) > 70% of Capacity
        size_t totalLoad = header->size + header->deleted;
        if (totalLoad * 10 >= header->capacity * 7)
        {
            Resize(header->capacity * 2);
        }

        header = GetHeader();

        Entry* entries = GetBuckets();

        size_t idx = key.GetHashCode().Value() % header->capacity;

        int64 firstDeleted = -1;

        while (entries[idx].state != SlotState::Empty)
        {
            if (entries[idx].state == SlotState::Occupied)
            {
                if (key == entries[idx].key)
                {
                    // FOUND: Update existing value
                    entries[idx].value = value;

                    dirty = true;

                    return;
                }
            }
            else if (entries[idx].state == SlotState::Deleted)
            {
                // remember deleted elem to recycle slot
                if (firstDeleted == -1)
                {
                    firstDeleted = idx;
                }
            }

            idx = (idx + 1) % header->capacity;
        }

        // key not found. insert new.
        // if we passed a deleted element, recycle it. otherwise use the current EMPTY slot.
        size_t insertIdx = (firstDeleted != -1) ? size_t(firstDeleted) : idx;

        if (entries[insertIdx].state == SlotState::Deleted)
        {
            header->deleted--; // We are reviving a dead slot
        }

        entries[insertIdx].key = key;
        entries[insertIdx].value = value;
        entries[insertIdx].state = SlotState::Occupied;

        dirty = true;

        header->size++;
    }

    bool Delete(StringHash key)
    {
        MapHeader* header = GetHeader();
        Entry* entries = GetBuckets();

        size_t idx = key.GetHashCode().Value() % header->capacity;
        size_t startIdx = idx;

        while (entries[idx].state != SlotState::Empty)
        {
            if (entries[idx].state == SlotState::Occupied && key == entries[idx].key)
            {
                // mark deleted
                entries[idx].state = SlotState::Deleted;
                header->size--;
                header->deleted++;

                dirty = true;

                return true;
            }

            idx = (idx + 1) % header->capacity;

            if (idx == startIdx)
                return false;
        }

        return false; // Not found
    }

    const void* Data() const
    {
        return mem.Data();
    }

    bool Dirty() const
    {
        return dirty;
    }

    void Save(ByteWriter& stream)
    {
        stream.Write(mem);

        dirty = false;
    }

    static Result Load(ByteReader& stream, BlobTableOfContents& outToc)
    {
        if (stream.Eof())
        {
            return HYP_MAKE_ERROR(Error, "Unexpected end of file");
        }

        const size_t numToRead = stream.Max() - stream.Position();

        outToc.mem.SetSize(numToRead);

        size_t numRead = stream.Read((void*)outToc.mem.Data(), numToRead);

        outToc.dirty = false;

        if (numRead != numToRead)
        {
            return HYP_MAKE_ERROR(Error, "Read size ({}) does not equal expected size ({})", numRead, numToRead);
        }

        return {};
    }

private:
    bool Insert_Internal(MapHeader* header, Entry* entry, StringHash key, const Value& value)
    {
        size_t idx = key.GetHashCode().Value() % header->capacity;
        size_t startIdx = idx;

        while (entry[idx].state == SlotState::Occupied)
        {
            if (key == entry[idx].key)
            {
                entry[idx].value = value; // Update

                dirty = true;

                return true;
            }

            idx = (idx + 1) % header->capacity;

            if (idx == startIdx)
                return false;
        }

        entry[idx].key = key;
        entry[idx].value = value;
        entry[idx].state = SlotState::Occupied;

        dirty = true;

        header->size++;

        return true;
    }

    void AllocateNew(size_t capacity)
    {
        size_t totalBytes = sizeof(MapHeader) + (capacity * sizeof(Entry));
        mem.SetSize(totalBytes, /* zeroize */ true);

        MapHeader* header = GetHeader();

        header->capacity = capacity;
        header->size = 0;
        header->deleted = 0;
    }

    void Resize(size_t newCapacity)
    {
        ByteBuffer newMem;
        size_t totalBytes = sizeof(MapHeader) + (newCapacity * sizeof(Entry));
        newMem.SetSize(totalBytes, /* zeroize */ true);

        Entry* newBuckets = reinterpret_cast<Entry*>(newMem.Data() + sizeof(MapHeader));

        MapHeader* newHeader = reinterpret_cast<MapHeader*>(newMem.Data());
        newHeader->capacity = newCapacity;
        newHeader->size = 0;
        newHeader->deleted = 0;

        // rehash
        MapHeader* oldHeader = GetHeader();
        Entry* oldBuckets = GetBuckets();

        for (size_t i = 0; i < oldHeader->capacity; ++i)
        {
            // Only copy OCCUPIED
            if (oldBuckets[i].state == SlotState::Occupied)
            {
                Insert_Internal(newHeader, newBuckets, oldBuckets[i].key, oldBuckets[i].value);
            }
        }

        std::swap(newMem, mem);
    }
};

#pragma endregion BlobTableOfContents

#pragma region BlobStorage

BlobStorage::BlobStorage()
    : m_toc(nullptr),
      m_isReadOnly(false),
      m_isInitialized(false)
{
}

BlobStorage::~BlobStorage()
{
    Shutdown();
}

void BlobStorage::Initialize(const FilePath& baseDir, bool readOnly)
{
    Mutex::Guard guard(m_mutex);

    if (m_isInitialized)
    {
        return;
    }

    m_baseDir = baseDir;
    m_isReadOnly = readOnly;

    InitBlobStorage(*this, m_baseDir, /* readOnly */ m_isReadOnly);

    m_isInitialized = true;
}

void BlobStorage::Shutdown()
{
    Mutex::Guard guard(m_mutex);

    if (!m_isInitialized)
    {
        return;
    }

    m_isInitialized = false;
    
    for (uint32 bucketIndex = 0; bucketIndex < uint32(m_blockData.Size()); bucketIndex++)
    {
        CloseBlock(bucketIndex);
    }

    if (callbacks.Destroy)
    {
        callbacks.Destroy(callbacks.context);
    }

    if (m_toc != nullptr)
    {
        delete m_toc;
        m_toc = nullptr;
    }
}

void BlobStorage::Lock(const FilePath& baseDir, bool readOnly)
{
    if (readOnly)
    {
        m_lockState.LockReader();
    }
    else
    {
        m_lockState.LockWriter();
    }

    Initialize(baseDir, readOnly);
}

void BlobStorage::Unlock()
{
    if (!m_isReadOnly)
    {
        Shutdown();
        m_lockState.UnlockWriter();
    }
    else
    {
        const bool isLast = (m_lockState.UnlockReader() == 0);

        if (isLast)
        {
            // to ensure unique access.
            m_lockState.LockWriter();

            Shutdown();
            
            m_lockState.UnlockWriter();
        }
    }
}

ByteWriter* BlobStorage::GetWriteStream(uint32 bucketIndex)
{
    AssertDebug(!m_isReadOnly);
    if (m_isReadOnly)
    {
        return nullptr;
    }

    Mutex::Guard guard(m_mutex);

    if (m_blockData.Size() < MaxAssetBuckets)
    {
        m_blockData.Resize(MaxAssetBuckets);
    }

    BlobBlockData& blockData = m_blockData[bucketIndex];

    if (blockData.writeStream != nullptr)
    {
        return blockData.writeStream;
    }

    MemoryMappedFile* file;
    Assert(InitMappedFile(file, bucketIndex));

    blockData.writeStream = new MemoryMappedByteWriter(file);
    blockData.writeStream->Seek(blockData.cursor);

    return blockData.writeStream;
}

ByteReader* BlobStorage::GetReadStream(uint32 bucketIndex)
{
    Mutex::Guard guard(m_mutex);

    if (m_blockData.Size() < MaxAssetBuckets)
    {
        m_blockData.Resize(MaxAssetBuckets);
    }

    BlobBlockData& blockData = m_blockData[bucketIndex];

    if (blockData.readStream != nullptr)
    {
        return blockData.readStream;
    }

    MemoryMappedFile* file;
    Assert(InitMappedFile(file, bucketIndex));

    blockData.readStream = new MemoryMappedByteReader(file);
    blockData.readStream->Seek(0);

    return blockData.readStream;
}

bool BlobStorage::InitMappedFile(MemoryMappedFile*& outMappedFile, uint32 bucketIndex)
{
    Assert(bucketIndex != AssetBucket::InvalidIndex && bucketIndex < MaxAssetBuckets);

    if (m_blockData.Size() < MaxAssetBuckets)
    {
        m_blockData.Resize(MaxAssetBuckets);
    }

    BlobBlockData& blockData = m_blockData[bucketIndex];

    if (blockData.file != nullptr)
    {
        outMappedFile = blockData.file;
        return true;
    }

    const char* nameRaw = GetAssetBucketName(bucketIndex);

    if ((blockData.file = callbacks.Open(callbacks.context, nameRaw)))
    {
        Assert(blockData.file->IsOpen());

        outMappedFile = blockData.file;

        Assert(blockData.view == nullptr);

        blockData.view = new MemoryMappedFileView;

        if (!blockData.file->MapRange(0, 0, *blockData.view))
        {
            Assert(false, "Failed to map file to view!");

            blockData.file->Close();
            blockData.file = nullptr;

            return false;
        }

        return true;
    }

    return false;
}

bool BlobStorage::HasData(StringHash key, size_t size)
{
    Mutex::Guard guard(m_mutex);

    return GetData_Internal(key, size, nullptr, /* logErrors */ false);
}

bool BlobStorage::GetData(StringHash key, size_t size, void*& outRawData)
{
    Mutex::Guard guard(m_mutex);

    return GetData_Internal(key, size, &outRawData, /* logErrors */ true);
}

bool BlobStorage::GetData_Internal(StringHash key, size_t size, void** outRawData, bool logErrors)
{
    if (!m_toc)
    {
        m_toc = new BlobTableOfContents;

        if ((m_baseDir / "toc.bin").Exists())
        {
            if (Result res = LoadTOC_Internal(); res.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to load blob TOC for BlobStorage at {}: {}", m_baseDir, res.GetError().GetMessage());

                delete m_toc;
                m_toc = nullptr;

                return false;
            }
        }
    }

    BlobTableOfContents::Value tocValue;
    if (!m_toc->Get(key, tocValue))
    {
        if (logErrors)
        {
            HYP_LOG(Assets, Error, "Blob data not found in table of contents {} for BlobStorage at {}", key.GetHashCode().Value(), m_baseDir);
        }

        return false;
    }

    if (tocValue.size != size)
    {
        if (logErrors)
        {
            HYP_LOG(Assets, Error, "Blob data does not match expected size ({}): {}", size, key.GetHashCode().Value());
        }

        return false;
    }

    MemoryMappedFile* file = nullptr;
    if (!InitMappedFile(file, tocValue.bucketIndex))
    {
        if (logErrors)
        {
            HYP_LOG(Assets, Error, "Failed to initialize mapped file for bucket '{}'", GetAssetBucketName(tocValue.bucketIndex));
        }

        return false;
    }

    BlobBlockData& blockData = m_blockData[tocValue.bucketIndex];

    if (tocValue.offset + size > blockData.file->FileSize())
    {
        if (logErrors)
        {
            HYP_LOG(Assets, Error, "Blob data for key {} in BlobStorage at {} is out of range of its block file (offset={}, size={}, file size={}), table of contents is stale or corrupt",
                key.GetHashCode().Value(), m_baseDir, tocValue.offset, size, blockData.file->FileSize());
        }

        return false;
    }

    if (outRawData)
    {
        *outRawData = reinterpret_cast<uint8*>(blockData.view->Data()) + tocValue.offset;
    }

    return true;
}

Result BlobStorage::BeginCook(const Array<BlobBlockInfo>& blocks, bool zeroize, Array<uint32>* outResetBuckets)
{
    Mutex::Guard guard(m_mutex);

    if (m_blockData.Size() < MaxAssetBuckets)
    {
        m_blockData.Resize(MaxAssetBuckets);
    }

    for (const BlobBlockInfo& blockInfo : blocks)
    {
        Assert(blockInfo.bucketIndex != AssetBucket::InvalidIndex && blockInfo.bucketIndex < MaxAssetBuckets,
            "Invalid asset bucket index in cook block info");

        CloseBlock(blockInfo.bucketIndex);

        BlobBlockData& blockData = m_blockData[blockInfo.bucketIndex];

        const char* nameRaw = GetAssetBucketName(blockInfo.bucketIndex);

        blockData.file = callbacks.Open(callbacks.context, nameRaw);

        if (!blockData.file)
        {
            return HYP_MAKE_ERROR(Error, "Failed to open block file for asset bucket '{}'", GetAssetBucketName(blockInfo.bucketIndex));
        }

        if (zeroize)
        {
            if (!blockData.file->Resize(blockInfo.totalSize))
            {
                return HYP_MAKE_ERROR(Error, "Failed to resize block file for asset bucket '{}' to {} bytes", GetAssetBucketName(blockInfo.bucketIndex), blockInfo.totalSize);
            }

            blockData.cursor = 0;

            if (outResetBuckets)
            {
                outResetBuckets->PushBack(blockInfo.bucketIndex);
            }
        }
        else
        {
            const size_t existingSize = blockData.file->FileSize();

            if (!blockData.file->EnsureCapacity(existingSize + blockInfo.totalSize))
            {
                return HYP_MAKE_ERROR(Error, "Failed to grow block file for asset bucket '{}' to {} bytes", GetAssetBucketName(blockInfo.bucketIndex), existingSize + blockInfo.totalSize);
            }

            blockData.cursor = existingSize;

            if (existingSize == 0 && outResetBuckets)
            {
                outResetBuckets->PushBack(blockInfo.bucketIndex);
            }
        }

        blockData.view = new MemoryMappedFileView;

        if (!blockData.file->MapRange(0, 0, *blockData.view))
        {
            return HYP_MAKE_ERROR(Error, "Failed to map block file for asset bucket '{}'", GetAssetBucketName(blockInfo.bucketIndex));
        }
    }

    return {};
}

bool BlobStorage::PutData(uint32 bucketIndex, StringHash key, const BlobHeader& header, const void* rawData)
{
    Mutex::Guard guard(m_mutex);

    Assert(bucketIndex != AssetBucket::InvalidIndex && bucketIndex < m_blockData.Size());

    BlobBlockData& blockData = m_blockData[bucketIndex];
    Assert(blockData.file != nullptr, "BeginCook must be called before PutData");

    if (!m_toc)
    {
        m_toc = new BlobTableOfContents;

        if ((m_baseDir / "toc.bin").Exists())
        {
            if (Result res = LoadTOC_Internal(); res.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to load blob TOC for BlobStorage at {}: {}", m_baseDir, res.GetError().GetMessage());

                delete m_toc;
                m_toc = nullptr;

                return false;
            }
        }
    }

    const size_t headerOffset = ByteUtil::AlignAs(blockData.cursor, alignof(BlobHeader));
    const size_t totalBlobSize = sizeof(BlobHeader) + header.payloadOffset + header.payloadSize;

    if (headerOffset + totalBlobSize > blockData.view->Size())
    {
        HYP_LOG(Assets, Error, "Blob data for key {} exceeds the reserved block size for asset bucket {} ({} > {}), block sizes must be computed before BeginCook is called",
            key.GetHashCode().Value(), bucketIndex, headerOffset + totalBlobSize, blockData.view->Size());

        return false;
    }

    ByteWriter* writeStream = blockData.writeStream;
    if (writeStream == nullptr)
    {
        blockData.writeStream = new MemoryMappedByteWriter(blockData.file);
        writeStream = blockData.writeStream;
    }

    writeStream->Seek(headerOffset);
    writeStream->Write(header);

    writeStream->Seek(writeStream->Position() + header.payloadOffset);

    const size_t offset = writeStream->Position();

    writeStream->Write(rawData, header.payloadSize);

    blockData.cursor = writeStream->Position();

    BlobTableOfContents::Value entryValue {};
    entryValue.bucketIndex = bucketIndex;
    entryValue.offset = offset;
    entryValue.size = header.payloadSize;

    m_toc->Put(key, entryValue);

    return true;
}

Result BlobStorage::FinishCook()
{
    Mutex::Guard guard(m_mutex);

    for (uint32 bucketIndex = 0; bucketIndex < uint32(m_blockData.Size()); bucketIndex++)
    {
        BlobBlockData& blockData = m_blockData[bucketIndex];

        if (blockData.writeStream != nullptr)
        {
            blockData.writeStream->Close();

            delete blockData.writeStream;
            blockData.writeStream = nullptr;
        }
    }

    return SaveTOC_Internal();
}

void BlobStorage::CloseBlock(uint32 bucketIndex)
{
    if (bucketIndex >= m_blockData.Size())
    {
        return;
    }

    BlobBlockData& blockData = m_blockData[bucketIndex];

    if (blockData.writeStream != nullptr)
    {
        blockData.writeStream->Close();

        delete blockData.writeStream;
        blockData.writeStream = nullptr;
    }

    if (blockData.readStream != nullptr)
    {
        blockData.readStream->Close();

        delete blockData.readStream;
        blockData.readStream = nullptr;
    }

    if (blockData.view != nullptr)
    {
        blockData.view->Close();

        delete blockData.view;
        blockData.view = nullptr;
    }

    MemoryMappedFile*& file = blockData.file;
    if (file != nullptr)
    {
        callbacks.Close(callbacks.context, file);
        file = nullptr;
    }
}

Result BlobStorage::SaveTOC()
{
    Mutex::Guard guard(m_mutex);

    return SaveTOC_Internal();
}

bool BlobStorage::IsDirty() const
{
    Mutex::Guard guard(m_mutex);

    return m_toc && m_toc->Dirty();
}

Result BlobStorage::SaveIfDirty()
{
    Mutex::Guard guard(m_mutex);

    if (!m_toc || !m_toc->Dirty())
    {
        return {};
    }

    Result result = SaveTOC_Internal();

    if (result.HasError())
    {
        return result;
    }

    return {};
}

Result BlobStorage::LoadTOC()
{
    Mutex::Guard guard(m_mutex);
    return LoadTOC_Internal();
}

Result BlobStorage::LoadTOC_Internal()
{
    const FilePath tocPath = m_baseDir / "toc.bin";

    if (!tocPath.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Blob table of contents file does not exist: {}", tocPath);
    }

    FileByteReader reader { tocPath };

    if (reader.Eof())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open BlobStorage table of contents file: {}", tocPath);
    }

    if (m_toc)
    {
        delete m_toc;
    }

    m_toc = new BlobTableOfContents;

    return BlobTableOfContents::Load(reader, *m_toc);
}

Result BlobStorage::SaveTOC_Internal()
{
    if (!m_toc)
    {
        m_toc = new BlobTableOfContents;
    }

    const FilePath tocPath = m_baseDir / "toc.bin";

    FileByteWriter tocWriter { tocPath };

    if (!tocWriter.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open table of contents file for BlobStorage at path: {}, errno: {}", tocPath, std::strerror(errno));
    }

    m_toc->Save(tocWriter);

    return {};
}

#pragma endregion BlobStorage

} // namespace Hyperion
