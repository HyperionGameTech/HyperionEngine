/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <AssetPch.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/AssetBatch.hpp>
#include <Asset/AssetReference.hpp>
#include <Asset/Assets.hpp>
#include <Asset/BlobStorage.hpp>
#include <Asset/BlobStorageViews.hpp>
#include <Asset/SerializationUtils.hpp>

#include <Core/Utilities/DeferredScope.hpp>
#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Reflection/TypeInfo.hpp>
#include <Core/Reflection/Class.hpp>

#include <Core/Threading/Scheduler.hpp>

#include <Core/Reflection/Field.hpp>
#include <Core/Reflection/Property.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>
#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/DataProcessing/Shared/CompilerError.hpp>
#include <Core/DataProcessing/Shared/ErrorList.hpp>

#include <Core/Config/Config.hpp>

#include <Core/Core.hpp>

#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>

#include <Streaming/StreamingCell.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/CacheClient.hpp>

#include <AssetRegistry.generated.inl>

namespace Hyperion {

namespace CoreApi {
CORE_API extern const FilePath& GetExecutablePath();
CORE_API extern const GlobalConfig& GetGlobalConfig();
} // namespace CoreApi

#ifdef HYP_ANDROID
CORE_API extern bool IsAndroidAssetPath(const FilePath& filepath);
#endif // HYP_ANDROID

static Handle<AssetRegistry> s_engineAssetRegistry;

#ifdef HYP_EDITOR
static Handle<AssetRegistry> s_editorAssetRegistry;
#endif // HYP_EDITOR

static Mutex& GetCurrentAssetRegistryMutex()
{
    static Mutex s_mutex;
    return s_mutex;
}

static Array<Handle<AssetRegistry>>& GetAssetRegistryStack()
{
    static Array<Handle<AssetRegistry>> s_stack;
    return s_stack;
}

ENGINE_API Handle<AssetRegistry> GetCurrentAssetRegistry()
{
    // try getting thread-local registry override.
    AssetRegistryContext* ctx = GetGlobalContext<AssetRegistryContext>();

    if (ctx != nullptr && ctx->registry.IsValid())
    {
        return ctx->registry;
    }

    Mutex::Guard guard(GetCurrentAssetRegistryMutex());

    Array<Handle<AssetRegistry>>& stack = GetAssetRegistryStack();

    if (stack.Any())
    {
        return stack.Back();
    }

    return Handle<AssetRegistry>::Null();
}

ENGINE_API void PushAssetRegistry(const Handle<AssetRegistry>& registry)
{
    AssertDebug(registry.IsValid(), "Cannot push a null AssetRegistry!");

    if (!registry.IsValid())
    {
        return;
    }

    Mutex::Guard guard(GetCurrentAssetRegistryMutex());
    GetAssetRegistryStack().PushBack(registry);

    registry->LoadAssetDescs();
}

ENGINE_API void PopAssetRegistry(const AssetRegistry* registry)
{
    Mutex::Guard guard(GetCurrentAssetRegistryMutex());

    // Iterate backwards to find the registry to pop.

    Array<Handle<AssetRegistry>>& stack = GetAssetRegistryStack();

    int index = int(stack.Size()) - 1;

    while (index >= 0 && stack[index] != registry)
    {
        index--;
    }

    if (index >= 0)
    {
        stack.Erase(stack.Begin() + index);
    }
}

ENGINE_API void ClearAssetRegistryStack()
{
    Mutex::Guard guard(GetCurrentAssetRegistryMutex());

    GetAssetRegistryStack().Clear();
}

ENGINE_API Handle<AssetRegistry> GetEngineAssetRegistry()
{
    return s_engineAssetRegistry;
}

ENGINE_API void SetEngineAssetRegistry(const Handle<AssetRegistry>& registry)
{
    if (registry.IsValid())
    {
        registry->LoadAssetDescs();
    }

    s_engineAssetRegistry = registry;
}

#ifdef HYP_EDITOR

ENGINE_API Handle<AssetRegistry> GetEditorAssetRegistry()
{
    return s_editorAssetRegistry;
}

ENGINE_API void SetEditorAssetRegistry(const Handle<AssetRegistry>& registry)
{
    if (registry.IsValid())
    {
        registry->LoadAssetDescs();
    }

    s_editorAssetRegistry = registry;
}

#endif // HYP_EDITOR

static const ThreadId& s_assetRegistryThread = g_simThread;

static constexpr const char* BlobStorageName = "Storage";

// If true, all mutation operations will be forced to run on the sim thread,
// otherwise a mutex will be used to allow multi-threaded access.
static constexpr bool UseSingleThread = false;

StringHash AssetDesc_KeyByFunction(const AssetDesc& assetDesc)
{
    return assetDesc.name;
}

StringHash AssetObject_KeyByFunction(const Handle<AssetObject>& assetObject)
{
    if (!assetObject.IsValid())
    {
        return {};
    }

    return assetObject->GetName();
}

HYP_NODISCARD String SanitizeName(const UTF8StringView& nameStr)
{
    String newString;
    newString.Reserve(nameStr.Size());

    for (auto it = nameStr.Begin(); it != nameStr.End(); ++it)
    {
        const utf::Char32 c = *it;

        if (!std::isalnum(int(c)) && c != '$' && c != '-' && c != '_')
        {
            newString.Append('_');

            continue;
        }

        newString.Append(*it);
    }

    return newString;
}

HYP_NODISCARD Name SanitizeName(Name name)
{
    if (!name.IsValid())
    {
        return Name::Invalid();
    }

    const char* str = name.LookupString();
    AssertDebug(str != nullptr);

    String newString = SanitizeName(UTF8StringView(str));

    if (newString == str)
    {
        // reuse existing name if nothing changed
        return name;
    }

    return CreateNameFromDynamicString(newString);
}

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

HYP_NODISCARD Name CreateFriendlyName(Name name)
{
    if (!name.IsValid())
    {
        return Name::Invalid();
    }

    const char* str = name.LookupString();
    AssertDebug(str != nullptr);

    String friendlyNameStr;

    for (auto it : UTF8StringView(str))
    {
        if (utf::IsAlphabetical(it) || utf::IsDecimal(it))
        {
            friendlyNameStr.Append(it);
        }
    }

    return CreateNameFromDynamicString(StringUtil::ToPascalCase(friendlyNameStr, true));
}

static Result ConstructObjectFromManifest(ByteReader& stream, const FilePath& manifestPath, BoxedValue& outManifestData, HMF::ErrorList* outErrorList)
{
    HMF::ParseResult parseResult = HMF::Parse(manifestPath, stream, outErrorList);

    if (Failed(parseResult))
    {
        return HYP_MAKE_ERROR(Error, "Failed to parse manifest file at {}! Message was: {}",
            manifestPath, parseResult.GetError().GetMessage());
    }

    outManifestData = std::move(parseResult.GetValue());

    return {};
}

#pragma region AssetBucketData

static const AssetBucket& GetBucketForAsset(const AssetObject& assetObject)
{
    const Class* cls = assetObject.InstanceClass();

    const ClassAttributeValue& attr = cls->GetAttributeDeep("assetbucket"_sh);

    if (attr.IsValid() && attr.GetType() == ClassAttributeType::STRING)
    {
        UTF8StringView bucketName = attr.GetString();

        const AssetBucket& bucket = GetAssetBucketByName(StringHash(bucketName));

        return bucket;
    }

    return AssetBuckets::None;
}

class AssetBucketData
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_assetPool);

    AssetRegistryId registryId : 3;
    uint32 bucketIndex : 29;

    AssetDescSet assetDescs;
    AssetObjectCache assetObjectCache;

    TBitset<AssetAllocator> dirtyIndices;
    TBitset<AssetAllocator> usedIndices;

    SharedMutex mtx;

    AssetBucketData()
    {
        registryId = AssetRegistryId::Game;
        bucketIndex = AssetBucket::InvalidIndex;

        // reserve index 0 for invalid
        usedIndices.Set(0, true);
    }

    AssetBucketData(const AssetBucketData& other) = delete;
    AssetBucketData& operator=(const AssetBucketData& other) = delete;

    AssetBucketData(AssetBucketData&& other) noexcept = delete;
    AssetBucketData& operator=(AssetBucketData&& other) noexcept = delete;

    void MarkDirty(uint32 index);

    void SetAsset(AssetDesc& assetDesc, const Handle<AssetObject>& assetObject);

    /*! \brief Get a unique asset name within this bucket by appending an incrementing number to the base name until an unused name is found.
     *   The returned AssetDesc has info about the allocated index/slot + the unique name that was generated.
     *   \param comparator If provided, returning true from the comparator will indicate equality between assets,
     *   meaning that new names will stop being generated from that point on, and the function will return. */
    void AllocateUniqueAssetName(
        ANSIStringView inAssetName,
        AssetDesc& outAssetDesc,
        const ProcRef<bool(const AssetDesc&)>& comparator = nullptr);

    bool GetAssetDesc(StringHash nameHash, AssetDesc& outAssetDesc) const;
};

