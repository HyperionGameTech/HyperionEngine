#include "MTLMaterialLoader.hpp"
#include "../../Engine.hpp"

#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using Tokens = std::vector<std::string>;
using MTLMaterialLoader = LoaderObject<MaterialGroup, LoaderFormat::MTL_MATERIAL_LIBRARY>::Loader;

enum IlluminationModel {
    ILLUM_COLOR,
    ILLUM_COLOR_AMBIENT,
    ILLUM_HIGHLIGHT,
    ILLUM_REFLECTIVE_RAYTRACED,
    ILLUM_TRANSPARENT_GLASS_RAYTRACED,
    ILLUM_FRESNEL_RAYTRACED,
    ILLUM_TRANSPARENT_REFRACTION_RAYTRACED,
    ILLUM_TRANSPARENT_FRESNEL_REFRACTION_RAYTRACED,
    ILLUM_REFLECTIVE,
    ILLUM_TRANSPARENT_REFLECTIVE_GLASS,
    ILLUM_SHADOWS
};

template <class Vector>
static Vector ReadVector(const Tokens &tokens, size_t offset = 1)
{
    Vector result{0.0f};

    int value_index = 0;

    for (size_t i = offset; i < tokens.size(); i++) {
        const auto &token = tokens[i];

        if (token.empty()) {
            continue;
        }

        result.values[value_index++] = float(std::atof(token.c_str()));

        if (value_index == std::size(result.values)) {
            break;
        }
    }

    return result;
}

static void AddMaterial(MTLMaterialLoader::Object &object, const std::string &tag)
{
    std::string unique_tag(tag);
    int counter = 0;

    while (std::any_of(
        object.materials.begin(),
        object.materials.end(),
        [&unique_tag](const auto &mesh) { return mesh.tag == unique_tag; }
    )) {
        unique_tag = tag + std::to_string(++counter);
    }

    object.materials.push_back({
        .tag = unique_tag
    });
}

static auto &LastMaterial(MTLMaterialLoader::Object &object)
{
    if (object.materials.empty()) {
        AddMaterial(object, "default");
    }

    return object.materials.back();
}

static bool IsTransparencyModel(IlluminationModel illum_model)
{
    return illum_model == ILLUM_TRANSPARENT_GLASS_RAYTRACED
        || illum_model == ILLUM_TRANSPARENT_REFRACTION_RAYTRACED
        || illum_model == ILLUM_TRANSPARENT_FRESNEL_REFRACTION_RAYTRACED
        || illum_model == ILLUM_TRANSPARENT_REFLECTIVE_GLASS;
}

