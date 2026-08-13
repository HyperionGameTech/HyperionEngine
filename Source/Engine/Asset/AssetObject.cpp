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

#include <Core/IO/ByteWriter.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <System/MessageBox.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineGlobals.hpp>

#include <AssetObject.generated.inl>

namespace Hyperion {

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
#ifdef HYP_ASSET_OBJECT_THREAD_SAFE
      m_rwState(0),
      m_isInit(false),
      m_isBlobLoaded(false)
#else // !HYP_ASSET_OBJECT_THREAD_SAFE
      m_numReaders(0)
#endif // HYP_ASSET_OBJECT_THREAD_SAFE
{
}

AssetObject::AssetObject(Name name)
    : m_name(SanitizeName(name)),
      m_flags(AssetObjectFlags::None),
      m_assetIndex(AssetDesc::InvalidIndex),
#ifdef HYP_ASSET_OBJECT_THREAD_SAFE
      m_rwState(0),
      m_isInit(false),
      m_isBlobLoaded(false)
#else // !HYP_ASSET_OBJECT_THREAD_SAFE
      m_numReaders(0)
#endif // HYP_ASSET_OBJECT_THREAD_SAFE
{
}

AssetObject::~AssetObject()
{
#ifdef HYP_ASSET_OBJECT_THREAD_SAFE
    if (!(m_rwState & 0x1)) // check not locked from derived dtor
    {
        // add writer here to wait for all reads to complete and
        // block new readers/writers from acquiring the resource while we're destroying it.
        LockWriter();
    }
#endif // HYP_ASSET_OBJECT_THREAD_SAFE
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
            SetPersistentRequested(isPersistent, /* markDirty */ false);
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

void AssetObject::SetPersistentRequested(bool persistentlyLoaded, bool markDirty)
{
    if (m_flags[AssetObjectFlags::Persistent] != persistentlyLoaded)
    {
        m_flags[AssetObjectFlags::Persistent] = persistentlyLoaded;
        
        MarkDirty();
    }
}

void AssetObject::SetIsTransient(bool isTransient)
{
    if (m_flags[AssetObjectFlags::Transient] == isTransient)
    {
        return;
    }

    m_flags[AssetObjectFlags::Transient] = isTransient;

    if (IsTransient())
    {
        // needs to be kept in memory if transient
        SetPersistentRequested(true, /* markDirty */ false);
    }
    else
    {
        SetPersistentRequested(false, /* markDirty */ false);
    }

    MarkDirty();
}

Result AssetObject::Rename(Name name)
{
    name = SanitizeName(name);

    if (name == m_name)
    {
        return {};
    }

    const Name oldName = m_name;

    m_name = name;

    // Don't sync yet, will happen on save

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

    if (manifestPath.GetExtension() != "hmf")
    {
        return HYP_MAKE_ERROR(Error, "Asset manifest path must have .hmf extension");
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

    BlobStorage* blobStorage = EngineGlobals::GetBlobStorage();

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
    String text;

    ToHMFOptions opts;
    opts.skipTransientProperties = true;
    opts.writeClassName = true;

    ObjectToHMF(InstanceClass(), BoxedValue(HandleFromThis()), text, &opts);

    stream.WriteString(text.ToUtf8());

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
            if (!storage->PutData(GetPath().bucketIndex, StringHash(reference->key), header, reference->raw))
            {
                AssertDebug(false, "Failed to write blob data reference!");

                return HYP_MAKE_ERROR(Error, "Failed to write blob data reference (magic: {}, version: {})", magic, version);
            }
        }

        if (!EngineGlobals::IsCooking() && localBlobDirectory.HasValue())
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
    }

    return {};
}

Result AssetObject::Load(
    BoxedValue& manifestData,
    Handle<AssetObject>& outAssetObject)
{
    static constexpr uint32 MaxRecursionDepth = 32;
    thread_local uint32 t_recursionDepth = 0;

    HYP_DEFER({ --t_recursionDepth; });

    if (++t_recursionDepth >= MaxRecursionDepth)
    {
        return HYP_MAKE_ERROR(Error, "Recursion depth limit reached. Is the asset self-referential causing a circular dependency?");
    }

    if (!manifestData.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Manifest data is null/invalid");
    }

    const TypeInfo* typeInfo = manifestData.GetTypeInfo();
    Assert(typeInfo != nullptr);

    const Class* cls = typeInfo->GetClass();

    if (!cls)
    {
        return HYP_MAKE_ERROR(Error, "Manifest data has unknown class");
    }

    if (!cls->IsDerivedFrom(AssetObject::StaticClass()))
    {
        return HYP_MAKE_ERROR(Error, "Class '{}' is not derived from AssetObject!", cls->GetName());
    }

    const Handle<AssetObject>& targetAssetObject = manifestData.Get<Handle<AssetObject>>();
    Assert(targetAssetObject.IsValid());

    outAssetObject = targetAssetObject;

    return {};
}

bool AssetObject::PageBlobDataFromStorage(BlobDataReference& reference)
{
    if (!ShouldUseBlobStorage())
    {
        return false;
    }

    if (!EngineGlobals::GetBlobStorage()->GetData(reference.key, reference.size, reference.raw))
    {
        return false;
    }

    reference.readOnly = true;

    return true;
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

            // AllocateBlobData() resets the reference, so carry the key across the copy -- without it
            // the blob can't be found again, and SetBlobDataResident(false) won't release it either.
            const Name blobKey = reference.key;

            AllocateBlobData(reference, reference.raw, reference.size, 16);

            reference.key = blobKey;
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

HYP_NODISCARD TUniqueResLock<AssetObject> AssetObject::GetWriteScope() const
{
    return TUniqueResLock<AssetObject> { const_cast<AssetObject&>(*this) };
}

HYP_NODISCARD TSharedResLock<AssetObject> AssetObject::GetReadScope() const
{
    return TSharedResLock<AssetObject> { const_cast<AssetObject&>(*this) };
}

void AssetObject::LockWriter()
{
#ifdef HYP_ASSET_OBJECT_THREAD_SAFE
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
#endif // HYP_ASSET_OBJECT_THREAD_SAFE
}

void AssetObject::UnlockWriter()
{
#ifdef HYP_ASSET_OBJECT_THREAD_SAFE
    AtomicBitAnd(&m_rwState, ~0x1);
#endif // HYP_ASSET_OBJECT_THREAD_SAFE
}

void AssetObject::LockReader()
{
#ifdef HYP_ASSET_OBJECT_THREAD_SAFE
    uint32 numSpins = 0;

    union
    {
        int64 state;
        uint64 ustate;
    };

    auto MaybeInitialize = [this](int64 state)
    {
        if (state == 0)
        {
            if (!m_flags[AssetObjectFlags::Persistent] || !m_isBlobLoaded.Get(MemoryOrder::ACQUIRE))
            {
                // Wait for m_isBlobLoaded to be false.
                // Another thread may be tearing down.

                while (m_isBlobLoaded.Get(MemoryOrder::ACQUIRE))
                {
                    m_isBlobLoaded.Wait(true, MemoryOrder::ACQUIRE);
                }

                // We're the initializing thread.
                m_isBlobLoaded.Exchange(true, MemoryOrder::RELEASE);

                // Add reader for blob storage
                if (ShouldUseBlobStorage())
                {
                    EngineGlobals::GetBlobStorage()->Lock(EngineGlobals::GetCacheDirectory(), /* readOnly */ true);
                }

                PageBlobData();

                if (m_flags[AssetObjectFlags::Persistent])
                {
                    SetBlobDataResident(true);
                    
                    // We don't need the lock anyymore; we have our own copy.
                    if (ShouldUseBlobStorage())
                    {
                        EngineGlobals::GetBlobStorage()->Unlock();
                    }
                }
            }

            m_isInit.Set(true, MemoryOrder::RELEASE);
            m_isInit.NotifyAll();
        }
        else
        {
            // Wait for another thread to initialize.

            while (!m_isInit.Get(MemoryOrder::ACQUIRE))
            {
                m_isInit.Wait(false, MemoryOrder::ACQUIRE);
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
#else // !HYP_ASSET_OBJECT_THREAD_SAFE
    if (++m_numReaders == 1)
    {
        PageBlobData();

        if (m_flags[AssetObjectFlags::Persistent])
        {
            SetBlobDataResident(true);
        }
    }
#endif // HYP_ASSET_OBJECT_THREAD_SAFE
}

void AssetObject::UnlockReader()
{
#ifdef HYP_ASSET_OBJECT_THREAD_SAFE
    if (AtomicSub(&m_rwState, 2) == 2)
    {
        bool expected = true;

        if (m_isInit.CompareExchangeStrong(expected, false, MemoryOrder::ACQUIRE_RELEASE))
        {
            if (!m_flags[AssetObjectFlags::Persistent])
            {
                UnpageBlobData();

                // Drop reader for blob storage
                if (ShouldUseBlobStorage())
                {
                    EngineGlobals::GetBlobStorage()->Unlock();
                }

                m_isBlobLoaded.Exchange(false, MemoryOrder::RELEASE);
                m_isBlobLoaded.NotifyAll();
            }
        }
    }
#else !HYP_ASSET_OBJECT_THREAD_SAFE
    if (--m_numReaders == 0)
    {
        if (!m_flags[AssetObjectFlags::Persistent])
        {
            SetBlobDataResident(false);
            UnpageBlobData();
        }
    }
#endif
}

void AssetObject::GetNumUsers(int64& outReaders, int64& outWriters) const
{
#ifdef HYP_ASSET_OBJECT_THREAD_SAFE
    int64 state = AtomicAdd(const_cast<volatile int64*>(&m_rwState), 0);

    outReaders = state >> 1;
    outWriters = state & 0x1;
#else // !HYP_ASSET_OBJECT_THREAD_SAFE
    // We don't have number of writers in this mode
    outReaders = int64(m_numReaders);
    outWriters = 0;
#endif // HYP_ASSET_OBJECT_THREAD_SAFE
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
#ifdef HYP_EDITOR
    case AssetRegistryId::Editor:
        return GetEditorAssetRegistry();
#endif // HYP_EDITOR
    }

    return Handle<AssetRegistry>::Null();
}

bool AssetObject::ShouldUseBlobStorage()
{
    return !EngineGlobals::IsCooking()
        && !EngineGlobals::IsCacheServer()
        && !EngineGlobals::IsEditor();
}

#pragma endregion AssetObject

} // namespace Hyperion