void AssetBucketData::MarkDirty(uint32 index)
{
    TUniqueLock lock(mtx);
    dirtyIndices.Set(index, true);
}

void AssetBucketData::SetAsset(
    AssetDesc& assetDesc,
    const Handle<AssetObject>& assetObject)
{
    AssertDebug(assetDesc.name.IsValid());

    TUniqueLock lock(mtx);

    auto existingAssetDescIt = assetDescs.Find(assetDesc.name);
    if (existingAssetDescIt != assetDescs.End())
    {
        assetDesc = *existingAssetDescIt;

        AssertDebug(usedIndices.Test(assetObject->m_assetIndex) == true);
    }

    if (assetObject.IsValid()
        && assetObject->m_assetIndex != AssetDesc::InvalidIndex
        && assetDesc.index != assetObject->m_assetIndex)
    {
        // reuse the existing index, if we don't have one.
        if (assetDesc.index == AssetDesc::InvalidIndex)
        {
            assetDesc.index = assetObject->m_assetIndex;
        }
        // otherwise, free the old index and take the desc one.
        else
        {
            // should be set if we are freeing!!
            AssertDebug(usedIndices.Test(assetObject->m_assetIndex) == true);

            usedIndices.Set(assetObject->m_assetIndex, false);

            assetObject->m_assetIndex = assetDesc.index;
        }
    }

    if (assetDesc.index == AssetDesc::InvalidIndex)
    {
        assetDesc.index = usedIndices.FirstZeroBitIndex();
        AssertDebug(assetDesc.index != AssetDesc::InvalidIndex);

        usedIndices.Set(assetDesc.index, true);
    }

    auto it = assetDescs.Emplace(assetDesc).first;
    assetDesc = *it; // update ref

    AssertDebug(usedIndices.Test(assetDesc.index) == true);

    // Evict old asset.
    if (const Handle<AssetObject>* pOldAssetObject = assetObjectCache.TryGet(assetDesc.index); pOldAssetObject && pOldAssetObject->IsValid() && (*pOldAssetObject) != assetObject)
    {
        const Handle<AssetObject>& oldAssetObject = *pOldAssetObject;

        oldAssetObject->OnUnloaded();
        oldAssetObject->m_assetIndex = AssetDesc::InvalidIndex;
    }

    assetObject->m_name = assetDesc.name;
    assetObject->m_assetIndex = assetDesc.index;
    assetObject->m_assetPath = AssetPath(registryId, *AssetBuckets::AllBuckets[bucketIndex], assetDesc.name);

    assetObjectCache.Set(assetDesc.index, assetObject);

    // transient assets are not saved, so they don't need to be marked dirty
    if (assetObject->IsTransient())
    {
        dirtyIndices.Set(assetDesc.index, false);
        return;
    }

    dirtyIndices.Set(assetDesc.index, true);
}

