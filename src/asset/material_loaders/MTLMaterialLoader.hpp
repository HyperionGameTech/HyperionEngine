#ifndef HYPERION_V2_MTL_MATERIAL_LOADER_H
#define HYPERION_V2_MTL_MATERIAL_LOADER_H

#include <asset/Assets.hpp>
#include <core/Containers.hpp>
#include <rendering/Material.hpp>

#include <vector>
#include <string>
#include <unordered_map>

namespace hyperion::v2 {


class MTLMaterialLoader : public AssetLoader
{
public:
    struct MaterialLibrary
    {
        struct TextureMapping
        {
            Material::TextureKey key;
            bool srgb = false;
            FilterMode filter_mode = FilterMode::TEXTURE_FILTER_LINEAR;
        };
        
        struct TextureDef
        {
            TextureMapping mapping;
            String name;
        };

        struct ParameterDef
        {
            FixedArray<float, 4> values;
        };

        struct MaterialDef
        {
            String tag;
            Array<TextureDef> textures;
            HashMap<Material::MaterialKey, ParameterDef> parameters;
        };

        String filepath;

        Array<MaterialDef> materials;
    };

    virtual ~MTLMaterialLoader() = default;

    virtual LoadedAsset LoadAsset(LoaderState &state) const override;
};

} // namespace hyperion::v2

#endif