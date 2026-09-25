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

#include <Core/IO/ByteReader.hpp>
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

    ANSIString str = *baseName;

    int counter = 0;
    while (elements.FindAs(StringHash(*str)) != elements.End())
    {
        counter++;

        str = HYP_FORMAT("{}{}", *baseName, counter);
    }

    if (counter > 0)
    {
        return Name(str);
    }

    return baseName;
}

#pragma region AssetObject

#ifdef HYP_ASSET_OBJECT_THREAD_SAFE
static constexpr uint32 BlobStateInit = 0x1;
static constexpr uint32 BlobStateLoaded = 0x2;
static constexpr uint32 BlobStateLoadedPersistent = 0x4;
#endif // HYP_ASSET_OBJECT_THREAD_SAFE

AssetObject::AssetObject()
    : m_flags(AssetObjectFlags::None),
      m_assetIndex(AssetDesc::InvalidIndex),
#ifdef HYP_ASSET_OBJECT_THREAD_SAFE
      m_rwState(0),
      m_blobState(0)
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
      m_blobState(0)
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

void AssetObject::SetUUID(const UUID& uuid)
{
    if (uuid == m_uuid)
    {
        return;
    }

    m_uuid = uuid;

    MarkDirty();
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

bool AssetObject::IsDirty() const
{
    if (IsTransient() || !IsRegistered())
    {
        return false;
    }
    
    if (IsGlobalContextActive<AssetLoadingContext>())
    {
        return false;
    }
    
    Handle<AssetRegistry> registry = GetAssetRegistry();
    AssertDebug(registry.IsValid());

    if (!registry.IsValid())
    {
        return false;
    }

    return registry->IsAssetDirty(*this);
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

    Result saveBlobDataResult = PersistBlobData(nullptr, dir);
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

Result AssetObject::ExportFiles(const FilePath& directory) const
{
    auto readScope = GetReadScope();

    if (!directory.MkDir())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create directory '{}' to export asset '{}' into", directory, m_name);
    }

    Array<Tuple<const char*, uint16, BlobDataReference*>> blobDataReferences;
    const_cast<AssetObject*>(this)->CollectBlobDataReferences(blobDataReferences);

    for (auto& tup : blobDataReferences)
    {
        const char* magic = tup.GetElement<0>();
        const BlobDataReference* reference = tup.GetElement<2>();

        if (!reference || reference->size == 0)
        {
            continue;
        }

        if (!reference->raw)
        {
            HYP_LOG(Assets, Warning, "Blob data '{}' of asset '{}' could not be paged in, the exported copy will be missing it", magic, m_name);

            continue;
        }

        FileByteWriter blobWriter { directory / (String(*m_name) + "." + magic + ".raw.blob") };

        if (!blobWriter.IsOpen())
        {
            return HYP_MAKE_ERROR(Error, "Failed to write blob data '{}' of asset '{}' to '{}'", magic, m_name, blobWriter.GetFilePath());
        }

        blobWriter.Write(reference->raw, reference->size);
        blobWriter.Close();
    }

    FileByteWriter manifestWriter { directory / (String(*m_name) + ".hmf") };

    if (!manifestWriter.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open manifest file for asset '{}', errno: {}", m_name, std::strerror(errno));
    }

    if (Result saveManifestResult = SaveManifest(manifestWriter); saveManifestResult.HasError())
    {
        return HYP_MAKE_ERROR(Error, "Failed to export manifest for asset '{}': {}", m_name, saveManifestResult.GetError().GetMessage());
    }

    manifestWriter.Close();

    return {};
}

Handle<AssetObject> AssetObject::CloneAsset() const
{
    Array<Tuple<const char*, uint16, BlobDataReference*>> blobReferences;
    const_cast<AssetObject*>(this)->CollectBlobDataReferences(blobReferences);

    if (blobReferences.Any())
    {
        HYP_LOG(Assets, Warning, "no generic implementation for type '{}' because it owns blob data; that type must override CloneAsset()",
                InstanceClass()->GetName());

        return {};
    }

    BoxedValue src(HandleFromThis());
    BoxedValue dst;

    if (!CloneWithoutTransientMembers(src, dst))
    {
        return {};
    }

    Handle<AssetObject> clone = dst.Get<Handle<AssetObject>>();
    clone->SetUUID(UUID());

    return clone;
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

Result AssetObject::PersistBlobData(
    BlobStorage* blobStorage,
    const Optional<FilePath>& localBlobDirectory)
{
    if (IsTransient() || !IsRegistered())
    {
        return {};
    }

    Array<Tuple<const char*, uint16, BlobDataReference*>> blobDataReferences;
    CollectBlobDataReferences(blobDataReferences);

    for (auto& tup : blobDataReferences)
    {
        const char* magic = tup.GetElement<0>();
        const uint16 version = tup.GetElement<1>();

        BlobDataReference* reference = tup.GetElement<2>();
        Assert(reference != nullptr, "Blob data reference is null");

        if (!reference)
        {
            continue;
        }

        Result res = PersistBlobData(
            magic,
            version,
            *reference,
            blobStorage,
            localBlobDirectory);

        if (res.HasError())
        {
            return res.GetError();
        }
    }

    return {};
}

Result AssetObject::PersistBlobData(
    const char* magic,
    uint16 version,
    BlobDataReference& reference,
    BlobStorage* blobStorage,
    const Optional<FilePath>& localBlobDirectory)
{
    if (IsTransient() || !IsRegistered())
    {
        return {};
    }

    Handle<AssetRegistry> registry = GetAssetRegistry();
    AssertDebug(registry.IsValid());

    if (!registry.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "No active asset registry for path: {}", m_assetPath.ToString());
    }

    const AssetBucket& bucket = m_assetPath.GetBucket();

    if (bucket == AssetBuckets::None)
    {
        return HYP_MAKE_ERROR(Error, "Asset '{}' does not have a valid bucket, cannot persist blob data", m_name);
    }

    const FilePath bucketDir = registry->GetRootPath() / bucket.GetName();

    if (!bucketDir.Exists() && !bucketDir.MkDir())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create bucket directory '{}' to persist blob data into", bucketDir);
    }

    // Not loaded
    if (!reference.raw)
    {
        return {};
    }

    const size_t magicLen = magic ? std::strlen(magic) : 0;

    AssertDebug(magicLen <= sizeof(BlobHeader::magic) && magicLen != 0,
                "Blob data reference magic must be non-empty and at most {} characters long",
                sizeof(BlobHeader::magic));

    BlobHeader header {};
    Memory::Copy((char*)header.magic, magic, MathUtil::Min(magicLen, sizeof(header.magic)));
    header.payloadOffset = 0;
    header.payloadSize = reference.size;
    header.version = version;

    // generate new key
    reference.key = Name(GetPath().ToString().ToAnsi() + "." + magic);

    if (blobStorage != nullptr)
    {
        if (!blobStorage->PutData(GetPath().bucketIndex, StringHash(reference.key), header, reference.raw))
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

        stream.Write(reference.raw, reference.size);
    }

    return {}; // ok
}

