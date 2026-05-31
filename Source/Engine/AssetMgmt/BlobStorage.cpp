/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <AssetPch.hpp>

#include <asset/BlobStorage.hpp>
#include <asset/BlobStorageViews.hpp>
#include <asset/SerializationUtils.hpp>

#include <Core/json/JSON.hpp>

#include <Core/io/ByteReader.hpp>
#include <Core/io/ByteWriter.hpp>

#include <BlobStorage.generated.inl>

namespace Hyperion {

ENGINE_API extern Pool* g_assetPool;

static void InitBlobStorage(BlobStorage& outStorage, const FilePath& baseDirectory, uint64 pageSize)
{
    MappedBlobStorage* mappedBlobStorage = HYP_POOL_NEW(g_assetPool, MappedBlobStorage, baseDirectory, pageSize, /* readOnly */ false);

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
        uint32 page;
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

        entries[idx].key = key;
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
    : m_baseDirectory(FilePath()),
      m_pageSize(DefaultPageSize),
      m_toc(nullptr)
{
}

BlobStorage::BlobStorage(const FilePath& baseDirectory, uint64 pageSize)
    : m_baseDirectory(baseDirectory),
      m_pageSize(pageSize),
      m_toc(nullptr)
{
    InitBlobStorage(*this, baseDirectory, pageSize);

    if (Result result = LoadManifest(); result.HasError())
    {
        HYP_LOG(Assets, Error, "Failed to load BlobStorage manifest: {}", result.GetError().GetMessage());
    }

    if (Result result = LoadTOC(); result.HasError())
    {
        HYP_LOG(Assets, Error, "Failed to load BlobStorage table of contents: {}", result.GetError().GetMessage());
    }
}

BlobStorage::BlobStorage(BlobStorage&& other) noexcept
    : callbacks(std::move(other.callbacks)),
      m_baseDirectory(std::move(other.m_baseDirectory)),
      m_pageSize(other.m_pageSize),
      m_freeRanges(std::move(other.m_freeRanges)),
      m_pageData(std::move(other.m_pageData)),
      m_toc(other.m_toc)
{
    other.callbacks = {};
    other.m_toc = nullptr;
}

BlobStorage& BlobStorage::operator=(BlobStorage&& other) noexcept
{
    if (&other == this)
    {
        return *this;
    }

    for (uint32 page = 0; page < uint32(m_pageData.Size()); page++)
    {
        ClosePage(page);
    }

    if (callbacks.Destroy)
    {
        callbacks.Destroy(callbacks.context);
    }

    callbacks = std::move(other.callbacks);

    if (m_toc != nullptr)
    {
        delete m_toc;
    }

    m_baseDirectory = std::move(other.m_baseDirectory);
    m_pageSize = other.m_pageSize;
    m_freeRanges = std::move(other.m_freeRanges);
    m_pageData = std::move(other.m_pageData);
    m_pageSize = other.m_pageSize;
    m_toc = other.m_toc;

    other.callbacks = {};
    other.m_toc = nullptr;

    return *this;
}

BlobStorage::~BlobStorage()
{
    for (uint32 page = 0; page < uint32(m_pageData.Size()); page++)
    {
        ClosePage(page);
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

ByteWriter* BlobStorage::GetWriteStream(uint32 page)
{
    Mutex::Guard guard(m_mutex);

    if (page >= m_pageData.Size())
    {
        m_pageData.Resize(page + 1);
    }

    BlobPageData& pd = m_pageData[page];

    if (pd.writeStream != nullptr)
    {
        return pd.writeStream;
    }

    MemoryMappedFile* file;
    Assert(InitMappedFile(file, page));

    pd.writeStream = new MemoryMappedByteWriter(file);
    pd.writeStream->Seek(pd.cursor);

    return pd.writeStream;
}

ByteReader* BlobStorage::GetReadStream(uint32 page)
{
    Mutex::Guard guard(m_mutex);

    if (page >= m_pageData.Size())
    {
        m_pageData.Resize(page + 1);
    }

    BlobPageData& pd = m_pageData[page];

    if (pd.readStream != nullptr)
    {
        return pd.readStream;
    }

    MemoryMappedFile* file;
    Assert(InitMappedFile(file, page));

    pd.readStream = new MemoryMappedByteReader(file);
    pd.readStream->Seek(0);

    return pd.readStream;
}

bool BlobStorage::InitMappedFile(MemoryMappedFile*& outMappedFile, uint32 page)
{
    Assert(m_baseDirectory.Length() != 0);

    if (page >= m_pageData.Size())
    {
        m_pageData.Resize(page + 1);
    }

    BlobPageData& pd = m_pageData[page];

    if (pd.file != nullptr)
    {
        outMappedFile = pd.file;
        return true;
    }

    const size_t previousFileSize = pd.file ? pd.file->FileSize() : 0;

    ClosePage(page);

    if ((pd.file = callbacks.Open(callbacks.context, ("storage." + String::ToString(page)).Data())))
    {
        Assert(pd.file->IsOpen());

        outMappedFile = pd.file;

        Assert(pd.view == nullptr);

        // map entire file
        pd.view = new MemoryMappedFileView;

        if (!pd.file->MapRange(0, 0, *pd.view))
        {
            Assert(false, "Failed to map file to view!");

            pd.file->Close();
            pd.file = nullptr;

            return false;
        }

        return true;
    }

    return false;
}

bool BlobStorage::GetData(StringHash key, size_t size, void*& outRawData)
{
    Mutex::Guard guard(m_mutex);

    if (!m_toc)
    {
        HYP_LOG(Assets, Warning, "Table of contents is NULL");

        return false;
    }

    BlobTableOfContents::Value tocValue;
    if (!m_toc->Get(key, tocValue))
    {
        HYP_LOG(Assets, Warning, "Blob data not found in table of contents: {}", key.GetHashCode().Value());

        return false;
    }

    if (tocValue.size != size)
    {
        HYP_LOG(Assets, Warning, "Blob data does not match expected size ({}): {}", size, key.GetHashCode().Value());

        return false;
    }

    MemoryMappedFile* file = nullptr;
    if (!InitMappedFile(file, tocValue.page))
    {
        HYP_FAIL("Failed to map file");

        return false;
    }

    BlobPageData& pd = m_pageData[tocValue.page];

    void* address = reinterpret_cast<void*>(reinterpret_cast<UIntPtr>(pd.view->Data()) + tocValue.offset);
    AssertDebug(reinterpret_cast<UIntPtr>(address) - reinterpret_cast<UIntPtr>(pd.view->Data()) + size <= pd.file->FileSize());

    outRawData = address;

    return true;
}

bool BlobStorage::PutData(StringHash key, const BlobHeader& header, const void* rawData)
{
    Mutex::Guard guard(m_mutex);

    if (!m_toc)
    {
        m_toc = new BlobTableOfContents;
    }

    const size_t totalBlobSize = header.payloadOffset + header.payloadSize;
    const size_t totalBlobSizePlusHeader = sizeof(BlobHeader) + totalBlobSize;

    BlobTableOfContents::Value existingValue;
    if (m_toc->Get(key, existingValue))
    {
        if (existingValue.size != header.payloadSize)
        {
            // needs new allocation if changed!
            Assert(m_toc->Delete(key));
        }
    }

    if (totalBlobSizePlusHeader > m_pageSize)
    {
        return false;
    }

    if (m_pageData.Empty())
    {
        m_pageData.Resize(1);
    }

    const auto TryAllocateInPage = [&](uint32 page, bool& outHasSpace) -> bool
    {
        if (page >= m_pageData.Size())
        {
            m_pageData.Resize(page + 1);
        }

        BlobPageData& pd = m_pageData[page];

        const size_t headerOffset = ByteUtil::AlignAs(pd.cursor, alignof(BlobHeader));
        const size_t requiredSize = headerOffset + totalBlobSizePlusHeader;

        if (requiredSize > m_pageSize)
        {
            outHasSpace = false;
            return false;
        }

        outHasSpace = true;

        MemoryMappedFile* file = nullptr;
        if (!InitMappedFile(file, page))
        {
            return false;
        }

        ByteWriter* writeStream = pd.writeStream;
        if (writeStream == nullptr)
        {
            pd.writeStream = new MemoryMappedByteWriter(file);
            writeStream = pd.writeStream;
        }

        writeStream->Seek(headerOffset);
        writeStream->Write(header);

        writeStream->Seek(writeStream->Position() + header.payloadOffset);

        const size_t offset = writeStream->Position();

        // fill data
        writeStream->Write(rawData, header.payloadSize);

        pd.cursor = writeStream->Position();

        BlobTableOfContents::Value entryValue {};
        entryValue.page = page;
        entryValue.offset = offset;
        entryValue.size = header.payloadSize;

        m_toc->Put(key, entryValue);

        return true;
    };

    const uint32 lastPage = uint32(m_pageData.Size() - 1);

    bool hasSpaceInLastPage = false;
    if (TryAllocateInPage(lastPage, hasSpaceInLastPage))
    {
        return true;
    }

    if (hasSpaceInLastPage)
    {
        return false;
    }

    bool hasSpaceInNewPage = false;
    if (TryAllocateInPage(lastPage + 1, hasSpaceInNewPage))
    {
        return true;
    }

    return false;
}

void BlobStorage::ClosePage(uint32 page)
{
    if (page >= m_pageData.Size())
    {
        return;
    }

    BlobPageData& pd = m_pageData[page];

    if (pd.writeStream != nullptr)
    {
        pd.writeStream->Close();

        delete pd.writeStream;
        pd.writeStream = nullptr;
    }

    if (pd.readStream != nullptr)
    {
        pd.readStream->Close();

        delete pd.readStream;
        pd.readStream = nullptr;
    }

    if (pd.view != nullptr)
    {
        pd.view->Close();

        delete pd.view;
        pd.view = nullptr;
    }

    MemoryMappedFile*& file = pd.file;
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

Result BlobStorage::SaveManifest()
{
    Mutex::Guard guard(m_mutex);

    return SaveManifest_Internal();
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

    result = SaveManifest_Internal();

    if (result.HasError())
    {
        return result;
    }

    return {};
}

Result BlobStorage::LoadManifest()
{
    Mutex::Guard guard(m_mutex);

    const FilePath manifestPath = m_baseDirectory / "Manifest.json";

    if (!manifestPath.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Manifest path does not exist: {}", manifestPath);
    }

    FileByteReader reader { manifestPath };

    if (reader.Eof())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open BlobStorage manifest file: {}", manifestPath);
    }

    JSON::ParseResult parseResult = JSON::Parse(String(reader.Read(reader.Max()).ToByteView()));

    if (!parseResult.ok)
    {
        return HYP_MAKE_ERROR(Error, "Failed to parse BlobStorage manifest file at {}: {}", manifestPath, parseResult.message);
    }

    JSON::Value json = parseResult.value;

    if (!json.IsObject())
    {
        return HYP_MAKE_ERROR(Error, "BlobStorage manifest file is invalid JSON!");
    }

    json.AsObject().Erase("BaseDirectory");
    json.AsObject().Erase("PageSize");

    BoxedValue thisBoxed(HandleFromThis());

    if (!ObjectFromJSON(json.AsObject(), InstanceClass(), thisBoxed))
    {
        HYP_LOG(Assets, Error, "Failed to deserialize manifest JSON for BlobStorage at '{}'", manifestPath);

        return HYP_MAKE_ERROR(Error, "Failed to load BlobStorage manifset file");
    }

    return {};
}

Result BlobStorage::LoadTOC()
{
    Mutex::Guard guard(m_mutex);

    const FilePath tocPath = m_baseDirectory / "storage.toc";

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

Result BlobStorage::SaveManifest_Internal()
{
    if (m_baseDirectory.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Base directory not set");
    }

    if (!m_baseDirectory.IsDirectory() && !m_baseDirectory.MkDir())
    {
        return HYP_MAKE_ERROR(Error, "Not a valid directory and could not make directory: {}", m_baseDirectory);
    }

    const FilePath manifestPath = m_baseDirectory / "Manifest.json";

    FileByteWriter manifestWriter { manifestPath };

    if (!manifestWriter.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open manifest file for BlobStorage at path: {}, errno: {}", manifestPath, std::strerror(errno));
    }

    JSON::Object manifestJson;
    ObjectToJSON(InstanceClass(), BoxedValue(HandleFromThis()), manifestJson);

    manifestJson.Erase("BaseDirectory");

    manifestWriter.WriteString(JSON::Value(std::move(manifestJson)).ToString(true).ToUtf8());

    return {};
}

Result BlobStorage::SaveTOC_Internal()
{
    if (!m_toc)
    {
        m_toc = new BlobTableOfContents;
    }

    const FilePath tocPath = m_baseDirectory / "storage.toc";

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