void AssetBucketData::AllocateUniqueAssetName(
    ANSIStringView inAssetName,
    AssetDesc& outAssetDesc,
    const ProcRef<bool(const AssetDesc&)>& comparator)
{
    AssertDebug(inAssetName.Length() > 0);

    TUniqueLock lock(mtx);

    StringHash nameHash(inAssetName);

    if (!assetDescs.Contains(nameHash))
    {
        outAssetDesc.name = CreateNameFromDynamicString(inAssetName);
        outAssetDesc.index = usedIndices.FirstZeroBitIndex();

        usedIndices.Set(outAssetDesc.index, true);

        return;
    }

    // Before generating a suffix, check if the existing entry with the original name is the same asset
    {
        auto existingIt = assetDescs.Find(nameHash);
        if (comparator.IsValid() && comparator(*existingIt))
        {
            outAssetDesc = *existingIt;

            AssertDebug(usedIndices.Test(outAssetDesc.index) == true);

            return;
        }
    }

    uint32 suffix = 1;

    while (true)
    {
        ANSIString uniqueName = HYP_FORMAT("{}_{}", inAssetName, suffix++);
        StringHash uniqueNameHash(uniqueName);

        auto existingIt = assetDescs.Find(uniqueNameHash);

        if (existingIt == assetDescs.End())
        {
            outAssetDesc.name = CreateNameFromDynamicString(uniqueName);
            outAssetDesc.index = usedIndices.FirstZeroBitIndex();

            usedIndices.Set(outAssetDesc.index, true);

            return;
        }
        else if (comparator.IsValid() && comparator(*existingIt))
        {
            outAssetDesc = *existingIt;

            AssertDebug(usedIndices.Test(outAssetDesc.index) == true);

            return;
        }
    }
}

bool AssetBucketData::GetAssetDesc(StringHash nameHash, AssetDesc& outAssetDesc) const
{
    TSharedLock lock(mtx);

    auto it = assetDescs.Find(nameHash);
    if (it == assetDescs.End())
    {
        return false;
    }

    outAssetDesc = *it;

    return true;
}

#pragma endregion AssetBucketData

#pragma region AssetRegistry

AssetRegistry::AssetRegistry(AssetRegistryId registryId, const FilePath& rootPath)
    : m_registryId(registryId),
      m_rootPath(rootPath),
      m_isInitialized(false),
      m_isSyncingCache(false),
      m_scheduler(new Scheduler(s_assetRegistryThread)),
      m_pruneTimer { 5.0 }, // every 5 seconds
      m_pruneTaskBatch(nullptr)
{
    m_assetBucketData = (AssetBucketData*)g_assetPool->Allocate(sizeof(AssetBucketData) * MaxAssetBuckets);
    Assert(m_assetBucketData != nullptr);

    for (uint32 bucketIndex = 0; bucketIndex < MaxAssetBuckets; bucketIndex++)
    {
        AssetBucketData& bucketData = *(new (m_assetBucketData + bucketIndex) AssetBucketData);
        bucketData.registryId = registryId;
        bucketData.bucketIndex = bucketIndex;
    }
}

AssetRegistry::~AssetRegistry()
{
    if (m_pruneTaskBatch != nullptr)
    {
        if (!m_pruneTaskBatch->IsCompleted())
        {
            HYP_LOG(Assets, Info, "Waiting for prune task batch to complete before destroying AssetRegistry...");
            m_pruneTaskBatch->AwaitCompletion();
        }

        delete m_pruneTaskBatch;
        m_pruneTaskBatch = nullptr;
    }

    for (uint32 bucketIndex = 0; bucketIndex < MaxAssetBuckets; bucketIndex++)
    {
        m_assetBucketData[bucketIndex].~AssetBucketData();
    }

    g_assetPool->Free(m_assetBucketData);
    m_assetBucketData = nullptr;

    delete m_scheduler;
}

void AssetRegistry::Initialize(Task<Result>* outSyncContentTask)
{
    if (m_isInitialized || m_isSyncingCache)
    {
        return;
    }

    AssertDebug(m_rootPath.Length() > 0);
    
    if (outSyncContentTask != nullptr)
    {
        // Check if we have a cache server to contect to and sync content before initialize.
        if (!EngineGlobals::IsEditor()
            && !EngineGlobals::IsCooking()
            && !EngineGlobals::IsCacheServer())
        {
            if (const auto& cfgCacheServer = CoreApi::GetCommandLineArguments()["CacheServer"]; cfgCacheServer.IsString())
            {
                CacheClient::Params params {};
                params.cacheServer = cfgCacheServer.AsString();
                params.registryId = m_registryId;
                params.outputCacheDir = EngineGlobals::GetCacheDirectory();
                params.outputContentDir = EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Game")>();
                params.numAttempts = 1; // 5;

                m_isSyncingCache = true;
                m_cacheSyncComplete.Reset();

                *outSyncContentTask = TaskSystem::GetInstance().Enqueue(
                    [this, weakThis = MakeWeakRef(this), params]() -> Result
                    {
                        Handle<AssetRegistry> strongThis = weakThis.Lock();
                        if (!strongThis.IsValid())
                        {
                            m_isSyncingCache = false;
                            m_cacheSyncComplete.Reset();

                            return HYP_MAKE_ERROR(Error, "AssetRegistry reference expired before cache could be downloaded.");
                        }

                        if (Result res = CacheClient::SyncContent(params); res.HasError())
                        {
                            m_isSyncingCache = false;
                            m_cacheSyncComplete.Reset();

                            return res;
                        }

                        LoadAssetDescs();

                        m_isInitialized = true;
                        m_isSyncingCache = false;

                        m_cacheSyncComplete.Signal();

                        // OK
                        return {};
                    },
                    TaskThreadPoolName::THREAD_POOL_BACKGROUND);
            }
        }
    }

    if (!m_isSyncingCache)
    {
        m_cacheSyncComplete.Reset();

        LoadAssetDescs();

        m_isInitialized = true;
    }
}

void AssetRegistry::Shutdown()
{
    if (m_isSyncingCache)
    {
        m_cacheSyncComplete.Wait();
    }

    if (!m_isInitialized)
    {
        return;
    }

    // unload all cached assets
    RemoveCached();

    m_isInitialized = false;
}

