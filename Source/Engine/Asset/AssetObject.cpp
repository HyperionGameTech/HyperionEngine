/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <AssetPch.hpp>

#include <Asset/AssetObject.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetBatch.hpp>
#include <Asset/Assets.hpp>
#include <Asset/BlobStorage.hpp>
#include <Asset/SerializationUtils.hpp>

#include <Core/Utilities/DeferredScope.hpp>
#include <Core/Utilities/GlobalContext.hpp>

#include <Core/IO/BufferedByteReader.hpp>
#include <Core/IO/ByteWriter.hpp>

#include <Core/JSON/JSON.hpp>

#include <System/MessageBox.hpp>

#include <Framework/EngineDriver.hpp>

#include <AssetObject.generated.inl>

namespace Hyperion {

//! for debugging
static constexpr bool DebugDisableUnload = false;

extern HYP_NODISCARD String SanitizeName(const UTF8StringView& nameStr);
extern HYP_NODISCARD Name SanitizeName(Name name);
extern HYP_NODISCARD Name CreateFriendlyName(Name name);

template <class T>
static Name GetUniqueName(Name baseName, T&& elements)
{
    baseName = SanitizeName(baseName);

    String str = *baseName;

    int counter = 0;
    while (elements.FindAs(StringHash(*str)) != elements.End())
    {
        counter++;

        str = HYP_FORMAT("{}{}", *baseName, counter);
    }

    if (counter > 0)
    {
        return CreateNameFromDynamicString(str);
    }

    return baseName;
}

#pragma region AssetObject

AssetObject::AssetObject()
    : m_flags(AssetObjectFlags::None),
      m_assetIndex(AssetDesc::InvalidIndex),
      m_rwState(0),
      m_isBlobLoaded(false)
{
}

AssetObject::AssetObject(Name name)
    : m_name(SanitizeName(name)),
      m_flags(AssetObjectFlags::None),
      m_assetIndex(AssetDesc::InvalidIndex),
      m_rwState(0),
      m_isBlobLoaded(false)
{
}

AssetObject::~AssetObject()
{
    if (!(m_rwState & 0x1)) // check not locked from derived dtor
    {
        // add writer here to wait for all reads to complete and
        // block new readers/writers from acquiring the resource while we're destroying it.
        LockWriter(/* doInitialize */ false);
    }
}

void AssetObject::SetAssetFlags(EnumFlags<AssetObjectFlags> flags)
{
    if (m_flags != flags)
    {
        const bool wasPersistent = m_flags[AssetObjectFlags::Persistent];

        m_flags = flags;

        const bool isPersistent = m_flags[AssetObjectFlags::Persistent];

        if (wasPersistent != isPersistent)
        {
            SetPersistentRequested(isPersistent, /* setFlag */ false);
        }

        MarkDirty();
    }
}

void AssetObject::MarkDirty()
{
    if (IsTransient() || !IsRegistered())
    {
        return;
    }

    if (IsGlobalContextActive<AssetLoadingContext>())
    {
        return;
    }

    Handle<AssetRegistry> registry = GetAssetRegistry();
    AssertDebug(registry.IsValid());

    if (!registry.IsValid())
    {
        HYP_LOG(Assets, Warning, "Cannot mark asset '{}' dirty: no active asset registry for path '{}'", m_name, m_assetPath.ToString());
        return;
    }

    registry->MarkAssetDirty(*this);
}

void AssetObject::SetPersistentRequested(
    bool persistentlyLoaded, bool setFlag, bool markDirty)
{
    if (setFlag && m_flags[AssetObjectFlags::Persistent] != persistentlyLoaded)
    {
        if (markDirty)
        {
            MarkDirty();
        }

        m_flags[AssetObjectFlags::Persistent] = persistentlyLoaded;
    }
}

void AssetObject::SetIsTransient(bool isTransient)
{
    m_flags[AssetObjectFlags::Transient] = isTransient;

    if (IsTransient())
    {
        // needs to be kept in memory if transient
        SetPersistentRequested(true, /* setFlag */ false);
    }
    else
    {
        SetPersistentRequested(false, /* setFlag */ false);
    }
}

Result AssetObject::Rename(Name name)
{
    name = SanitizeName(name);

    if (name == m_name)
    {
        // same name, do nothing
        return {};
    }

    // @TODO Need to invoke on AssetRegistry, so it updates the AssetDesc

    m_name = name;
    m_friendlyName = CreateFriendlyName(name);

    MarkDirty();

    return {};
}

bool AssetObject::IsSaved() const
{
    return m_assetPath.IsValid() && !IsTransient();
}

Result AssetObject::Save()
{
    AssertDebug(IsSaved());

    Handle<AssetRegistry> registry = GetAssetRegistry();
    AssertDebug(registry.IsValid());

    if (!registry.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "No active asset registry for path: {}", m_assetPath.ToString());
    }

