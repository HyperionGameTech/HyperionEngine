#include "MTLMaterialLoader.hpp"
#include <Engine.hpp>

#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using Tokens = std::vector<std::string>;
using MaterialLibrary = MTLMaterialLoader::MaterialLibrary;
using TextureMapping = MaterialLibrary::TextureMapping;
using TextureDef = MaterialLibrary::TextureDef;
using ParameterDef = MaterialLibrary::ParameterDef;
using MaterialDef = MaterialLibrary::MaterialDef;

enum IlluminationModel
{
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
static Vector ReadVector(const Tokens &tokens, SizeType offset = 1)
{
    Vector result { 0.0f };

    int value_index = 0;

    for (SizeType i = offset; i < tokens.size(); i++) {
        const auto &token = tokens[i];

        if (token.empty()) {
            continue;
        }

        result.values[value_index++] = Float(std::atof(token.c_str()));

        if (value_index == std::size(result.values)) {
            break;
        }
    }

    return result;
}

static void AddMaterial(MaterialLibrary &library, const std::string &tag)
{
    std::string unique_tag(tag);
    int counter = 0;

    while (std::any_of(
        library.materials.begin(),
        library.materials.end(),
        [&unique_tag](const auto &mesh) { return mesh.tag == unique_tag; }
    )) {
        unique_tag = tag + std::to_string(++counter);
    }

    library.materials.push_back(MaterialDef {
        .tag = unique_tag
    });
}

static auto &LastMaterial(MaterialLibrary &library)
{
    if (library.materials.empty()) {
        AddMaterial(library, "default");
    }

    return library.materials.back();
}

static bool IsTransparencyModel(IlluminationModel illum_model)
{
    return illum_model == ILLUM_TRANSPARENT_GLASS_RAYTRACED
        || illum_model == ILLUM_TRANSPARENT_REFRACTION_RAYTRACED
        || illum_model == ILLUM_TRANSPARENT_FRESNEL_REFRACTION_RAYTRACED
        || illum_model == ILLUM_TRANSPARENT_REFLECTIVE_GLASS;
}

LoadedAsset MTLMaterialLoader::LoadAsset(LoaderState &state) const
{
    AssertThrow(state.asset_manager != nullptr);

    MaterialLibrary library;
    library.filepath = state.filepath;

    const std::unordered_map<std::string, TextureMapping> texture_keys {
        std::make_pair("map_kd", TextureMapping { .key = Material::MATERIAL_TEXTURE_ALBEDO_MAP, .srgb = true, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP }),
        std::make_pair("map_bump", TextureMapping { .key = Material::MATERIAL_TEXTURE_NORMAL_MAP, .srgb = false, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP }),
        std::make_pair("bump", TextureMapping { .key = Material::MATERIAL_TEXTURE_NORMAL_MAP, .srgb = false, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP }),
        std::make_pair("map_ka", TextureMapping { .key = Material::MATERIAL_TEXTURE_METALNESS_MAP, .srgb = false, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP }),
        std::make_pair("map_ks", TextureMapping { .key = Material::MATERIAL_TEXTURE_METALNESS_MAP, .srgb = false, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP }),
        std::make_pair("map_ns", TextureMapping { .key = Material::MATERIAL_TEXTURE_ROUGHNESS_MAP, .srgb = false, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP }),
        std::make_pair("map_height", TextureMapping { .key = Material::MATERIAL_TEXTURE_PARALLAX_MAP, .srgb = false, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP }) /* custom */,
        std::make_pair("map_ao", TextureMapping { .key = Material::MATERIAL_TEXTURE_AO_MAP, .srgb = false, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP })       /* custom */
    };

    Tokens tokens;
    tokens.reserve(5);

    auto splitter = [&tokens](const std::string &token) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    };

    state.stream.ReadLines([&](const String &line) {
        tokens.clear();

        const auto trimmed = StringUtil::Trim(std::string(line.Data()));

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

            AddMaterial(library, name);

            return;
        }

        if (tokens[0] == "kd") {
            auto color = ReadVector<Vector4>(tokens);

            if (tokens.size() < 5) {
                color.w = 1.0f;
            }

            LastMaterial(library).parameters[Material::MATERIAL_KEY_ALBEDO] = ParameterDef {
                FixedArray<Float, 4> { color[0], color[1], color[2], color[3] }
            };

            return;
        }

        if (tokens[0] == "ns") {
            if (tokens.size() < 2) {
                DebugLog(LogType::Warn, "Obj Mtl loader: spec value missing\n");

                return;
            }

            const auto spec = StringUtil::Parse<Int>(tokens[1]);

            LastMaterial(library).parameters[Material::MATERIAL_KEY_ROUGHNESS] = ParameterDef {
                .values = { 1.0f - MathUtil::Clamp(Float(spec) / 1000.0f, 0.0f, 1.0f) }
            };

            return;
        }

        if (tokens[0] == "illum") {
            if (tokens.size() < 2) {
                DebugLog(LogType::Warn, "Obj Mtl loader: illum value missing\n");

                return;
            }

            const auto illum_model = StringUtil::Parse<Int>(tokens[1]);

            if (IsTransparencyModel(static_cast<IlluminationModel>(illum_model))) {
                LastMaterial(library).parameters[Material::MATERIAL_KEY_TRANSMISSION] = ParameterDef {
                    .values = FixedArray<Float, 4> { 0.95f, 0.0f, 0.0f, 0.0f }
                };
                // TODO: Bucket, alpha blend
            }

            LastMaterial(library).parameters[Material::MATERIAL_KEY_METALNESS] = ParameterDef {
                .values = { Float(illum_model) / 9.0f } /* rough approx */
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

            LastMaterial(library).textures.push_back(TextureDef {
                .mapping = texture_it->second,
                .name = name
            });

            return;
        }

        DebugLog(LogType::Warn, "Obj Mtl loader: Unable to parse mtl material line: %s\n", trimmed.c_str());
    });

