/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/Assets.hpp>
#include <asset/AssetBatch.hpp>

#include <asset/model_loaders/FBOMModelLoader.hpp>
#include <asset/model_loaders/FBXModelLoader.hpp>
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

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>

#include <core/object/HypClassUtils.hpp>

#include <ui/UIObject.hpp>

#include <core/filesystem/FsUtil.hpp>
#include <core/profiling/ProfileScope.hpp>

#include <HyperionEngine.hpp>

namespace hyperion {

#pragma region AssetCollector

AssetCollector::~AssetCollector()
{
    if (IsWatching())
    {
        StopWatching();
    }
}

void AssetCollector::Init()
{
    if (IsInitCalled())
    {
        return;
    }

    HypObject::Init();

    if (!m_base_path.Any())
    {
        m_base_path = FilePath::Current();
    }

    if (!m_base_path.IsDirectory())
    {
        m_base_path = m_base_path.BasePath();
    }

    if (!m_base_path.Exists())
    {
        m_base_path.MkDir();
    }

    SetReady(true);
}

void AssetCollector::NotifyAssetChanged(const FilePath& path, AssetChangeType change_type)
{
    AssertReady();

    OnAssetChanged(path, change_type);
}

#pragma endregion AssetCollector

#pragma region AssetManagerWorkerThread

class AssetManagerWorkerThread : public TaskThread
{
public:
    AssetManagerWorkerThread(ThreadID id)
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
    return g_asset_manager;
}

AssetManager::AssetManager()
    : m_asset_cache(MakeUnique<AssetCache>()),
      m_thread_pool(MakeUnique<AssetManagerThreadPool>()),
      m_num_pending_batches { 0 }
{
}

AssetManager::~AssetManager()
{
    if (m_thread_pool)
    {
        m_thread_pool->Stop();
        m_thread_pool.Reset();
    }
}

TaskThreadPool* AssetManager::GetThreadPool() const
{
    return m_thread_pool.Get();
}

FilePath AssetManager::GetBasePath() const
{
    Mutex::Guard guard(m_asset_collectors_mutex);

    if (Handle<AssetCollector> asset_collector = m_base_asset_collector.Lock())
    {
        return asset_collector->GetBasePath();
    }

    return FilePath::Current();
}

Handle<AssetCollector> AssetManager::GetBaseAssetCollector() const
{
    Mutex::Guard guard(m_asset_collectors_mutex);

    return m_base_asset_collector.Lock();
}

void AssetManager::SetBasePath(const FilePath& base_path)
{
    Mutex::Guard guard(m_asset_collectors_mutex);

    Handle<AssetCollector> asset_collector;

    auto asset_collectors_it = m_asset_collectors.FindIf([base_path](const Handle<AssetCollector>& asset_collector)
        {
            return asset_collector->GetBasePath() == base_path;
        });

    if (asset_collectors_it != m_asset_collectors.End())
    {
        asset_collector = *asset_collectors_it;
    }
    else
    {
        asset_collector = CreateObject<AssetCollector>(base_path);
        InitObject(asset_collector);

        m_asset_collectors.PushBack(asset_collector);

        OnAssetCollectorAdded(asset_collector);
    }

    if (m_base_asset_collector == asset_collector)
    {
        return;
    }

    m_base_asset_collector = asset_collector;

    OnBaseAssetCollectorChanged(asset_collector);
}

void AssetManager::ForEachAssetCollector(const ProcRef<void(const Handle<AssetCollector>&)>& callback) const
{
    HYP_SCOPE;

    Mutex::Guard guard(m_asset_collectors_mutex);

    for (const Handle<AssetCollector>& asset_collector : m_asset_collectors)
    {
        callback(asset_collector);
    }
}

void AssetManager::AddAssetCollector(const Handle<AssetCollector>& asset_collector)
{
    if (!asset_collector.IsValid())
    {
        return;
    }

    {
        Mutex::Guard guard(m_asset_collectors_mutex);

        if (m_asset_collectors.Contains(asset_collector))
        {
            return;
        }

        m_asset_collectors.PushBack(asset_collector);
    }

    OnAssetCollectorAdded(asset_collector);
}

void AssetManager::RemoveAssetCollector(const Handle<AssetCollector>& asset_collector)
{
    if (!asset_collector.IsValid())
    {
        return;
    }

    {
        Mutex::Guard guard(m_asset_collectors_mutex);

        if (!m_asset_collectors.Erase(asset_collector))
        {
            return;
        }
    }

    OnAssetCollectorRemoved(asset_collector);
}

const Handle<AssetCollector>& AssetManager::FindAssetCollector(ProcRef<bool(const Handle<AssetCollector>&)> proc) const
{
    Mutex::Guard guard(m_asset_collectors_mutex);

    for (const Handle<AssetCollector>& asset_collector : m_asset_collectors)
    {
        if (proc(asset_collector))
        {
            return asset_collector;
        }
    }

    return Handle<AssetCollector>::empty;
}

RC<AssetBatch> AssetManager::CreateBatch()
{
    return MakeRefCountedPtr<AssetBatch>(this);
}

void AssetManager::RegisterDefaultLoaders()
{
    SetBasePath(FilePath::Join(HYP_ROOT_DIR, "res"));

    Register<OBJModelLoader, Node>("obj");
    Register<OgreXMLModelLoader, Node>("mesh.xml");
    Register<OgreXMLSkeletonLoader, Skeleton>("skeleton.xml");
    Register<TextureLoader, Texture>(
        "png", "jpg", "jpeg", "tga",
        "bmp", "psd", "gif", "hdr", "tif");
    Register<MTLMaterialLoader, MaterialGroup>("mtl");
    Register<WAVAudioLoader, AudioSource>("wav");
    Register<FBOMModelLoader, Node>("hypmodel");
    Register<FBXModelLoader, Node>("fbx");
    // Register<PLYModelLoader, PLYModel>("ply");
    Register<JSONLoader, JSONValue>("json");
    // freetype font loader
    Register<FontFaceLoader, RC<FontFace>>(
        "ttf", "otf", "ttc", "dfont");
    Register<FontAtlasLoader, RC<FontAtlas>>();
    Register<UILoader, UIObject>();
}

const AssetLoaderDefinition* AssetManager::GetLoaderDefinition(const FilePath& path, TypeID desired_type_id)
{
    HYP_SCOPE;

    const String extension = StringUtil::GetExtension(path).ToLower();

    AssetLoaderBase* loader = nullptr;

    SortedArray<KeyValuePair<uint32, const AssetLoaderDefinition*>> loader_ptrs;

    for (const AssetLoaderDefinition& asset_loader_definition : m_loaders)
    {
        uint32 rank = 0;

        if (desired_type_id != TypeID::Void())
        {
            if (!asset_loader_definition.HandlesResultType(desired_type_id))
            {
                continue;
            }

            // Result type is required to be provided for wildcard loaders to be considered
            if (asset_loader_definition.IsWildcardExtensionLoader())
            {
                rank += 1;
            }
        }

        if (!extension.Empty() && asset_loader_definition.HandlesExtension(path))
        {
            rank += 2;
        }

        if (rank == 0)
        {
            continue;
        }

        loader_ptrs.Insert({ rank, &asset_loader_definition });
    }

    if (!loader_ptrs.Empty())
    {
        return loader_ptrs.Front().second;
    }

    return nullptr;
}

void AssetManager::Init()
{
    if (IsInitCalled())
    {
        return;
    }

    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
        {
            if (m_thread_pool)
            {
                m_thread_pool->Stop();
                m_thread_pool.Reset();
            }
        }));

    RegisterDefaultLoaders();

    m_thread_pool->Start();
}

