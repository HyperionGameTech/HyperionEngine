/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <AssetPch.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetBatch.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/AssetLoader.hpp>

#include <asset/model_loaders/FBOMModelLoader.hpp>
#include <asset/model_loaders/FBXModelLoader.hpp>
#include <asset/model_loaders/GLTFModelLoader.hpp>
#include <asset/model_loaders/OBJModelLoader.hpp>
#include <asset/model_loaders/PLYModelLoader.hpp>
#include <asset/model_loaders/OgreXMLModelLoader.hpp>
#include <asset/material_loaders/MTLMaterialLoader.hpp>

#include <asset/skeleton_loaders/OgreXMLSkeletonLoader.hpp>

#include <asset/texture_loaders/TextureLoader.hpp>

#include <asset/audio_loaders/WAVAudioLoader.hpp>

#include <asset/data_loaders/JSONLoader.hpp>

#include <asset/font_loaders/FontFaceLoader.hpp>
#include <asset/font_loaders/FontAtlasLoader.hpp>

#include <asset/ui_loaders/UILoader.hpp>

#include <Core/threading/TaskSystem.hpp>
#include <Core/threading/TaskThread.hpp>

#include <system/DirectoryInitializer.hpp>

#include <ui/UIObject.hpp>

#include <Core/filesystem/FsUtil.hpp>

#include <engine/EngineDriver.hpp>

#include <HyperionEngine.hpp>

#include <Assets.generated.inl>

namespace Hyperion {

class Skeleton;
class AudioSource;

#pragma region AssetCollector

AssetCollector::~AssetCollector()
{
    if (IsWatching())
    {
        StopWatching();
    }
}

void AssetCollector::NotifyAssetChanged(const FilePath& path, AssetChangeType changeType)
{
    AssertReady();

    OnAssetChanged(path, changeType);
}

#pragma endregion AssetCollector

#pragma region AssetManagerWorkerThread

class AssetManagerWorkerThread : public TaskThread
{
public:
    AssetManagerWorkerThread(ThreadId id)
        : TaskThread(id, ThreadPriorityValue::NORMAL)
    {
    }

    virtual ~AssetManagerWorkerThread() override = default;
};

#pragma endregion AssetManagerWorkerThread

#pragma region AssetManagerThreadPool

class AssetManagerThreadPool : public TaskThreadPool
{
public:
    AssetManagerThreadPool()
        : TaskThreadPool(TypeWrapper<AssetManagerWorkerThread>(), "AssetManagerWorker", 2)
    {
    }