    return SaveAs(registry->GetManifestPath(m_assetPath));
}

Result AssetObject::SaveAs(const FilePath& manifestPath)
{
    auto readScope = GetReadScope();

    // save our manifest first
    if (manifestPath.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Asset manifest path is empty, cannot save");
    }

    if (manifestPath.GetExtension() != "json")
    {
        return HYP_MAKE_ERROR(Error, "Asset manifest path must have .json extension");
    }

    const FilePath dir = manifestPath.BasePath();

    if (!dir.Exists() || !dir.IsDirectory())
    {
        return HYP_MAKE_ERROR(Error, "Path '{}' is not a valid directory, cannot save asset", dir);
    }

    Handle<AssetRegistry> registry = GetAssetRegistry();
    AssertDebug(registry.IsValid());

    if (!registry.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "No active asset registry for path: {}", m_assetPath.ToString());
    }

    registry->PutAssetsDeep(MakeStrongRef(this));

    BlobStorage* blobStorage = registry->HasBlobStorage() ? &registry->GetBlobStorage() : nullptr;

    Result saveBlobDataResult = SaveBlobData(blobStorage, dir);
    if (saveBlobDataResult.HasError())
    {
        return saveBlobDataResult;
    }

    // save manifest after updating blob info

    {
        FileByteWriter manifestWriter { manifestPath };

        if (!manifestWriter.IsOpen())
        {
            return HYP_MAKE_ERROR(Error, "Failed to open manifest file for asset '{}', errno: {}", m_name, std::strerror(errno));
        }

        if (Result saveManifestResult = SaveManifest(manifestWriter); saveManifestResult.HasError())
        {
            return HYP_MAKE_ERROR(Error, "Failed to save manifest for asset '{}': {}", m_name, saveManifestResult.GetError().GetMessage());
        }

        manifestWriter.Close();
    }

    HYP_LOG(Assets, Verbose, "Saved asset manifest to '{}'", manifestPath);

    return {};
}

Result AssetObject::SaveManifest(ByteWriter& stream) const
{
    JSON::Object manifestJson;

    ToJSONOptions opts;
    opts.skipTransientProperties = true;
    opts.writeClassNames = true;

    ObjectToJSON(InstanceClass(), BoxedValue(HandleFromThis()), manifestJson, &opts);

    stream.WriteString(JSON::Value(std::move(manifestJson)).ToString(true).ToUtf8());

    return {};
}

Result AssetObject::SaveBlobData(BlobStorage* storage, const Optional<FilePath>& localBlobDirectory)
{
    Array<Tuple<const char*, uint16, BlobDataReference*>> blobDataReferences;
    CollectBlobDataReferences(blobDataReferences);

    for (auto& tup : blobDataReferences)
    {
        const char* magic = tup.GetElement<0>();
        uint16 version = tup.GetElement<1>();
        BlobDataReference* reference = tup.GetElement<2>();

        AssertDebug(reference != nullptr);

        // Not loaded
        if (!reference->raw)
        {
            continue;
        }

        const size_t magicLen = magic ? std::strlen(magic) : 0;

        AssertDebug(magicLen <= sizeof(BlobHeader::magic) && magicLen != 0,
                    "Blob data reference magic must be non-empty and at most {} characters long",
                    sizeof(BlobHeader::magic));

        BlobHeader header {};
        Memory::Copy((char*)header.magic, magic, MathUtil::Min(magicLen, sizeof(header.magic)));
        header.payloadOffset = 0;
        header.payloadSize = reference->size;
        header.version = version;

        // generate new key
        reference->key = CreateNameFromDynamicString(GetPath().ToString() + "." + magic);

        if (storage != nullptr)
        {
            if (!storage->PutData(StringHash(reference->key), header, reference->raw))
            {
                AssertDebug(false, "Failed to write blob data reference!");

                return HYP_MAKE_ERROR(Error, "Failed to write blob data reference (magic: {}, version: {})", magic, version);
            }
        }

#if HYP_EDITOR
        if (localBlobDirectory.HasValue())
        {
            // Save the blob data locally as well, as other users may not have the blob data or have mismatched blob data
            // and we need to "import" it via individual blobs upon fail.
            // In cooked builds that data will be excluded
            FileByteWriter stream { *localBlobDirectory / (String(*GetName()) + "." + magic + ".raw.blob") };
            if (!stream.IsOpen())
            {
                return HYP_MAKE_ERROR(Error, "Failed to write local blob data at path: {}", stream.GetFilePath());
            }

            stream.Write(reference->raw, reference->size);
        }
#endif
    }

    return {};
}