    auto material_group_handle = UniquePtr<Handle<MaterialGroup>>::Construct(CreateObject<MaterialGroup>());

    std::unordered_map<std::string, std::string> texture_names_to_path;
    FlatSet<String> unique_paths;

    for (const auto &item : library.materials) {
        for (const auto &it : item.textures) {
            const auto texture_path = FileSystem::Join(
                FileSystem::RelativePath(
                    StringUtil::BasePath(library.filepath),
                    FileSystem::CurrentPath()
                ),
                it.name
            );

            texture_names_to_path[it.name] = texture_path;
            unique_paths.Insert(texture_path.c_str());
        }
    }

    std::unordered_map<std::string, Handle<Texture>> texture_refs;
    
    AssetMap loaded_textures;
    std::vector<std::string> all_filepaths;

    {
        if (!texture_names_to_path.empty()) {
            UInt num_enqueued = 0;
            String paths_string;

            AssetBatch textures_batch = state.asset_manager->CreateBatch();

            // for (const String &str : unique_paths) {
            //     textures_batch.Add<Texture>(str, str);

            //     ++num_enqueued;

            //     if (paths_string.Any()) {
            //         paths_string += ", ";
            //     }

            //     paths_string += str;
            // }
            for (auto &it : texture_names_to_path) {
                all_filepaths.push_back(it.second);
                ++num_enqueued;
                textures_batch.Add<Texture>(
                    String(it.first.c_str()),
                    String(it.second.c_str())
                );
                if (paths_string.Any()) {
                    paths_string += ", ";
                }

                paths_string += it.second.data();
            }

            if (num_enqueued != 0) {
                DebugLog(
                    LogType::Info,
                    "Loading %u textures async:\n\t%s\n",
                    num_enqueued,
                    paths_string.Data()
                );

                textures_batch.LoadAsync();
                loaded_textures = textures_batch.AwaitResults();
            }
        }
    }

    for (auto &item : library.materials) {
        MaterialAttributes attributes;
        Material::ParameterTable parameters = Material::DefaultParameters();
        Material::TextureSet textures;

        for (auto &it : item.parameters) {
            parameters.Set(it.first, Material::Parameter(
                it.second.values.Data(),
                it.second.values.Size()
            ));

            if (it.first == Material::MATERIAL_KEY_TRANSMISSION && it.second.values.Any([](float value) { return value > 0.0f; })) {
                attributes.blend_mode = BlendMode::NORMAL;
                attributes.bucket = BUCKET_TRANSLUCENT;
            }
        }

        for (auto &it : item.textures) {
            // const auto &texture_path = texture_names_to_path[it.name];

            // if (texture_path.empty()) {
            //     DebugLog(
            //         LogType::Warn,
            //         "OBJ MTL loader: Texture %s could not be used because it was in the texture name -> path map!\n",
            //         it.name.c_str()
            //     );

            //     continue;
            // }

            // Handle<Texture> texture = loaded_textures[String(texture_path.c_str())].Get<Texture>();
            auto texture = loaded_textures[String(it.name.c_str())].Get<Texture>();

            if (!texture) {
                DebugLog(
                    LogType::Warn,
                    "OBJ MTL loader: Texture %s could not be used because it could not be loaded!\n",
                    it.name.c_str()
                );

                continue;
            }

            texture->GetImage()->SetIsSRGB(it.mapping.srgb);
            texture->GetImage()->SetFilterMode(it.mapping.filter_mode);

            texture->SetName(CreateNameFromDynamicString(it.name.c_str()));

            textures.Set(it.mapping.key, std::move(texture));
        }

        Handle<Material> material = Engine::Get()->GetMaterialCache().GetOrCreate(
            attributes,
            parameters,
            textures
        );

        (*material_group_handle)->Add(item.tag, std::move(material));
    }

    return { { LoaderResult::Status::OK }, std::move(material_group_handle) };
}

} // namespace hyperion::v2