void AssetObject::AssertBlobDataPersisted(const BlobDataReference& reference) const
{
    // If this fires, we're unpaging data that was never saved to disk, so it will be lost;
    // the next attempt to page it back in will fail (gracefully) and force a re-derive/recompile.
    // Not fatal - the callers unconditionally null the reference right after this, so keeping this
    // non-fatal just means that data has to be regenerated instead of crashing the engine outright.
    if (reference.raw != nullptr && !reference.readOnly && !reference.key.IsValid())
    {
        HYP_LOG(Assets, Error, "Asset '{}' is unpaging blob data that was never persisted to disk, data will be lost", m_name);
    }
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

void AssetObject::UnpageBlobData()
{
#if 0//def HYP_EDITOR
    // Save before unpaging, if the data is dirty.
    // This is editor only (saves the local small blob files)

    if (!EngineGlobals::IsEditor())
    {
        return;
    }

    if (!IsSaved() || !IsDirty())
    {
        return;
    }

    Handle<AssetRegistry> registry = GetAssetRegistry();
    AssertDebug(registry.IsValid());

    if (!registry.IsValid())
    {
        return;
    }

    const FilePath manifestPath = registry->GetManifestPath(m_assetPath);
    const FilePath localBlobDir = manifestPath.BasePath();

    Result res = PersistBlobData(nullptr, localBlobDir);
    if (res.HasError())
    {
        HYP_LOG(Assets, Error, "Failed to persist blob data when unpaging '{}': {}", m_name, res.GetError().GetMessage());
        return;
    }
#endif
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

bool AssetObject::PageBlobDataFromLocalFile(BlobDataReference& reference, const char* magic, size_t alignment)
{
    Handle<AssetRegistry> registry = GetAssetRegistry();
    AssertDebug(registry.IsValid());

    if (!registry.IsValid() || !GetPath().IsValid())
    {
        return false;
    }

    const Name blobKey = reference.key;
    const uint64 expectedSize = reference.size;

    FileByteReader stream { registry->GetRootPath() / GetPath().GetBucket().GetName() / (String(*GetName()) + "." + magic + ".raw.blob") };

    if (stream.Eof())
    {
        HYP_LOG(Assets, Error, "Blob data missing or corrupted for {} ({}) at: {}", GetName(), magic, stream.GetFilepath());

        return false;
    }

    if (stream.Max() != expectedSize)
    {
        HYP_LOG(Assets, Error, "Local blob data for {} ({}) is {} bytes but the manifest expects {}, ignoring it",
            GetName(), magic, stream.Max(), expectedSize);

        return false;
    }

    ByteBuffer buffer = stream.Read(stream.Max());
    AssertDebug(buffer.Size() == stream.Max());

    AllocateBlobData(reference, buffer.Data(), buffer.Size(), alignment);
    reference.key = blobKey;

    return true;
}

void AssetObject::AllocateBlobData(BlobDataReference& reference, const void* inData, size_t count, size_t alignment)
{
    Assert(reference.raw == nullptr || reference.readOnly);

    const Name blobKey = reference.key;

    reference = BlobDataReference {};
    reference.key = blobKey;

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

            if (reference.raw == nullptr)
            {
                // Data failed to page in
                return;
            }

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
            const bool isPersistent = m_flags[AssetObjectFlags::Persistent];

            const uint32 blobState = m_blobState.Get(MemoryOrder::ACQUIRE);

            // a persistent load is never torn down, so it is still loaded if the flag has since been cleared
            if ((blobState & BlobStateLoaded) && (blobState & BlobStateLoadedPersistent))
            {
                if (!isPersistent)
                {
                    // hand the resident copy over to the non-persistent lifecycle, so the last reader tears it down.
                    // take the blob storage reader that teardown releases.
                    if (ShouldUseBlobStorage())
                    {
                        EngineGlobals::GetBlobStorage()->Lock(EngineGlobals::GetCacheDirectory(), /* readOnly */ true);
                    }

                    m_blobState.BitAnd(~BlobStateLoadedPersistent, MemoryOrder::RELEASE);
                }
            }
            else
            {
                // wait for BlobStateLoaded to be cleared.
                // another thread may be tearing down.
                m_blobState.WaitForBitClear(BlobStateLoaded, MemoryOrder::ACQUIRE);

                // We're the initializing thread.
                m_blobState.BitOr(isPersistent ? (BlobStateLoaded | BlobStateLoadedPersistent) : BlobStateLoaded, MemoryOrder::RELEASE);

                // Add reader for blob storage
                if (ShouldUseBlobStorage())
                {
                    EngineGlobals::GetBlobStorage()->Lock(EngineGlobals::GetCacheDirectory(), /* readOnly */ true);
                }

                PageBlobData();

                if (isPersistent)
                {
                    SetBlobDataResident(true);

                    // We don't need the lock anyymore; we have our own copy.
                    if (ShouldUseBlobStorage())
                    {
                        EngineGlobals::GetBlobStorage()->Unlock();
                    }
                }
            }

            m_blobState.BitOr(BlobStateInit, MemoryOrder::RELEASE);
            m_blobState.NotifyAll();
        }
        else
        {
            // Wait for another thread to initialize.
            m_blobState.WaitForBit(BlobStateInit, MemoryOrder::ACQUIRE);
        }
    };

    // CAS instead of add-then-undo, so the reader count never includes readers that will back out
    // (a transient count made a real first reader think it wasn't first and wait on BlobStateInit forever)
    while (true)
    {
        // volatile read
        state = m_rwState;

        // wait for writer (or a last reader tearing down) to release
        if (state & 0x1)
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

        int64 expected = state;

        if (AtomicCompareExchange(&m_rwState, expected, state + 2))
        {
            MaybeInitialize(state);

            return;
        }
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
    int64 state = m_rwState;

    while (true)
    {
        int64 expected = state;

        // last reader swaps its read for the writer bit, so no reader/writer can get in mid-teardown
        const int64 desired = state == 2 ? 1 : state - 2;

        if (AtomicCompareExchange(&m_rwState, expected, desired))
        {
            break;
        }

        state = expected;
    }

    if (state != 2)
    {
        return;
    }

    const uint32 previousBlobState = m_blobState.BitAnd(~BlobStateInit, MemoryOrder::ACQUIRE_RELEASE);

    const bool isTearingDown = (previousBlobState & BlobStateInit)
        && !(previousBlobState & BlobStateLoadedPersistent);

    if (isTearingDown)
    {
        if (IsDirty() || !IsRegistered())
        {
            // Modified blob data only exists in memory, so it has to be kept resident until
            // it has been persisted by SaveDirtyAssets(); otherwise it would be lost.
            if (ShouldUseBlobStorage())
            {
                // We keep the data, but we cannot hold on to storage-mapped (read-only)
                // memory after dropping the blob storage reader lock.
                SetBlobDataResident(true);
            }
        }
        else
        {
            UnpageBlobData();
        }
    }

    UnlockWriter();

    if (isTearingDown)
    {
        // Outside the writer bit; a new first reader waits on BlobStateLoaded for this to finish
        if (ShouldUseBlobStorage())
        {
            EngineGlobals::GetBlobStorage()->Unlock();
        }

        m_blobState.BitAnd(~BlobStateLoaded, MemoryOrder::RELEASE);
        m_blobState.NotifyAll();
    }
#else // !HYP_ASSET_OBJECT_THREAD_SAFE
    if (--m_numReaders == 0)
    {
        if (!m_flags[AssetObjectFlags::Persistent])
        {
            if (IsRegistered() && !IsDirty())
            {
                SetBlobDataResident(false);
                UnpageBlobData();
            }
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

Handle<AssetRegistry> AssetObject::GetAssetRegistry() const
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