FilePath AssetRegistry::GetRootPath() const
{
    TSharedLock lock(m_mutex);
    return m_rootPath;
}

void AssetRegistry::SetRootPath(const FilePath& rootPath)
{
    TUniqueLock lock(m_mutex);

    if (rootPath == m_rootPath)
    {
        return;
    }

    // Mark all assets dirty so they get saved to the new location
    for (uint32 bucketIndex = 0; bucketIndex < MaxAssetBuckets; bucketIndex++)
    {
        AssetBucketData& bucketData = m_assetBucketData[bucketIndex];

        TUniqueLock bucketLock(bucketData.mtx);

        for (const AssetDesc& assetDesc : bucketData.assetDescs)
        {
            // Load the asset object if it's not already loaded, so that it gets marked dirty and saved to the new location. This is needed in case the asset was modified but not yet saved, otherwise those changes would be lost.
            if (!bucketData.assetObjectCache.HasIndex(assetDesc.index))
            {
                bucketLock.Reset();

                Handle<AssetObject> assetObject = GetAsset(*AssetBuckets::AllBuckets[bucketIndex], assetDesc.name);
                (void)assetObject; // silence unused variable warning

                bucketLock.Reset(bucketData.mtx);
            }

            bucketData.dirtyIndices.Set(assetDesc.index, true);
        }
    }

    // @TODO - Move blob storage data?

    m_rootPath = rootPath;
}

Handle<AssetObject> AssetRegistry::GetAsset(const AssetBucket& bucket, StringHash name)
{
    AssetBucketData& data = m_assetBucketData[bucket.GetIndex()];

    TSharedLock lock(data.mtx);

    auto it = data.assetDescs.Find(name);
    if (it == data.assetDescs.End())
    {
        return Handle<AssetObject>::Null();
    }

    const uint32 index = it->index;
    AssertDebug(data.usedIndices.Test(index) == true);

    const Handle<AssetObject>* pAssetObject = data.assetObjectCache.TryGet(index);

    if (pAssetObject != nullptr)
    {
        return *pAssetObject;
    }

    lock.Reset();

    // Load it into cache
    Handle<AssetObject> assetObject;

    const AssetPath assetPath { m_registryId, bucket, Name(name) };

    const FilePath manifestPath = GetManifestPath(assetPath);
    FileByteReader stream { manifestPath };

    BoxedValue objectBoxed;
    HMF::ErrorList errorList;

    if (Result readManifestResult = ConstructObjectFromManifest(stream, manifestPath, objectBoxed, &errorList); readManifestResult.HasError())
    {
        HYP_LOG(Assets, Warning, "Failed to read asset manifest: {}", readManifestResult.GetError().GetMessage());

        return Handle<AssetObject>::Null();
    }

    Result loadResult = AssetObject::Load(objectBoxed, assetObject);

    if (loadResult.HasError())
    {
        HYP_LOG(Assets, Warning, "Failed to load asset: {}", loadResult.GetError().GetMessage());

        return Handle<AssetObject>::Null();
    }

    assetObject->m_assetIndex = index;
    assetObject->m_assetPath = assetPath;

    { // set the asset in cache
        TUniqueLock packageLock(data.mtx);

        // check again in case another thread added it; in that case we'll reuse the existing asset and discard ours
        pAssetObject = data.assetObjectCache.TryGet(index);

        if (pAssetObject != nullptr)
        {
            // revert
            assetObject->m_assetIndex = AssetDesc::InvalidIndex;
            assetObject->m_assetPath = AssetPath();

            assetObject.Reset();

            return *pAssetObject;
        }

        data.assetObjectCache.Set(index, assetObject);
    }

    InitObject(assetObject);
    assetObject->OnLoaded();

    AssertDebug(assetObject->m_assetIndex != AssetDesc::InvalidIndex);

    return assetObject;
}

void AssetRegistry::MarkAssetDirty(const AssetObject& assetObject)
{
    if (assetObject.IsTransient())
    {
        return;
    }

    const AssetPath& assetPath = assetObject.GetPath();

    if (!assetPath.IsValid())
    {
        HYP_LOG(Assets, Warning, "Attempted to mark asset '{}' dirty, but it does not have a valid asset path",
                assetObject.GetName());

        return;
    }

    const AssetBucket& bucket = assetPath.GetBucket();

    if (bucket == AssetBuckets::None)
    {
        HYP_LOG(Assets, Warning, "Attempted to mark asset '{}' dirty, but it does not have a valid bucket",
                assetObject.GetName());

        return;
    }

    AssetBucketData& data = m_assetBucketData[bucket.GetIndex()];

    data.MarkDirty(assetObject.m_assetIndex);
}

uint32 AssetRegistry::GetBucketAssetDescs(uint32 bucketIndex, Array<AssetDesc>& outDescs) const
{
    if (bucketIndex >= MaxAssetBuckets)
    {
        return 0;
    }

    const AssetBucketData& data = m_assetBucketData[bucketIndex];

    TSharedLock lock(data.mtx);

    for (const AssetDesc& desc : data.assetDescs)
    {
        outDescs.PushBack(desc);
    }

    return uint32(outDescs.Size());
}

void AssetRegistry::PutAsset(const Handle<AssetObject>& assetObject)
{
    if (!assetObject.IsValid())
    {
        return;
    }

    const AssetBucket& bucket = GetBucketForAsset(*assetObject);
    AssertDebug(bucket != AssetBuckets::None);

    if (bucket == AssetBuckets::None)
    {
        HYP_LOG(Assets, Warning, "Asset '{}' does not have a valid bucket, cannot be put in registry", assetObject->GetName());
        return;
    }

    PutAsset(bucket, assetObject);
}

