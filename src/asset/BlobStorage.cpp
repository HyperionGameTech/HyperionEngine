/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <asset/BlobStorage.hpp>
#include <asset/BlobStorageViews.hpp>

#include <serialization/SerializationUtils.hpp>

#include <core/json/JSON.hpp>

#include <io/ByteReader.hpp>
#include <io/ByteWriter.hpp>

#include <BlobStorage.generated.inl>

namespace Hyperion {

HYP_API extern Pool* g_assetPool;

static void InitBlobStorage(BlobStorage& outStorage, const FilePath& baseDirectory, uint64 pageSize)
{
    MappedBlobStorage* mappedBlobStorage = PoolNew<MappedBlobStorage>(
        *g_assetPool,
        baseDirectory,
        pageSize,
        /* readOnly */ false);

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

        if (!file->Open())
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

#pragma region BlobStorage

BlobStorage::BlobStorage()
    : m_baseDirectory(FilePath()),
      m_pageSize(DefaultPageSize)
{
}

BlobStorage::BlobStorage(const FilePath& baseDirectory, uint64 pageSize)
    : m_baseDirectory(baseDirectory),
      m_pageSize(pageSize)
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
      m_pageData(std::move(other.m_pageData))
{
    other.callbacks = {};
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

    m_baseDirectory = std::move(other.m_baseDirectory);
    m_pageSize = other.m_pageSize;
    m_freeRanges = std::move(other.m_freeRanges);
    m_pageData = std::move(other.m_pageData);
    m_pageSize = other.m_pageSize;

    other.callbacks = {};

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

    const SizeType previousFileSize = pd.file ? pd.file->FileSize() : 0;

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

HYP_NODISCARD void* BlobStorage::GetData(StringHash key, SizeType size)
{
    Mutex::Guard guard(m_mutex);

    BlobTableOfContents::Value tocValue;
    if (!m_toc.Get(key, tocValue))
    {
        AssertDebug(false, "Failed to get value from table of contents for key: {}", key.GetHashCode().Value());

        return nullptr;
    }

    if (tocValue.size != size)
    {
        AssertDebug(false, "Data corrupt! Expected size: {} but got {} for key: {}", size, tocValue.size, key.GetHashCode().Value());

        return nullptr;
    }

    MemoryMappedFile* file = nullptr;
    if (!InitMappedFile(file, tocValue.page))
    {
        HYP_FAIL("Failed to map file");

        return nullptr;
    }

    BlobPageData& pd = m_pageData[tocValue.page];

    void* address = reinterpret_cast<void*>(reinterpret_cast<UIntPtr>(pd.view->Data()) + tocValue.offset);
    AssertDebug(reinterpret_cast<UIntPtr>(address) - reinterpret_cast<UIntPtr>(pd.view->Data()) + size <= pd.file->FileSize());

    return address;
}

bool BlobStorage::PutData(StringHash key, const BlobHeader& header, const void* rawData)
{
    Mutex::Guard guard(m_mutex);
    
    const SizeType totalBlobSize = header.payloadOffset + header.payloadSize;
    const SizeType totalBlobSizePlusHeader = sizeof(BlobHeader) + totalBlobSize;

    BlobTableOfContents::Value existingValue;
    if (m_toc.Get(key, existingValue))
    {
        if (existingValue.size != header.payloadSize)
        {
            // needs new allocation if changed!
            Assert(m_toc.Delete(key));
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

        const SizeType headerOffset = ByteUtil::AlignAs(pd.cursor, alignof(BlobHeader));
        const SizeType requiredSize = headerOffset + totalBlobSizePlusHeader;

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
        
        const SizeType offset = writeStream->Position();

        // fill data
        writeStream->Write(rawData, header.payloadSize);

        pd.cursor = writeStream->Position();

        m_toc.Put(key, BlobTableOfContents::Value {
            .page = page,
            .offset = offset,
            .size = header.payloadSize
        });

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

Result BlobStorage::SaveManifest()
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

    manifestWriter.WriteString(JSON::Value(std::move(manifestJson)).ToString(true).ToUtf8());

    return {};
}

Result BlobStorage::LoadManifest()
{
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

Result BlobStorage::SaveTOC()
{
    const FilePath tocPath = m_baseDirectory / "storage.toc";

    FileByteWriter tocWriter { tocPath };

    if (!tocWriter.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open table of contents file for BlobStorage at path: {}, errno: {}", tocPath, std::strerror(errno));
    }

    m_toc.Save(tocWriter);

    return {};
}

Result BlobStorage::LoadTOC()
{
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

    return BlobTableOfContents::Load(reader, m_toc);
}

#pragma endregion BlobStorage

} // namespace Hyperion