    virtual ~AssetManagerThreadPool() override = default;
};

#pragma endregion AssetManagerThreadPool

#pragma region AssetManager

const Handle<AssetManager>& AssetManager::GetInstance()
{
    return g_assetManager;
}

AssetManager::AssetManager()
    : m_threadPool(MakeUnique<AssetManagerThreadPool>()),
      m_numPendingBatches { 0 }
{
}

AssetManager::~AssetManager()
{
    if (m_threadPool)
    {
        m_threadPool->Stop();
        m_threadPool.Reset();
    }
}

TaskThreadPool* AssetManager::GetThreadPool() const
{
    return m_threadPool.Get();
}

FilePath AssetManager::GetBasePath() const
{
    Mutex::Guard guard(m_assetCollectorsMutex);

    if (Handle<AssetCollector> assetCollector = m_baseAssetCollector.Lock(); assetCollector.IsValid())
    {
        return assetCollector->GetBasePath();
    }

    return FilePath::Current();
}

Handle<AssetCollector> AssetManager::GetBaseAssetCollector() const
{
    Mutex::Guard guard(m_assetCollectorsMutex);

    return m_baseAssetCollector.Lock();
}

void AssetManager::SetBasePath(const FilePath& basePath)
{
    Mutex::Guard guard(m_assetCollectorsMutex);

    Handle<AssetCollector> assetCollector;

    auto assetCollectorsIt = m_assetCollectors.FindIf([basePath](const Handle<AssetCollector>& assetCollector)
        {
            return assetCollector->GetBasePath() == basePath;
        });

    if (assetCollectorsIt != m_assetCollectors.End())
    {
        assetCollector = *assetCollectorsIt;
    }
    else
    {
        assetCollector = MakeHandle<AssetCollector>(basePath);
        InitObject(assetCollector);

        m_assetCollectors.PushBack(assetCollector);

        OnAssetCollectorAdded(assetCollector);
    }

    if (m_baseAssetCollector == assetCollector)
    {
        return;
    }

    m_baseAssetCollector = assetCollector;

    OnBaseAssetCollectorChanged(assetCollector);
}

void AssetManager::ForEachAssetCollector(const ProcRef<void(const Handle<AssetCollector>&)>& callback) const
{
    Mutex::Guard guard(m_assetCollectorsMutex);

    for (const Handle<AssetCollector>& assetCollector : m_assetCollectors)
    {
        callback(assetCollector);
    }
}

void AssetManager::AddAssetCollector(const Handle<AssetCollector>& assetCollector)
{
    if (!assetCollector.IsValid())
    {
        return;
    }

    {
        Mutex::Guard guard(m_assetCollectorsMutex);

        if (m_assetCollectors.Contains(assetCollector))
        {
            return;
        }

        m_assetCollectors.PushBack(assetCollector);
    }

    OnAssetCollectorAdded(assetCollector);
}

void AssetManager::RemoveAssetCollector(const Handle<AssetCollector>& assetCollector)
{
    if (!assetCollector.IsValid())
    {
        return;
    }

    {
        Mutex::Guard guard(m_assetCollectorsMutex);

        if (!m_assetCollectors.Erase(assetCollector))
        {
            return;
        }
    }

    OnAssetCollectorRemoved(assetCollector);
}

const Handle<AssetCollector>& AssetManager::FindAssetCollector(ProcRef<bool(const Handle<AssetCollector>&)> proc) const
{
    Mutex::Guard guard(m_assetCollectorsMutex);

    for (const Handle<AssetCollector>& assetCollector : m_assetCollectors)
    {
        if (proc(assetCollector))
        {
            return assetCollector;
        }
    }

    return Handle<AssetCollector>::empty;
}

AssetBatch* AssetManager::CreateBatch(const String& identifier)
{
    return new AssetBatch(MakeStrongRef(this), identifier);
}

void AssetManager::RegisterDefaultLoaders()
{
    SetBasePath(GetDataDirectory());

    HYP_LOG(Assets, Verbose, "AssetManager Base Path: {}", GetBasePath());

    Register<OBJModelLoader, Node>("obj");
    Register<OgreXMLModelLoader, Node>("mesh.xml");
    Register<OgreXMLSkeletonLoader, Skeleton>("skeleton.xml");
    Register<TextureLoader, Texture>(
        "png", "jpg", "jpeg", "tga",
        "bmp", "psd", "gif", "hdr", "tif");
    Register<WAVAudioLoader, AudioSource>("wav");
    Register<FBOMModelLoader, Node>("hypmodel");
    Register<FBXModelLoader, Node>("fbx");
    Register<GLTFModelLoader, Node>("gltf", "glb");
    // Register<PLYModelLoader, PLYModel>("ply");
    Register<JSONLoader, Value>("json");
    // freetype font loader
    Register<FontFaceLoader, RC<FontFace>>(
        "ttf", "otf", "ttc", "dfont");
    Register<FontAtlasLoader, FontAtlas>();
    Register<UILoader, UIObject>();
}

const AssetLoaderDefinition* AssetManager::GetLoaderDefinition(const FilePath& path, TypeId desiredTypeId)
{
    const String extension = StringUtil::GetExtension(path).ToLower();

    AssetLoaderBase* loader = nullptr;

    SortedArray<KeyValuePair<uint32, const AssetLoaderDefinition*>> loaderPtrs;

    for (const AssetLoaderDefinition& assetLoaderDefinition : m_loaders)
    {
        uint32 rank = 0;

        if (desiredTypeId != TypeId::Void())
        {
            if (!assetLoaderDefinition.HandlesResultType(desiredTypeId))
            {
                continue;
            }

            // Result type is required to be provided for wildcard loaders to be considered
            if (assetLoaderDefinition.IsWildcardExtensionLoader())
            {
                rank += 1;
            }
        }

        if (!extension.Empty() && assetLoaderDefinition.HandlesExtension(path))
        {
            rank += 2;
        }

        if (rank == 0)
        {
            continue;
        }

        loaderPtrs.Insert({ rank, &assetLoaderDefinition });
    }

    if (!loaderPtrs.Empty())
    {
        return loaderPtrs.Front().second;
    }

    return nullptr;
}

void AssetManager::Init()
{
    RegisterDefaultLoaders();

    m_threadPool->Start();

    SetReady(true);
}

Handle<AssetRegistry> AssetManager::GetAssetRegistry() const
{
    return GetCurrentAssetRegistry();
}

void AssetManager::Update(float delta)
{
    HYP_SCOPE;
    AssertOnThread(g_simThread);

    if (Handle<AssetRegistry> engineRegistry = GetEngineAssetRegistry())
    {
        engineRegistry->Update();
    }

    if (Handle<AssetRegistry> currentRegistry = GetCurrentAssetRegistry())
    {
        currentRegistry->Update();
    }

    uint32 numPendingBatches;

    if ((numPendingBatches = m_numPendingBatches.Get(MemoryOrder::ACQUIRE)) != 0)
    {
        HYP_NAMED_SCOPE_FMT("Update pending batches ({})", numPendingBatches);

        /// \todo Set thread priorities based on number of pending batches

        Mutex::Guard guard(m_pendingBatchesMutex);

        for (auto it = m_pendingBatches.Begin(); it != m_pendingBatches.End();)
        {
            if ((*it)->IsCompleted())
            {
                m_completedBatches.PushBack(*it);

                it = m_pendingBatches.Erase(it);

                m_numPendingBatches.Decrement(1, MemoryOrder::RELEASE);
            }
            else
            {
                ++it;
            }
        }
    }

    if (m_completedBatches.Empty())
    {
        return;
    }

    for (AssetBatch* batch : m_completedBatches)
    {
        HYP_NAMED_SCOPE("Process completed batch");

        AssetMap results = batch->AwaitResults();

        for (auto& it : results)
        {
            it.second.OnPostLoad();
        }

        batch->OnComplete(results);

        delete batch;
    }

    m_completedBatches.Clear();
}

void AssetManager::AddPendingBatch(AssetBatch* batch)
{
    if (!batch)
    {
        return;
    }

    Mutex::Guard guard(m_pendingBatchesMutex);

    if (m_pendingBatches.Contains(batch))
    {
        return;
    }

    m_pendingBatches.PushBack(batch);
    m_numPendingBatches.Increment(1, MemoryOrder::RELEASE);
}

HYP_NODISCARD AssetLoadResult AssetManager::Load(
    const TypeId& typeId,
    const String& path,
    const String& batchIdentifier,
    AssetLoadHint hint)
{
    const AssetLoaderDefinition* loaderDefinition = GetLoaderDefinition(path, typeId);

    if (!loaderDefinition)
    {
        return HYP_MAKE_ERROR(AssetLoadError, "No registered loader for the given path", AssetLoadError::ERR_NO_LOADER);
    }

    const Handle<AssetLoaderBase>& loader = loaderDefinition->loader;
    Assert(loader.IsValid());

    return AssetLoadResult(loader->Load(*this, path, batchIdentifier, hint));
}

#pragma endregion AssetManager

} // namespace Hyperion