void AssetRegistry::PutAsset(const AssetBucket& bucket, const Handle<AssetObject>& assetObject)
{
    if (!assetObject.IsValid())
    {
        return;
    }

    AssetBucketData& data = m_assetBucketData[bucket.GetIndex()];

    AssetDesc assetDesc;
    assetDesc.name = assetObject->m_name;
    assetDesc.index = AssetDesc::InvalidIndex;

    if (!assetDesc.name.IsValid())
    {
        // no name; create one using the instance class's name. (e.g Mesh123)

        auto UniquePredicate = [&assetObject, registryId = data.registryId, bucketIndex = data.bucketIndex](const AssetDesc& otherDesc) -> bool
        {
            if (assetObject->m_assetIndex == otherDesc.index)
            {
                return true;
            }

            // When index is invalid (e.g. after RemoveCached), identify by asset path
            if (assetObject->m_assetIndex == AssetDesc::InvalidIndex)
            {
                const AssetPath& assetPath = assetObject->GetPath();

                return assetPath.IsValid()
                    && assetPath.registryId == registryId
                    && assetPath.bucketIndex == bucketIndex
                    && assetPath.assetName == otherDesc.name;
            }

            return false;
        };

        data.AllocateUniqueAssetName(
            *assetObject->InstanceClass()->GetName(),
            assetDesc,
            UniquePredicate);
    }

    data.SetAsset(assetDesc, assetObject);
}

void AssetRegistry::PutAssetUnique(const Handle<AssetObject>& assetObject)
{
    if (!assetObject.IsValid())
    {
        return;
    }

    const AssetBucket& bucket = GetBucketForAsset(*assetObject);
    AssertDebug(bucket != AssetBuckets::None);

    if (bucket == AssetBuckets::None)
    {
        HYP_LOG(Assets, Warning, "Asset '{}' does not have a valid bucket, cannot be put in registry", assetObject->GetName());
        return;
    }

    PutAssetUnique(bucket, assetObject);
}

void AssetRegistry::PutAssetUnique(const AssetBucket& bucket, const Handle<AssetObject>& assetObject)
{
    if (!assetObject.IsValid())
    {
        return;
    }

    AssetBucketData& data = m_assetBucketData[bucket.GetIndex()];

    AssetDesc assetDesc;
    assetDesc.name = assetObject->m_name;
    assetDesc.index = AssetDesc::InvalidIndex;

    if (!assetDesc.name.IsValid())
    {
        assetDesc.name = assetObject->InstanceClass()->GetName();
    }

    auto UniquePredicate = [&assetObject, registryId = data.registryId, bucketIndex = data.bucketIndex](const AssetDesc& otherDesc) -> bool
    {
        if (assetObject->m_assetIndex == otherDesc.index)
        {
            return true;
        }

        // When index is invalid (e.g. after RemoveCached), identify by asset path
        if (assetObject->m_assetIndex == AssetDesc::InvalidIndex)
        {
            const AssetPath& assetPath = assetObject->GetPath();

            return assetPath.IsValid()
                && assetPath.registryId == registryId
                && assetPath.bucketIndex == bucketIndex
                && assetPath.assetName == otherDesc.name;
        }

        return false;
    };

    data.AllocateUniqueAssetName(
        *assetDesc.name,
        assetDesc,
        UniquePredicate);

    assetObject->m_name = assetDesc.name;

    data.SetAsset(assetDesc, assetObject);
}

void AssetRegistry::WalkAssetDeep(const BoxedValue& target, const ProcRef<void(const Handle<AssetObject>&)>& onAssetFound)
{
    Set<const ObjectBase*> visited; // to avoid infinite recursion

    bool shouldFollowAssetPaths = false;

    ProcRef<void(const BoxedValue&)> iterate;
    auto lambda = [&](const BoxedValue& current) -> void
    {
        if (!current.IsValid() || current.IsNull())
        {
            return;
        }

        {
            ObjectBase* object = current.TryGet<ObjectBase*>().GetOr(nullptr);
            if (object && !visited.Insert(object).second)
            {
                HYP_LOG(Assets, Verbose, "Already visited {} with ID {}, skipping to avoid infinite recursion",
                        object->InstanceClass() ? *object->InstanceClass()->GetName() : "<no class>", object->Id());

                return;
            }
        }

        Handle<AssetObject> assetObject;

        Optional<AssetReference> tmpAssetReference;
        const AssetReference* assetReference = nullptr;

        if (current.Is<AssetObject>())
        {
            assetObject = MakeStrongRef(&current.Get<AssetObject>());
            Assert(assetObject != nullptr);

            assetReference = &tmpAssetReference.Emplace(assetObject);
        }
        else if (current.Is<AssetPath>() && shouldFollowAssetPaths)
        {
            assetReference = &tmpAssetReference.Emplace(current.Get<AssetPath>());
        }
        else if (current.Is<AssetReference>())
        {
            assetReference = &current.Get<AssetReference>();
        }

        if (assetReference && !assetReference->IsValid())
        {
            assetReference = nullptr;
        }

        if (assetReference && !assetObject)
        {
            assetObject = assetReference->Resolve();

            if (!assetObject)
            {
                HYP_LOG(Assets, Warning, "AssetReference {} failed to resolve!", assetReference->GetAssetPath().ToString());
            }
        }

        shouldFollowAssetPaths = false;

        bool walked = false;

        auto functor = [&](const BoxedValue& value)
        {
            iterate(value);

            walked = true;
        };

        WalkBoxedValue(current, functor);

        if (walked)
        {
            return;
        }

        // Special handling for Entity: needs to collect from components
        if (current.Is<Entity>())
        {
            const Entity& entity = current.Get<Entity>();

            EntityManager* entityManager = entity.GetEntityManager();
            if (entityManager != nullptr)
            {
                const auto componentIds = entityManager->GetAllComponents(&entity);
                if (componentIds.HasValue())
                {
                    for (const auto& [typeId, componentId] : *componentIds)
                    {
                        const AnyRef componentRef = entityManager->TryGetComponent(typeId, &entity);
                        if (!componentRef.HasValue())
                        {
                            continue;
                        }

                        iterate(BoxedValue(componentRef));
                    }
                }
            }
            else
            {
                HYP_LOG(Assets, Warning, "Entity {} has no valid EntityManager, cannot iterate components", entity.Id());
            }
        }

        // if (current.Is<StreamingCell>())
        // {
        //     const StreamingCell& streamingCell = current.Get<StreamingCell>();

        //     for (const AssetReference& assetReference : streamingCell.GetAssetReferences())
        //     {
        //         if (IsRelocatable(assetReference.GetAssetPath()))
        //         {
        //             HYP_LOG(Assets, Error, "StreamingCell contains a reference to the asset: {}, which is in an in-memory package or the $Temp package on the filesystem.\n"
        //                 "This may result in issues with loading the asset later down the line.",
        //                 assetReference.GetAssetPath().ToString());
        //         }
        //     }
        // }

        const Class* cls = GetClass(current.GetTypeId());

        const BoxedValue* boxed = &current;
        BoxedValue tmpBoxed;

        if (assetObject != nullptr)
        {
            tmpBoxed = BoxedValue(assetObject);
            boxed = &tmpBoxed;

            cls = assetObject->InstanceClass();
        }

        if (!cls) // no Class; not an object we can iterate over.
        {
            return;
        }

        for (const IMember& member : cls->GetMembers(MemberType::Property | MemberType::Field, /* deep */ true))
        {
            if (member.IsDelegate())
            {
                continue;
            }

            if (member.GetMemberType() != MemberType::Property && member.GetAttribute(Attributes::g_attrProperty).IsValid())
            {
                // skip non-property members if they have the 'property' attribute (synthetic property)
                continue;
            }

            if (member.GetAttribute(Attributes::g_attrTransient).GetBool() || !member.GetAttribute(Attributes::g_attrSerialize).GetBool(true))
            {
                continue;
            }

            BoxedValue memberData;
            switch (member.GetMemberType())
            {
            case MemberType::Property:
            {
                const Property* property = static_cast<const Property*>(&member);
                memberData = property->Get(*boxed);
                break;
            }
            case MemberType::Field:
            {
                const Field* field = static_cast<const Field*>(&member);
                memberData = field->Get(*boxed);
                break;
            }
            default:
                HYP_UNREACHABLE();
                break;
            }

            if (!memberData.IsValid() || memberData.IsNull())
            {
                continue;
            }

            const Class* memberClass = memberData.GetTypeInfo()->GetClass();
            if (memberClass != nullptr && !memberClass->GetAttribute(Attributes::g_attrSerialize).GetBool(true))
            {
                // skip members with Serialize=false on class
                continue;
            }

            shouldFollowAssetPaths = member.GetAttribute(Attributes::g_attrFollowAssetPath).GetBool();

            iterate(memberData);
        }

        if (assetObject)
        {
            onAssetFound(assetObject);
        }
    };

    iterate = lambda;
    iterate(target);
}