Result AssetObject::LoadDesc(
    JSON::Object& manifestData,
    AssetDesc& outAssetDesc)
{
    if (!manifestData["Name"].IsString() || !manifestData["$Class"].IsString())
    {
        return HYP_MAKE_ERROR(Error, "Manifest must have 'Name', '$Class' values to be considered valid!");
    }

    outAssetDesc = {};
    outAssetDesc.name = CreateNameFromDynamicString(*manifestData["Name"].ToString());
    outAssetDesc.index = AssetDesc::InvalidIndex;

    return {};
}

Result AssetObject::Load(
    JSON::Object& manifestData,
    Handle<AssetObject>& outAssetObject)
{
    static constexpr uint32 MaxRecursionDepth = 32;
    thread_local uint32 t_recursionDepth = 0;

    HYP_DEFER({ --t_recursionDepth; });

    if (++t_recursionDepth >= MaxRecursionDepth)
    {
        return HYP_MAKE_ERROR(Error, "Recursion depth limit reached. Is the asset self-referential causing a circular dependency?");
    }

    JSON::Value classNameValue = manifestData["$Class"];

    if (!classNameValue.IsString())
    {
        return HYP_MAKE_ERROR(Error, "Manifest JSON must contain a '$Class' string");
    }

    const Class* cls = GetClass(classNameValue.AsString().ToUtf8());

    if (!cls)
    {
        return HYP_MAKE_ERROR(Error, "Class '{}' not found!", classNameValue.AsString());
    }

    if (!cls->IsDerivedFrom(AssetObject::StaticClass()))
    {
        return HYP_MAKE_ERROR(Error, "Class '{}' is not derived from AssetObject!", classNameValue.AsString());
    }

    BoxedValue targetData;
    if (!cls->CreateInstance(targetData))
    {
        return HYP_MAKE_ERROR(Error, "Failed to create instance of class '{}'", classNameValue.AsString());
    }

    AssetObject* targetAssetObject = &targetData.Get<AssetObject>();
    Assert(targetAssetObject != nullptr);

    // remove class property
    manifestData.Erase("$Class");

    {
        GlobalContextScope loadingContextScope { AssetLoadingContext {} };

        if (!ObjectFromJSON(manifestData, cls, targetData))
        {
            return HYP_MAKE_ERROR(Error, "Failed to deserialize asset object from manifest JSON");
        }
    }

    outAssetObject = MakeStrongRef(targetAssetObject);

    return {};
}

void AssetObject::AllocateBlobData(BlobDataReference& reference, const void* inData, size_t count, size_t alignment)
{
    Assert(reference.raw == nullptr || reference.readOnly);

    reference = BlobDataReference {};

    if (count != 0)
    {
        if (alignment < alignof(std::max_align_t))
            alignment = alignof(std::max_align_t);

        const size_t countAligned = ByteUtil::AlignAs(count, alignment);

        // Allocate memory from the asset pool
        reference.raw = g_assetPool->Allocate(countAligned, alignment);
        Assert(reference.raw != nullptr);

        if (inData != nullptr)
        {
            Memory::Copy(reference.raw, inData, count);
        }

        reference.size = count;
        reference.readOnly = false;
    }
}

void AssetObject::FreeBlobData(BlobDataReference& reference)
{
    if (reference.raw == nullptr || reference.readOnly)
    {
        return;
    }

    g_assetPool->Free(reference.raw);
    reference.raw = nullptr;
}

void AssetObject::SetBlobDataResident(bool resident)
{
    Array<Tuple<const char*, uint16, BlobDataReference*>> tuples;
    CollectBlobDataReferences(tuples);

    for (auto& tup : tuples)
    {
        const char* magic = tup.GetElement<0>();
        [[maybe_unused]] uint16 version = tup.GetElement<1>();
        BlobDataReference* reference = tup.GetElement<2>();

        Assert(reference != nullptr);

        SetBlobDataResident(resident, *reference);
    }
}

