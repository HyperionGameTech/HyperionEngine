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

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/utilities/Format.hpp>

#include <core/object/HypClassUtils.hpp>

#include <ui/UIObject.hpp>

#include <util/fs/FsUtil.hpp>
#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_BEGIN_CLASS(AssetManager)
    HYP_PROPERTY(BasePath, &AssetManager::GetBasePath, &AssetManager::SetBasePath)
HYP_END_CLASS

const Handle<AssetManager> &AssetManager::GetInstance()
{
    return g_asset_manager;
}

AssetManager::AssetManager()
    : m_asset_cache(new AssetCache()),
      m_num_pending_batches { 0 }
{
    RegisterDefaultLoaders();
}

RC<AssetBatch> AssetManager::CreateBatch()
{
    RC<AssetBatch> batch(new AssetBatch(this));

    Mutex::Guard guard(m_pending_batches_mutex);

    m_pending_batches.PushBack(batch);
    m_num_pending_batches.Increment(1, MemoryOrder::RELEASE);

    return batch;
}

void AssetManager::RegisterDefaultLoaders()
{
    SetBasePath(FilePath::Join(HYP_ROOT_DIR, "res"));

    Register<OBJModelLoader, Node>("obj");
    Register<OgreXMLModelLoader, Node>("mesh.xml");
    Register<OgreXMLSkeletonLoader, Skeleton>("skeleton.xml");
    Register<TextureLoader, Texture>(
        "png", "jpg", "jpeg", "tga",
        "bmp", "psd", "gif", "hdr", "tif"
    );
    Register<MTLMaterialLoader, MaterialGroup>("mtl");
    Register<WAVAudioLoader, AudioSource>("wav");
    Register<FBOMModelLoader, Node>("fbom");
    Register<FBXModelLoader, Node>("fbx");
    // Register<PLYModelLoader, PLYModel>("ply");
    Register<JSONLoader, JSONValue>("json");
    // freetype font loader
    Register<FontFaceLoader, RC<FontFace>>(
        "ttf", "otf", "ttc", "dfont"
    );
    Register<FontAtlasLoader, RC<FontAtlas>>();
    Register<UILoader, RC<UIObject>>();
}

const AssetLoaderDefinition *AssetManager::GetLoader(const FilePath &path, TypeID desired_type_id)
{
    HYP_SCOPE;

    const String extension(StringUtil::ToLower(StringUtil::GetExtension(path.Data())).c_str());

    AssetLoaderBase *loader = nullptr;

    SortedArray<KeyValuePair<uint32, const AssetLoaderDefinition *>> loader_ptrs;

    for (const AssetLoaderDefinition &asset_loader_definition : m_loaders) {
        uint32 rank = 0;

        if (desired_type_id != TypeID::Void()) {
            if (!asset_loader_definition.HandlesResultType(desired_type_id)) {
                continue;
            }

            // Result type is required to be provided for wildcard loaders to be considered
            if (asset_loader_definition.IsWildcardExtensionLoader()) {
                rank += 1;
            }
        }

        if (!extension.Empty() && asset_loader_definition.HandlesExtension(path)) {
            rank += 2;
        }

        if (rank == 0) {
            continue;
        }

        loader_ptrs.Insert({ rank, &asset_loader_definition });
    }

    if (!loader_ptrs.Empty()) {
        return loader_ptrs.Front().second;
    }

    return nullptr;
}

void AssetManager::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    uint32 num_pending_batches;

    if ((num_pending_batches = m_num_pending_batches.Get(MemoryOrder::ACQUIRE)) != 0) {
        HYP_NAMED_SCOPE_FMT("Update pending batches ({})", num_pending_batches);

        Mutex::Guard guard(m_pending_batches_mutex);

        for (auto it = m_pending_batches.Begin(); it != m_pending_batches.End();) {
            if ((*it)->IsCompleted()) {
                m_completed_batches.PushBack(std::move(*it));

                it = m_pending_batches.Erase(it);

                m_num_pending_batches.Decrement(1, MemoryOrder::RELEASE);
            } else {
                ++it;
            }
        }
    }

    if (m_completed_batches.Empty()) {
        return;
    }

    for (const RC<AssetBatch> &batch : m_completed_batches) {
        HYP_NAMED_SCOPE("Process completed batch");

        AssetMap results = batch->AwaitResults();

        for (auto &it : results) {
            it.second.OnPostLoad();
        }
        
        batch->OnComplete(results);
    }

    m_completed_batches.Clear();
}

} // namespace hyperion