void AssetRegistry::PutAssetsDeep(const Handle<AssetObject>& targetAsset, bool overwriteExisting)
{
    if (!targetAsset.IsValid())
    {
        return;
    }

    GlobalContextScope contextScope { AssetRegistryContext { MakeStrongRef(this) } };

    auto callback = [this, overwriteExisting](const Handle<AssetObject>& assetObject)
        {
            if (assetObject->m_assetIndex == AssetDesc::InvalidIndex)
            {
                if (overwriteExisting)
                {
                    PutAsset(assetObject);
                }
                else
                {
                    PutAssetUnique(assetObject);
                }
            }

            AssertDebug(assetObject->m_name.IsValid());
        };

    WalkAssetDeep(BoxedValue(targetAsset), callback);
}

void AssetRegistry::RemoveAsset(const Handle<AssetObject>& asset)
{
    if (!asset.IsValid())
    {
        return;
    }

    const AssetBucket& bucket = GetBucketForAsset(*asset);
    AssertDebug(bucket != AssetBuckets::None);

    if (bucket == AssetBuckets::None)
    {
        HYP_LOG(Assets, Warning, "Asset '{}' does not have a valid bucket, cannot be removed from registry", asset->GetName());
        return;
    }

    RemoveAsset(bucket, asset->GetName());
}

void AssetRegistry::RemoveAsset(const AssetBucket& bucket, StringHash name)
{
    AssetBucketData& data = m_assetBucketData[bucket.GetIndex()];

    TUniqueLock lock(data.mtx);

    auto it = data.assetDescs.Find(name);
    if (it == data.assetDescs.End())
    {
        return;
    }

    Handle<AssetObject>* pAssetObject = data.assetObjectCache.TryGet(it->index);

    if (pAssetObject != nullptr && pAssetObject->IsValid())
    {
        Handle<AssetObject>& assetObject = *pAssetObject;

        assetObject->m_assetIndex = AssetDesc::InvalidIndex;
        assetObject->OnUnloaded();
    }

    const uint32 index = it->index;
    AssertDebug(index != AssetDesc::InvalidIndex);

    if (index == AssetDesc::InvalidIndex)
    {
        return;
    }

    data.assetDescs.Erase(it);
    data.usedIndices.Set(index, false);
    data.dirtyIndices.Set(index, false);
    data.assetObjectCache.EraseAt(index);
}

void AssetRegistry::SyncAssetName(const AssetBucket& bucket, Name oldName, Name newName)
{
    AssetBucketData& data = m_assetBucketData[bucket.GetIndex()];

    TUniqueLock lock(data.mtx);

    auto it = data.assetDescs.Find(StringHash(oldName));
    if (it == data.assetDescs.End())
    {
        return;
    }

    const uint32 index = it->index;

    data.assetDescs.Erase(it);

    AssetDesc newDesc;
    newDesc.name = newName;
    newDesc.index = index;
    data.assetDescs.Add(std::move(newDesc));

    // Remove old file, leave new file to be written by SaveDirtyAssets
    const FilePath oldFilePath = GetRootPath() / bucket.GetName() / (oldName.ToString() + ".hmf");

    if (oldFilePath.Exists())
    {
        if (!oldFilePath.Remove())
        {
            HYP_LOG(Assets, Error, "Failed to remove file {}", oldFilePath);

            return;
        }
    }

    HYP_LOG(Assets, Info, "Synced asset name '{}' -> '{}' in bucket '{}'", oldName, newName, bucket.GetName());
}

