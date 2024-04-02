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
#include <asset/font_loaders/FontAtlasLoader.hpp>

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
    Register<FontAtlasLoader, FontAtlas>();
}

const AssetLoaderDefinition *AssetManager::GetLoader(const FilePath &path, TypeID desired_type_id)
{
    const String extension(StringUtil::ToLower(StringUtil::GetExtension(path.Data())).c_str());

    AssetLoaderBase *loader = nullptr;

    SortedArray<KeyValuePair<uint, const AssetLoaderDefinition *>> loader_ptrs;

    for (const AssetLoaderDefinition &asset_loader_definition : m_loaders) {
        uint rank = 0;

        if (desired_type_id != TypeID::void_type_id) {
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

} // namespace hyperion::v2