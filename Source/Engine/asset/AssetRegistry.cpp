/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <AssetPch.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/AssetObject.hpp>
#include <asset/AssetBatch.hpp>
#include <asset/AssetReference.hpp>
#include <asset/Assets.hpp>
#include <asset/BlobStorage.hpp>
#include <asset/BlobStorageViews.hpp>

#include <Core/utilities/DeferredScope.hpp>
#include <Core/utilities/GlobalContext.hpp>

#include <Core/reflection/TypeInfo.hpp>
#include <Core/reflection/Class.hpp>

#include <Core/threading/Scheduler.hpp>

#include <Core/serialization/SerializationUtils.hpp>
#include <Core/reflection/Field.hpp>
#include <Core/reflection/Property.hpp>

#include <Core/io/ByteWriter.hpp>

#include <Core/json/JSON.hpp>

#include <Core/config/Config.hpp>

#include <scene/Entity.hpp>
#include <scene/EntityManager.hpp>
#include <scene/Scene.hpp>

#include <streaming/StreamingCell.hpp>

#include <engine/EngineDriver.hpp>

#include <AssetRegistry.generated.inl>

namespace Hyperion {

namespace CoreApi {
extern FilePath GetExecutablePath();
extern HYP_NODISCARD FilePath CreateTempDirectory();
extern const GlobalConfig& GetGlobalConfig();
} // namespace CoreApi

HYP_API extern const FilePath& GetCacheDirectory();

static Handle<AssetRegistry> s_engineAssetRegistry;

static Mutex s_currentAssetRegistryMtx;
static Array<Handle<AssetRegistry>> s_currentAssetRegistryStack;

HYP_API Handle<AssetRegistry> GetCurrentAssetRegistry()
{
    // try getting thread-local registry override.
    AssetRegistryContext* ctx = GetGlobalContext<AssetRegistryContext>();

    if (ctx != nullptr && ctx->registry.IsValid())
    {
        return ctx->registry;
    }

    Mutex::Guard guard(s_currentAssetRegistryMtx);

    if (s_currentAssetRegistryStack.Any())
    {
        return s_currentAssetRegistryStack.Back();
    }

    return Handle<AssetRegistry>::Null();
}

HYP_API void PushCurrentAssetRegistry(const Handle<AssetRegistry>& registry)
{
    Mutex::Guard guard(s_currentAssetRegistryMtx);
    s_currentAssetRegistryStack.PushBack(registry);
}

HYP_API void PopCurrentAssetRegistry()
{
    Mutex::Guard guard(s_currentAssetRegistryMtx);
    s_currentAssetRegistryStack.PopBack();
}

HYP_API Handle<AssetRegistry> GetEngineAssetRegistry()
{
    return s_engineAssetRegistry;
}

HYP_API void SetEngineAssetRegistry(const Handle<AssetRegistry>& registry)
{
    if (registry.IsValid())
    {
        registry->LoadAssetDescs();
    }
    
    s_engineAssetRegistry = registry;
}

static const ThreadId& s_assetRegistryThread = g_simThread;

static constexpr const char* BlobStorageName = "Storage";

// If true, all mutation operations will be forced to run on the sim thread,
// otherwise a mutex will be used to allow multi-threaded access.
static constexpr bool UseSingleThread = false;

HYP_API extern const FilePath& GetLibraryDirectory();

#if HYP_EDITOR
HYP_API extern const FilePath& GetProjectsDirectory();
#endif

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

static Result ReadManifest(ByteReader& stream, const FilePath& manifestPath, JSON::Object& outManifestData)
{
    String str = String(stream.Read().ToByteView());

    JSON::ParseResult parseResult = JSON::Parse(str);

    if (!parseResult.ok)
    {
        return HYP_MAKE_ERROR(Error, "Failed to parse manifest JSON: {}", parseResult.message);
    }

    JSON::Value manifestJson = std::move(parseResult.value);

    if (!manifestJson.IsObject())
    {
        return HYP_MAKE_ERROR(Error, "Manifest JSON at path {} is not a valid JSON object:\n{}",
            manifestPath, manifestJson.ToString(true));
    }

    outManifestData = std::move(manifestJson.AsObject());

    return {}; // ok
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
    }