bool AssetRegistry::LoadAssetDescs()
{
    const FilePath rootPath = GetRootPath();

    HYP_LOG(Assets, Verbose, "Loading asset descs from '{}'", rootPath);

    if (!rootPath.Exists())
    {
        HYP_LOG(Assets, Warning, "AssetRegistry root path does not exist at {}", rootPath);
        return false;
    }
   
    GlobalContextScope loadingContextScope { AssetLoadingContext {} };
    // @TODO Load from index file rather than using file iteration.
    // or simply maintain a bin with all AssetPath stored for each bucket as a flat file.
    for (const AssetBucket* bucket : AssetBuckets::AllBuckets)
    {
        if (bucket == &AssetBuckets::None)
        {
            continue;
        }

        AssetBucketData& data = m_assetBucketData[bucket->GetIndex()];

        Array<FilePath> assetFiles;

        FilePath subdir = rootPath / bucket->GetName();

        for (auto iter = subdir.OpenDirectory(); iter.HasNext(); iter.Advance())
        {
            if (iter.CurrentIsDirectory())
            {
                continue;
            }

            const FilePath curr = iter.Current();

            if (curr.GetExtension() != "hmf")
            {
                continue;
            }

            assetFiles.PushBack(curr);
        }

        Array<AssetDesc> assetDescs;
        assetDescs.Reserve(assetFiles.Size());

        for (const FilePath& entry : assetFiles)
        {
            AssetDesc& assetDesc = assetDescs.EmplaceBack();
            assetDesc.index = AssetDesc::InvalidIndex;
            assetDesc.name = CreateNameFromDynamicString(StringUtil::StripExtension(entry.Basename()).ToAnsi());
            
            HYP_LOG(Assets, Verbose, "Found asset desc '{}' in '{}'", assetDesc.name, entry);
        }

        if (assetDescs.Any())
        {
            TUniqueLock lock(data.mtx);

            for (AssetDesc& assetDesc : assetDescs)
            {
                if (data.assetDescs.Contains(assetDesc.name))
                {
                    HYP_LOG(Assets, Verbose, "Asset '{}' already present in bucket '{}', skipping",
                            assetDesc.name, bucket->GetName());

                    continue;
                }

                AssertDebug(assetDesc.index == AssetDesc::InvalidIndex);

                assetDesc.index = data.usedIndices.FirstZeroBitIndex();
                data.usedIndices.Set(assetDesc.index, true);

                HYP_LOG(Assets, Verbose, "Add asset desc {} to registry {}", assetDesc.name, m_rootPath.Basename());

                data.assetDescs.Add(std::move(assetDesc));
            }
        }
    }

    return true;
}

void AssetRegistry::SaveDirtyAssets()
{
    const FilePath rootPath = GetRootPath();

    if (m_rootPath.Exists())
    {
        if (!m_rootPath.IsDirectory())
        {
            HYP_LOG(Assets, Error, "AssetRegistry root path ({}) exists but is not a directory! Cannot save assets.", m_rootPath);

            return;
        }
    }
    else if (!m_rootPath.MkDir())
    {
        HYP_LOG(Assets, Error, "Failed to create root directory for AssetRegistry: {}! Cannot save assets.", m_rootPath);

        return;
    }

    for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; ++bucketIndex)
    {
        AssetBucketData& data = m_assetBucketData[bucketIndex];

        const char* bucketName = GetAssetBucketName(bucketIndex);
        AssertDebug(bucketName != nullptr);

        Set<Handle<AssetObject>> dirtyAssets;

        {
            TUniqueLock lock(data.mtx);

            if (!data.dirtyIndices.AnyBitsSet())
            {
                continue;
            }

            TBitset<AssetAllocator> seenIndices;

            Bitset::BitIndex index;

            do
            {
                index = data.dirtyIndices.NextSetBitIndex(1);

                if (index == Bitset::NotFound)
                {
                    break;
                }

                if (!seenIndices.Test(index))
                {
                    const Handle<AssetObject>* pAssetObject = data.assetObjectCache.TryGet(index);

                    if (!pAssetObject || !pAssetObject->IsValid() || (*pAssetObject)->IsTransient())
                    {
                        data.dirtyIndices.Set(index, false);

                        seenIndices.Set(index, true);

                        continue;
                    }

                    Handle<AssetObject> assetObject = *pAssetObject;

                    dirtyAssets.Add(assetObject);

                    lock.Reset();

                    // recursively register properties for this asset object
                    PutAssetsDeep(*pAssetObject);

                    // relock
                    lock.Reset(data.mtx);

                    seenIndices.Set(index, true);
                }

                // mark this asset as no longer dirty so we don't keep looping over the same index.
                data.dirtyIndices.Set(index, false);
            }
            while (index != Bitset::NotFound);
        }

        if (dirtyAssets.Empty())
        {
            continue;
        }

        const FilePath bucketDir = rootPath / bucketName;

        if (!bucketDir.Exists())
        {
            if (!bucketDir.MkDir())
            {
                HYP_LOG(Assets, Warning, "Failed to create bucket directory '{}'", bucketDir);
                continue;
            }
        }

        for (AssetObject* assetObject : dirtyAssets)
        {
            // Unique lock so nothing mutates it
            // Asset data should already be in mem if it's dirty.
            //auto writeScope = assetObject->GetWriteScope();

            const Name assetName = assetObject->GetName();
            AssertDebug(assetName.IsValid());

            if (!assetName.IsValid())
            {
                continue;
            }

            const FilePath manifestPath = GetManifestPath(assetObject->GetPath());

            if (Result saveBlobResult = assetObject->SaveBlobData(nullptr, bucketDir); saveBlobResult.HasError())
            {
                HYP_LOG(Assets, Warning, "Failed to save blob data for asset '{}' in bucket '{}': {}",
                        assetName, bucketName, saveBlobResult.GetError().GetMessage());
                continue;
            }

            {
                FileByteWriter manifestWriter { manifestPath };

                if (!manifestWriter.IsOpen())
                {
                    HYP_LOG(Assets, Warning, "Failed to open manifest file '{}' for writing", manifestPath);
                    continue;
                }

                if (Result saveManifestResult = assetObject->SaveManifest(manifestWriter); saveManifestResult.HasError())
                {
                    HYP_LOG(Assets, Warning, "Failed to save manifest for asset '{}' in bucket '{}': {}",
                            assetName, bucketName, saveManifestResult.GetError().GetMessage());
                    continue;
                }

                manifestWriter.Close();

                HYP_LOG(Assets, Verbose, "Saved asset manifest for '{}' to '{}'", assetName, manifestPath);

                // If the content name differs from the file-system name that the
                // AssetDesc was registered under, sync the AssetDesc and clean up
                // the old file so future lookups use the content name.
                {
                    const Name oldFsName = assetObject->m_assetPath.assetName;
                    const Name contentName = assetName;

                    if (oldFsName.IsValid() && oldFsName != contentName)
                    {
                        TUniqueLock syncLock(data.mtx);

                        auto oldDescIt = data.assetDescs.Find(StringHash(oldFsName));

                        if (oldDescIt != data.assetDescs.End() && oldDescIt->name != contentName)
                        {
                            const uint32 savedIndex = oldDescIt->index;
                            const FilePath oldPath = bucketDir / (oldFsName.ToString() + ".hmf");

                            data.assetDescs.Erase(oldDescIt);

                            if (oldPath != manifestPath && oldPath.Exists())
                            {
                                if (!oldPath.Remove())
                                {
                                    HYP_LOG(Assets, Error, "Failed to remove file {}", oldPath);
                                }
                            }

                            AssetDesc newDesc;
                            newDesc.name = contentName;
                            newDesc.index = savedIndex;
                            data.assetDescs.Add(std::move(newDesc));

                            assetObject->m_assetPath = AssetPath(m_registryId, *AssetBuckets::AllBuckets[bucketIndex], contentName);

                            HYP_LOG(Assets, Info, "Synced asset name from '{}' to '{}'", oldFsName, contentName);
                        }
                    }
                }
            }
        }
    }
}