void AssetManager::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    uint32 num_pending_batches;

    if ((num_pending_batches = m_num_pending_batches.Get(MemoryOrder::ACQUIRE)) != 0)
    {
        HYP_NAMED_SCOPE_FMT("Update pending batches ({})", num_pending_batches);

        /// \todo Set thread priorities based on number of pending batches

        Mutex::Guard guard(m_pending_batches_mutex);

        for (auto it = m_pending_batches.Begin(); it != m_pending_batches.End();)
        {
            if ((*it)->IsCompleted())
            {
                m_completed_batches.PushBack(std::move(*it));

                it = m_pending_batches.Erase(it);

                m_num_pending_batches.Decrement(1, MemoryOrder::RELEASE);
            }
            else
            {
                ++it;
            }
        }
    }

    if (m_completed_batches.Empty())
    {
        return;
    }

    for (const RC<AssetBatch>& batch : m_completed_batches)
    {
        HYP_NAMED_SCOPE("Process completed batch");

        AssetMap results = batch->AwaitResults();

        for (auto& it : results)
        {
            it.second.OnPostLoad();
        }

        batch->OnComplete(results);
    }

    m_completed_batches.Clear();
}

void AssetManager::AddPendingBatch(const RC<AssetBatch>& batch)
{
    if (!batch)
    {
        return;
    }

    Mutex::Guard guard(m_pending_batches_mutex);

    if (m_pending_batches.Contains(batch))
    {
        return;
    }

    m_pending_batches.PushBack(batch);
    m_num_pending_batches.Increment(1, MemoryOrder::RELEASE);
}

#pragma endregion AssetManager

} // namespace hyperion