void AssetObject::SetBlobDataResident(bool resident, BlobDataReference& reference)
{
    if (resident)
    {
        if (reference.readOnly)
        {
            Assert(reference.raw != nullptr);

            AllocateBlobData(reference, reference.raw, reference.size, 16);
        }
    }
    else
    {
        if (!reference.readOnly && reference.raw != nullptr && reference.key.IsValid())
        {
            FreeBlobData(reference);
        }
    }
}

TUniqueResLock<AssetObject> AssetObject::GetWriteScope() const
{
    return TUniqueResLock<AssetObject> { const_cast<AssetObject&>(*this) };
}

TSharedResLock<AssetObject> AssetObject::GetReadScope() const
{
    return TSharedResLock<AssetObject> { const_cast<AssetObject&>(*this) };
}

void AssetObject::LockWriter(bool doInitialize)
{
    uint32 numSpins = 0;

    int64 expected = 0;
    while (!AtomicCompareExchange(&m_rwState, expected, 1))
    {
        expected = 0;

        // volatile read
        while (m_rwState != 0)
        {
            if (numSpins++ < 16)
            {
                HYP_WAIT_IDLE();
            }
            else
            {
                // yield to other threads
                ThreadSleep(0);
            }
        }
    }
}

void AssetObject::UnlockWriter(bool doDeinitialize)
{
    AtomicBitAnd(&m_rwState, ~0x1);
}

void AssetObject::LockReader()
{
    uint32 numSpins = 0;

    union
    {
        int64 state;
        uint64 ustate;
    };

    auto MaybeInitialize = [this](int64 state)
    {
        bool blobLoaded = false;

        if (state == 0)
        {
            // successfully acquired read lock
            Mutex::Guard initGuard(m_initMutex);

            blobLoaded = m_isBlobLoaded;

            if (!blobLoaded)
            {
                // need to do initialize here, since we're the first reader
                m_isBlobLoaded = true;
                blobLoaded = true;

                PageBlobData();

                if (m_flags[AssetObjectFlags::Persistent])
                {
                    SetBlobDataResident(true);
                }

                m_initCV.NotifyAll();
            }
        }

        if (!blobLoaded)
        {
            // successfully acquired read lock
            Mutex::Guard initGuard(m_initMutex);

            // wait for initialization to complete
            while (!m_isBlobLoaded)
            {
                m_initCV.Wait(m_initMutex);
            }
        }
    };

    // first pass: optimistic read
    if ((m_rwState & 0x1) == 0)
    {
        state = AtomicAdd(&m_rwState, 2);

        if ((state & 0x1) == 0)
        {
            MaybeInitialize(state);

            return;
        }

        AtomicSub(&m_rwState, 2);
    }

    while (true)
    {
        // failed, wait for writer to release
        if (m_rwState & 0x1)
        {
            if (numSpins++ < 16)
            {
                HYP_WAIT_IDLE();
            }
            else
            {
                ThreadSleep(0);
            }

            continue;
        }

        state = AtomicAdd(&m_rwState, 2);

        if ((state & 0x1) == 0)
        {
            MaybeInitialize(state);

            return;
        }

        AtomicSub(&m_rwState, 2);
    }
}

void AssetObject::UnlockReader()
{
    Mutex::Guard initGuard(m_initMutex);

    if (AtomicSub(&m_rwState, 2) == 2 && m_isBlobLoaded)
    {
        if (!m_flags[AssetObjectFlags::Persistent])
        {
            SetBlobDataResident(false);
            UnpageBlobData();

            m_isBlobLoaded = false;
        }
    }
}

void AssetObject::GetNumUsers(int64& outReaders, int64& outWriters) const
{
    int64 state = AtomicAdd(const_cast<volatile int64*>(&m_rwState), 0);

    outReaders = state >> 1;
    outWriters = state & 0x1;
}

Handle<AssetRegistry> AssetObject::GetAssetRegistry()
{
    Assert(IsRegistered());

    switch (m_assetPath.registryId)
    {
    case AssetRegistryId::Game:
        return GetCurrentAssetRegistry();
    case AssetRegistryId::Engine:
        return GetEngineAssetRegistry();
#if HYP_EDITOR
    case AssetRegistryId::Editor:
        return GetEditorAssetRegistry();
#endif // HYP_EDITOR
    }

    return Handle<AssetRegistry>::Null();
}

#pragma endregion AssetObject

} // namespace Hyperion