bool AssetRegistry::HasDirtyAssets() const
{
    for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; ++bucketIndex)
    {
        AssetBucketData& data = m_assetBucketData[bucketIndex];

        TSharedLock lock(data.mtx);

        if (data.dirtyIndices.AnyBitsSet())
        {
            return true;
        }
    }

    return false;
}

void AssetRegistry::RemoveCached()
{
    for (uint32 bucketIndex = 0; bucketIndex < MaxAssetBuckets; bucketIndex++)
    {
        AssetBucketData& bucketData = m_assetBucketData[bucketIndex];

        TUniqueLock lock(bucketData.mtx);

        for (AssetDesc& desc : bucketData.assetDescs)
        {
            const Handle<AssetObject>* pAssetObject = bucketData.assetObjectCache.TryGet(desc.index);

            if (pAssetObject != nullptr)
            {
                const Handle<AssetObject>& assetObject = *pAssetObject;

                if (assetObject.IsValid())
                {
                    assetObject->m_assetIndex = AssetDesc::InvalidIndex;
                    assetObject->OnUnloaded();
                }

                bucketData.assetObjectCache.EraseAt(desc.index);
            }
        }
    }
}

void AssetRegistry::RemoveCached(const AssetBucket& bucket)
{
    AssetBucketData& bucketData = m_assetBucketData[bucket.GetIndex()];

    TUniqueLock lock(bucketData.mtx);

    for (AssetDesc& desc : bucketData.assetDescs)
    {
        const Handle<AssetObject>* pAssetObject = bucketData.assetObjectCache.TryGet(desc.index);

        if (pAssetObject != nullptr)
        {
            const Handle<AssetObject>& assetObject = *pAssetObject;

            if (assetObject.IsValid())
            {
                assetObject->m_assetIndex = AssetDesc::InvalidIndex;
                assetObject->OnUnloaded();
            }

            bucketData.assetObjectCache.EraseAt(desc.index);
        }
    }
}

void AssetRegistry::Update()
{
    HYP_SCOPE;
    AssertOnThread(s_assetRegistryThread);

#ifdef HYP_EDITOR
    if (!m_pruneTimer.Waiting())
    {
        m_pruneTimer.NextTick();

        // @TODO
    }
#endif

    if (m_scheduler->NumEnqueued() > 0)
    {
        Queue<Scheduler::ScheduledTask> tasks;
        m_scheduler->AcceptAll(tasks);

        while (tasks.Any())
        {
            Scheduler::ScheduledTask scheduledTask = tasks.Pop();

            scheduledTask.Execute();
        }
    }
}

template <class Func, class FutureType>
void AssetRegistry::PostTask(Func&& fn, Task<FutureType>* pOutFuture)
{
    if (!UseSingleThread || IsOnThread(s_assetRegistryThread))
    {
        if (pOutFuture)
        {
            *pOutFuture = Task<FutureType>();

            if constexpr (std::is_void_v<FutureType>)
            {
                fn();
                pOutFuture->Fulfill();
            }
            else
            {
                pOutFuture->Fulfill(fn());
            }
        }
        else
        {
            fn();
        }

        return;
    }

    if (pOutFuture)
    {
        *pOutFuture = Task<FutureType>();

        m_scheduler->Enqueue([promise = pOutFuture->Promise(), fn = std::forward<Func>(fn)]() mutable
                             {
                                 if constexpr (std::is_void_v<FutureType>)
                                 {
                                     fn();
                                     promise->Fulfill();
                                 }
                                 else
                                 {
                                     promise->Fulfill(fn());
                                 }
                             },
                             TaskEnqueueFlags::FIRE_AND_FORGET);
    }
    else
    {
        m_scheduler->Enqueue(std::forward<Func>(fn), TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

FilePath AssetRegistry::GetManifestPath(const AssetPath& assetPath) const
{
    const char* bucketName = GetAssetBucketName(assetPath.bucketIndex);
    AssertDebug(bucketName != nullptr);

    return GetRootPath() / bucketName / (String(*assetPath.assetName) + ".hmf");
}

#pragma endregion AssetRegistry

} // namespace Hyperion
