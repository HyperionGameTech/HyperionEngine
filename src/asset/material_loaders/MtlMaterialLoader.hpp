#ifndef HYPERION_V2_MTL_MATERIAL_LOADER_H
#define HYPERION_V2_MTL_MATERIAL_LOADER_H

#include <asset/LoaderObject.hpp>
#include <asset/Loader.hpp>
#include <rendering/Material.hpp>

#include <vector>
#include <string>
#include <unordered_map>

namespace hyperion::v2 {

template <>
struct LoaderObject<MaterialGroup, LoaderFormat::MTL_MATERIAL_LIBRARY> {
    class Loader : public LoaderBase<MaterialGroup, LoaderFormat::MTL_MATERIAL_LIBRARY> {
        static LoaderResult LoadFn(LoaderState *state, Object &);
        static std::unique_ptr<MaterialGroup> BuildFn(Engine *engine, const Object &);

    public:
        Loader()
            : LoaderBase({
                .load_fn = LoadFn,
                .build_fn = BuildFn
            })
        {
        }
    };
    
    struct TextureMapping {
        Material::TextureKey key;
        bool                 srgb = false;
    };
    
    struct TextureDef {
        TextureMapping mapping;
        std::string    name;
    };

    struct ParameterDef {
        std::vector<float> values;
    };

    struct MaterialDef {
        std::string tag;
        std::vector<TextureDef> textures;
        std::unordered_map<Material::MaterialKey, ParameterDef> parameters;
    };

    std::string filepath;

    std::vector<MaterialDef> materials;
};

} // namespace hyperion::v2

#endif