    if (assetObject.IsValid() && assetObject->m_assetIndex != AssetDesc::InvalidIndex)
    {
        // reuse the existing index, if we don't have one.
        if (assetDesc.index == AssetDesc::InvalidIndex)
        {
            assetDesc.index = assetObject->m_assetIndex;
        }
        // otherwise, free the old index and take the desc one.
        else
        {
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

    if (const Handle<AssetObject>* pOldAssetObject = assetObjectCache.TryGet(assetDesc.index); pOldAssetObject && pOldAssetObject->IsValid() && (*pOldAssetObject) != assetObject)
    {
        const Handle<AssetObject>& oldAssetObject = *pOldAssetObject;

        oldAssetObject->OnUnloaded();
        oldAssetObject->m_assetIndex = AssetDesc::InvalidIndex;
    }
    
    assetObject->m_name = assetDesc.name;
    assetObject->m_assetIndex = assetDesc.index;
    assetObject->m_assetPath = AssetPath(registryId, *AssetBuckets::AllBuckets[bucketIndex], assetDesc.name);

    assetObjectCache[assetDesc.index] = assetObject;

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
      m_scheduler(new Scheduler(s_assetRegistryThread)),
      m_pruneTimer { 5.0 }, // every 5 seconds
      m_pruneTaskBatch(nullptr),
      m_saveBlobCacheTimer { 5.0 }, // every 5 seconds
      m_saveBlobCacheBatch(nullptr),
      m_blobStorage(nullptr)
{
    for (uint32 bucketIndex = 0; bucketIndex < uint32(m_assetBucketData.Size()); bucketIndex++)
    {
        AssetBucketData& bucketData = m_assetBucketData[bucketIndex];

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

    if (m_saveBlobCacheBatch != nullptr)
    {
        if (!m_saveBlobCacheBatch->IsCompleted())
        {
            HYP_LOG(Assets, Info, "Waiting for blob cache to finish saving");
            m_saveBlobCacheBatch->AwaitCompletion();
        }

        delete m_saveBlobCacheBatch;
        m_saveBlobCacheBatch = nullptr;
    }

    if (m_blobStorage != nullptr)
    {
        m_blobStorage->Release();
        m_blobStorage = nullptr;
    }

    delete m_scheduler;
}

void AssetRegistry::Initialize()
{
    if (m_isInitialized)
    {
        return;
    }

    Assert(m_rootPath.Length() > 0);

    if (m_rootPath.Exists())
    {
        Assert(m_rootPath.IsDirectory(), "AssetRegistry root path ({}) exists but is not a directory!", m_rootPath);
    }
    else
    {
        Assert(m_rootPath.MkDir(), "Failed to create root directory for AssetRegistry: {}", m_rootPath);
    }

    const FilePath blobStorageDir = m_rootPath / "Cache";
    Assert(blobStorageDir.Exists() ? blobStorageDir.IsDirectory() : blobStorageDir.MkDir(),
        "Failed to create blob storage directory for AssetRegistry: {}", blobStorageDir);

#if !HYP_EDITOR
    InitBlobStorage(blobStorageDir);
#endif

    m_isInitialized = true;
}

void AssetRegistry::Shutdown()
{
    if (!m_isInitialized)
    {
        return;
    }

    // unload all cached assets
    RemoveCached();

    if (m_blobStorage != nullptr)
    {
        m_blobStorage->Release();
        m_blobStorage = nullptr;
    }

    m_isInitialized = false;
}

void AssetRegistry::SaveBlobCache(bool async)
{
    auto DoSaveBlobCache = [this, weakThis = MakeWeakRef(this)]()
    {
        Handle<AssetRegistry> registry = weakThis.Lock();

        if (!registry.IsValid())
            return;

        if (m_blobStorage != nullptr)
        {
            Result result = m_blobStorage->SaveIfDirty();

            if (result.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to save blob storage - error message was: {}", result.GetError().GetMessage());
            }
        }
    };

    if (async)
    {
        if (m_saveBlobCacheBatch != nullptr)
        {
            if (!m_saveBlobCacheBatch->IsCompleted())
            {
                HYP_LOG(Assets, Warning, "Skipping saving blob cache - async task already processing");

                return;
            }

            m_saveBlobCacheBatch->ResetState();
        }
        else
        {
            m_saveBlobCacheBatch = new TaskBatch;
            m_saveBlobCacheBatch->pool = &TaskSystem::GetInstance().GetPool(TaskThreadPoolName::THREAD_POOL_BACKGROUND);
        }

        m_saveBlobCacheBatch->AddTask(DoSaveBlobCache);

        TaskSystem::GetInstance().EnqueueBatch(m_saveBlobCacheBatch);
    }
    else
    {
        DoSaveBlobCache();
    }
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
    for (AssetBucketData& bucketData : m_assetBucketData)
    {
        TUniqueLock bucketLock(bucketData.mtx);

        for (const AssetDesc& assetDesc : bucketData.assetDescs)
        {
            // Load the asset object if it's not already loaded, so that it gets marked dirty and saved to the new location. This is needed in case the asset was modified but not yet saved, otherwise those changes would be lost.
            if (!bucketData.assetObjectCache.HasIndex(assetDesc.index))
            {
                bucketLock.Reset();

                Handle<AssetObject> assetObject = GetAsset(*AssetBuckets::AllBuckets[bucketData.bucketIndex], assetDesc.name);
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

    const Handle<AssetObject>* pAssetObject = data.assetObjectCache.TryGet(index);

    if (pAssetObject != nullptr)
    {
        return *pAssetObject;
    }

    lock.Reset();

    String strName = String(*Name(name));
    AssertDebug(strName.Length() > 0);

    // Load it into cache
    Handle<AssetObject> assetObject;

    const FilePath manifestPath = GetManifestPath(AssetPath(m_registryId, bucket, Name(name)));
    FileByteReader stream { manifestPath };

    JSON::Object manifestData;

    if (Result readManifestResult = ReadManifest(stream, manifestPath, manifestData); readManifestResult.HasError())
    {
        HYP_LOG(Assets, Error, "Failed to read asset manifest: {}", readManifestResult.GetError().GetMessage());

        return Handle<AssetObject>::Null();
    }
        
    Result loadResult = AssetObject::Load(manifestData, assetObject);

    if (loadResult.HasError())
    {
        HYP_LOG(Assets, Error, "Failed to load asset: {}", loadResult.GetError().GetMessage());

        return Handle<AssetObject>::Null();
    }

    assetObject->m_assetIndex = index;
    assetObject->m_assetPath = AssetPath(m_registryId, bucket, assetObject->m_name);

    { // set the asset in cache
        TUniqueLock packageLock(data.mtx);

        // check again in case another thread added it; in that case we'll reuse the existing asset and discard ours
        pAssetObject = data.assetObjectCache.TryGet(index);

        if (pAssetObject != nullptr)
        {
            return *pAssetObject;
        }

        data.assetObjectCache[index] = assetObject;
    }

    InitObject(assetObject);
    assetObject->OnLoaded();

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

        data.AllocateUniqueAssetName(
            *assetObject->InstanceClass()->GetName(),
            assetDesc,
            [&assetObject](const AssetDesc& otherDesc) -> bool
            {
                return assetObject->m_assetIndex == otherDesc.index;
            });
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

    data.AllocateUniqueAssetName(
        *assetDesc.name,
        assetDesc,
        [&assetObject](const AssetDesc& otherDesc) -> bool
        {
            return assetObject->m_assetIndex == otherDesc.index;
        });

    assetObject->m_name = assetDesc.name;

    data.SetAsset(assetDesc, assetObject);
}

void AssetRegistry::PutAssetsDeep(const Handle<AssetObject>& targetAsset)
{
    if (!targetAsset.IsValid())
    {
        return;
    }

    GlobalContextScope contextScope { AssetRegistryContext { MakeStrongRef(this) } };

    // Recurse through the objects' fields, registering assets with their respective buckets
    //// \todo : Change to a Stack, recursion could get impressively deep.

    HashSet<const ObjectBase*> visited; // to avoid infinite recursion

    bool shouldFollowAssetPaths = false;

    Proc<void(const BoxedValue&)> Iterate;
    Iterate = [&](const BoxedValue& current) -> void
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

        const TypeInfo& typeInfo = *current.GetTypeInfo();

        bool walked = false;

        WalkBoxedValue(current, [&](const BoxedValue& value)
            {
                Iterate(value);

                walked = true;
            });

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

                        Iterate(BoxedValue(componentRef));
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

            Iterate(memberData);
        }

        if (assetObject)
        {
            if (assetObject->m_assetIndex == AssetDesc::InvalidIndex)
            {
                PutAssetUnique(assetObject);
            }

            AssertDebug(assetObject->m_name.IsValid());
        }
    };

    Iterate(BoxedValue(targetAsset));
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

void AssetRegistry::LoadAssetDescs()
{
    const FilePath rootPath = GetRootPath();

    if (!rootPath.Exists() || !rootPath.IsDirectory())
    {
        HYP_LOG(Assets, Warning, "Root directory '{}' does not exist or is not a directory", rootPath);
        return;
    }

    for (const FilePath& subdirectory : rootPath.GetSubdirectories())
    {
        const String bucketName = subdirectory.Basename();
        const AssetBucket& bucket = GetAssetBucketByName(StringHash(bucketName));

        if (bucket == AssetBuckets::None)
        {
            HYP_LOG(Assets, Verbose, "Subdirectory '{}' does not match any known AssetBucket, skipping", bucketName);
            continue;
        }

        AssetBucketData& data = m_assetBucketData[bucket.GetIndex()];

        // Reserve index 0 (== AssetDesc::InvalidIndex) so it is never assigned to a real asset.
        data.usedIndices.Set(0, true);

        Array<FilePath> assetFiles;

        for (auto iter = subdirectory.OpenDirectory(); iter.HasNext(); iter.Advance())
        {
            if (iter.CurrentIsDirectory())
            {
                continue;
            }

            const FilePath curr = iter.Current();

            if (curr.GetExtension() != "json")
            {
                continue;
            }

            assetFiles.PushBack(curr);
        }

        Array<AssetDesc> assetDescs;
        assetDescs.Reserve(assetFiles.Size());

        for (const FilePath& entry : assetFiles)
        {
            FileByteReader stream { entry };

            JSON::Object manifestData;
            if (Result readResult = ReadManifest(stream, entry, manifestData); readResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to read manifest '{}': {}", entry, readResult.GetError().GetMessage());
                continue;
            }

            AssetDesc assetDesc;
            if (Result loadDescResult = AssetObject::LoadDesc(manifestData, assetDesc); loadDescResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to load asset desc from '{}': {}", entry, loadDescResult.GetError().GetMessage());
                continue;
            }

            assetDescs.PushBack(std::move(assetDesc));
        }

        if (assetDescs.Any())
        {
            TUniqueLock lock(data.mtx);

            for (AssetDesc& assetDesc : assetDescs)
            {
                if (data.assetDescs.Contains(assetDesc.name))
                {
                    HYP_LOG(Assets, Verbose, "Asset '{}' already present in bucket '{}', skipping",
                        assetDesc.name, bucketName);
                    continue;
                }

                AssertDebug(assetDesc.index == AssetDesc::InvalidIndex);

                assetDesc.index = data.usedIndices.FirstZeroBitIndex();
                data.usedIndices.Set(assetDesc.index, true);

                data.assetDescs.Add(std::move(assetDesc));
            }
        }
    }
}

void AssetRegistry::SaveDirtyAssets()
{
    const FilePath rootPath = GetRootPath();

    if (!rootPath.Exists() || !rootPath.IsDirectory())
    {
        HYP_LOG(Assets, Warning, "Root directory '{}' does not exist or is not a directory", rootPath);
        return;
    }

    BlobStorage* blobStorage = HasBlobStorage() ? &GetBlobStorage() : nullptr;

    for (uint32 bucketIndex = 1; bucketIndex < MaxAssetBuckets; ++bucketIndex)
    {
        AssetBucketData& data = m_assetBucketData[bucketIndex];

        const char* bucketName = GetAssetBucketName(bucketIndex);
        AssertDebug(bucketName != nullptr);

        HashSet<Handle<AssetObject>> dirtyAssets;

        {
            TUniqueLock lock(data.mtx);

            if (!data.dirtyIndices.AnyBitsSet())
            {
                continue;
            }

            Bitset seenIndices;

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
                HYP_LOG(Assets, Error, "Failed to create bucket directory '{}'", bucketDir);
                continue;
            }
        }

        for (const Handle<AssetObject>& assetObject : dirtyAssets)
        {
            auto readScope = assetObject->GetReadScope();

            const Name assetName = assetObject->GetName();
            AssertDebug(assetName.IsValid());

            if (!assetName.IsValid())
            {
                continue;
            }

            const FilePath manifestPath = GetManifestPath(assetObject->GetPath());

            if (Result saveBlobResult = assetObject->SaveBlobData(blobStorage, bucketDir); saveBlobResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to save blob data for asset '{}' in bucket '{}': {}",
                    assetName, bucketName, saveBlobResult.GetError().GetMessage());

                continue;
            }

            {
                FileByteWriter manifestWriter { manifestPath };

                if (!manifestWriter.IsOpen())
                {
                    HYP_LOG(Assets, Error, "Failed to open manifest file '{}' for writing", manifestPath);
                    continue;
                }

                if (Result saveManifestResult = assetObject->SaveManifest(manifestWriter); saveManifestResult.HasError())
                {
                    HYP_LOG(Assets, Error, "Failed to save manifest for asset '{}' in bucket '{}': {}",
                        assetName, bucketName, saveManifestResult.GetError().GetMessage());
                    continue;
                }

                manifestWriter.Close();
            }

            readScope.Reset();

            HYP_LOG(Assets, Verbose, "Saved asset '{}' to '{}'", assetName, manifestPath);
        }
    }
}


void AssetRegistry::RemoveCached()
{
    for (AssetBucketData& bucketData : m_assetBucketData)
    {
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

BlobStorage& AssetRegistry::GetBlobStorage()
{
    Assert(m_blobStorage != nullptr);

    return *m_blobStorage;
}

void AssetRegistry::InitBlobStorage(const FilePath& blobStorageDir)
{
    if (m_blobStorage != nullptr)
    {
        return;
    }

    Assert(blobStorageDir.Exists() && blobStorageDir.IsDirectory(), "Blob storage directory '{}' does not exist or is not a directory", blobStorageDir);

    const uint64 s_blobStoragePageSize = CoreApi::GetGlobalConfig().Get("App.Cache.PageSize")
        .ToUInt64(/* defaultValue */ BlobStorage::DefaultPageSize);
    
    m_blobStorage = new BlobStorage(blobStorageDir, s_blobStoragePageSize);
}

void AssetRegistry::Update()
{
    HYP_SCOPE;
    AssertOnThread(s_assetRegistryThread);

#if HYP_EDITOR
    if (!m_pruneTimer.Waiting())
    {
       m_pruneTimer.NextTick();

       // @TODO
    }

    if (!m_saveBlobCacheTimer.Waiting())
    {
        m_saveBlobCacheTimer.NextTick();

        SaveBlobCache(/* async */ true);
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

    return GetRootPath() / bucketName / (String(*assetPath.assetName) + ".json");
}

#pragma endregion AssetRegistry

} // namespace Hyperion
