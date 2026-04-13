/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <asset/AssetLoader.hpp>

#include <rendering/Material.hpp>

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class MTLMaterialLoader : public AssetLoaderBase
{
    HYP_OBJECT_BODY(MTLMaterialLoader);

public:
    struct MaterialLibrary
    {
        struct TextureMapping
        {
            MaterialTextureKey key;
            bool srgb = false;
            TextureFilterMode filterMode = TFM_LINEAR;
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
            MaterialParameters parameters;
        };

        String filepath;
        Array<MaterialDef> materials;
    };

    virtual ~MTLMaterialLoader() = default;

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;
};

} // namespace Hyperion
