/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MTL_MATERIAL_LOADER_HPP
#define HYPERION_MTL_MATERIAL_LOADER_HPP

#include <asset/Assets.hpp>

#include <scene/Material.hpp>

namespace hyperion {

class MTLMaterialLoader : public AssetLoaderBase
{
public:
    struct MaterialLibrary
    {
        struct TextureMapping
        {
            MaterialTextureKey key;
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
            FixedArray<float, 4> values {};
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

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace hyperion

#endif