LoaderResult MTLMaterialLoader::LoadFn(LoaderState *state, Object &object)
{
    object.filepath = state->filepath;

    const std::unordered_map<std::string, TextureMapping> texture_keys{
        std::make_pair("map_kd",     TextureMapping{.key = Material::MATERIAL_TEXTURE_ALBEDO_MAP, .srgb = true}),
        std::make_pair("map_bump",   TextureMapping{.key = Material::MATERIAL_TEXTURE_NORMAL_MAP}),
        std::make_pair("bump",       TextureMapping{.key = Material::MATERIAL_TEXTURE_NORMAL_MAP}),
        std::make_pair("map_ka",     TextureMapping{.key = Material::MATERIAL_TEXTURE_METALNESS_MAP}),
        std::make_pair("map_ks",     TextureMapping{.key = Material::MATERIAL_TEXTURE_METALNESS_MAP}),
        std::make_pair("map_ns",     TextureMapping{.key = Material::MATERIAL_TEXTURE_ROUGHNESS_MAP}),
        std::make_pair("map_height", TextureMapping{.key = Material::MATERIAL_TEXTURE_PARALLAX_MAP}) /* custom */,
        std::make_pair("map_ao",     TextureMapping{.key = Material::MATERIAL_TEXTURE_AO_MAP})       /* custom */
    };

    Tokens tokens;
    tokens.reserve(5);

    auto splitter = [&tokens](const std::string &token) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    };

    state->stream.ReadLines([&](const std::string &line) {
        tokens.clear();

        const auto trimmed = StringUtil::Trim(line);

        if (trimmed.empty() || trimmed.front() == '#') {
            return;
        }

        StringUtil::SplitBuffered(trimmed, ' ', splitter);

        if (tokens.empty()) {
            return;
        }

        tokens[0] = StringUtil::ToLower(tokens[0]);

        if (tokens[0] == "newmtl") {
            std::string name = "default";

            if (tokens.size() >= 2) {
                name = tokens[1];
            } else {
                DebugLog(LogType::Warn, "Obj Mtl loader: material arg name missing\n");
            }

            AddMaterial(object, name);

            return;
        }

        if (tokens[0] == "kd") {
            auto color = ReadVector<Vector4>(tokens);

            if (tokens.size() < 5) {
                color.w = 1.0f;
            }

            LastMaterial(object).parameters[Material::MATERIAL_KEY_ALBEDO] = ParameterDef{
                .values = std::vector<float>(std::begin(color.values), std::end(color.values))
            };

            return;
        }

        if (tokens[0] == "ns") {
            if (tokens.size() < 2) {
                DebugLog(LogType::Warn, "Obj Mtl loader: spec value missing\n");

                return;
            }

            const auto spec = StringUtil::Parse<int>(tokens[1]);

            LastMaterial(object).parameters[Material::MATERIAL_KEY_ROUGHNESS] = ParameterDef{
                .values = {1.0f - MathUtil::Clamp(float(spec) / 1000.0f, 0.0f, 1.0f)}
            };

            return;
        }

        if (tokens[0] == "illum") {
            if (tokens.size() < 2) {
                DebugLog(LogType::Warn, "Obj Mtl loader: illum value missing\n");

                return;
            }

            const auto illum_model = StringUtil::Parse<int>(tokens[1]);

            LastMaterial(object).parameters[Material::MATERIAL_KEY_METALNESS] = ParameterDef{
                .values = {float(illum_model) / 9.0f} /* rough approx */
            };

            return;
        }

        const auto texture_it = texture_keys.find(tokens[0]);

        if (texture_it != texture_keys.end()) {
            std::string name;

            if (tokens.size() >= 2) {
                name = tokens[tokens.size() - 1];
            } else {
                DebugLog(LogType::Warn, "Obj Mtl loader: texture arg name missing\n");
            }

            LastMaterial(object).textures.push_back({
                .mapping = texture_it->second,
                .name = name
            });

            return;
        }

        DebugLog(LogType::Warn, "Obj Mtl loader: Unable to parse mtl material line: %s\n", trimmed.c_str());
    });

    return {};
}

std::unique_ptr<MaterialGroup> MTLMaterialLoader::BuildFn(Engine *engine, const Object &object)
{
    auto material_library = std::make_unique<MaterialGroup>();
    
    std::unordered_map<std::string, std::string> texture_names_to_path;

    for (const auto &item : object.materials) {
        for (const auto &it : item.textures) {
            const auto texture_path = FileSystem::Join(
                FileSystem::RelativePath(
                    StringUtil::BasePath(object.filepath),
                    FileSystem::CurrentPath()
                ),
                it.name
            );

            texture_names_to_path[it.name] = texture_path;
        }
    }
    
    std::vector<std::string> all_filepaths;
    std::vector<std::unique_ptr<Texture>> loaded_textures;

    if (!texture_names_to_path.empty()) {
        for (auto &it : texture_names_to_path) {
            all_filepaths.push_back(it.second);
        }

        loaded_textures = engine->assets.Load<Texture>(all_filepaths);
    }

    engine->resources->Lock([&](Resources &resources) {
        std::unordered_map<std::string, Handle<Texture>> texture_refs;

        for (size_t i = 0; i < loaded_textures.size(); i++) {
            if (loaded_textures[i] == nullptr) {
                texture_refs[all_filepaths[i]].Reset();

                continue;
            }

            texture_refs[all_filepaths[i]] = Handle<Texture>(loaded_textures[i].release());
        }

        for (auto &item : object.materials) {
            auto material = std::make_unique<Material>(item.tag.c_str());

            for (auto &it : item.parameters) {
                material->SetParameter(it.first, Material::Parameter(it.second.values.data(), it.second.values.size()));
            }

            for (auto &it : item.textures) {
                auto &texture = texture_refs[texture_names_to_path[it.name]];

                if (texture == nullptr) {
                    DebugLog(
                        LogType::Warn,
                        "Obj Mtl loader: Texture %s could not be used because it could not be loaded\n",
                        it.name.c_str()
                    );

                    continue;
                }

                texture->GetImage().SetIsSRGB(it.mapping.srgb);

                material->SetTexture(it.mapping.key, Handle<Texture>(texture));
            }

            material_library->Add(item.tag, Handle<Material>(material.release()));
        }
    });

    return std::move(material_library);
}

} // namespace hyperion::v2