/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <asset/AssetLoader.hpp>

#include <rendering/MaterialInstance.hpp>

#include <Core/containers/Map.hpp>

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

    /*! \brief Parse a .mtl file and return a map of material name → Handle<MaterialInstance>.
     *  This can be called directly without going through the asset system. */
    static TMap<String, Handle<MaterialInstance>> ParseMtl(
        FilePath filepath,
        AssetManager& assetManager,
        const String& batchIdentifier = String::empty);

    virtual AssetLoadResult LoadAsset(LoaderState& state) const override;

private:
    static TMap<String, Handle<MaterialInstance>> ParseMtl_Internal(LoaderState& state);
};

} // namespace Hyperion
