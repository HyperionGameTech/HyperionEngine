#include <asset/Assets.hpp>
#include <Engine.hpp>
#include <util/fs/FsUtil.hpp>

#include <asset/model_loaders/FBOMModelLoader.hpp>
#include <asset/model_loaders/FBXModelLoader.hpp>
#include <asset/model_loaders/OBJModelLoader.hpp>
#include <asset/model_loaders/PLYModelLoader.hpp>
#include <asset/material_loaders/MTLMaterialLoader.hpp>
#include <asset/model_loaders/OgreXMLModelLoader.hpp>
#include <asset/skeleton_loaders/OgreXMLSkeletonLoader.hpp>
#include <asset/texture_loaders/TextureLoader.hpp>
#include <asset/audio_loaders/WAVAudioLoader.hpp>
#include <asset/data_loaders/JSONLoader.hpp>
#include <asset/font_loaders/FontFaceLoader.hpp>

namespace hyperion::v2 {

const bool AssetManager::asset_cache_enabled = false;

AssetManager::AssetManager()
    : m_asset_cache(new AssetCache())
{
    RegisterDefaultLoaders();
}

ObjectPool &AssetManager::GetObjectPool()
{
    return g_engine->GetObjectPool();
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
    Register<FontFaceLoader, FontFace>(
        "ttf", "otf", "ttc", "dfont"
    );
}

AssetLoaderBase *AssetManager::GetLoader(const FilePath &path)
{
    const String extension(StringUtil::ToLower(StringUtil::GetExtension(path.Data())).c_str());

    if (extension.Empty()) {
        return nullptr;
    }

    AssetLoaderBase *loader = nullptr;

    { // find loader for the requested type
        const auto it = m_loaders.Find(extension);

        if (it != m_loaders.End()) {
            loader = it->second.second.Get();
        } else {
            for (auto &loader_it : m_loaders) {
                if (String(StringUtil::ToLower(path.Data()).c_str()).EndsWith(loader_it.first)) {
                    loader = loader_it.second.second.Get();
                    break;
                }
            }
        }
    }

    return loader;
}

} // namespace hyperion::v2