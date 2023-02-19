#include <asset/Assets.hpp>
#include <Engine.hpp>
#include <util/fs/FsUtil.hpp>

#include <asset/model_loaders/FBOMModelLoader.hpp>
#include <asset/model_loaders/FBXModelLoader.hpp>
#include <asset/model_loaders/OBJModelLoader.hpp>
#include <asset/material_loaders/MTLMaterialLoader.hpp>
#include <asset/model_loaders/OgreXMLModelLoader.hpp>
#include <asset/skeleton_loaders/OgreXMLSkeletonLoader.hpp>
#include <asset/texture_loaders/TextureLoader.hpp>
#include <asset/audio_loaders/WAVAudioLoader.hpp>
#include <asset/script_loaders/ScriptLoader.hpp>

namespace hyperion::v2 {

AssetManager::AssetManager()
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

    Register<OBJModelLoader>("obj");
    Register<OgreXMLModelLoader>("mesh.xml");
    Register<OgreXMLSkeletonLoader>("skeleton.xml");
    Register<TextureLoader>(
        "png", "jpg", "jpeg", "tga",
        "bmp", "psd", "gif", "hdr", "tif"
    );
    Register<MTLMaterialLoader>("mtl");
    Register<WAVAudioLoader>("wav");
    Register<ScriptLoader>("hypscript");
    Register<FBOMModelLoader>("fbom");
    Register<FBXModelLoader>("fbx");
}

} // namespace hyperion::v2