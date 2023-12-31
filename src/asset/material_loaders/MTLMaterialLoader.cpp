#include "MTLMaterialLoader.hpp"
#include <Engine.hpp>

#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using Tokens = Array<String>;
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

    for (SizeType i = offset; i < tokens.Size(); i++) {
        const String &token = tokens[i];

        if (token.Empty()) {
            continue;
        }

        result.values[value_index++] = Float(std::atof(token.Data()));

        if (value_index == std::size(result.values)) {
            break;
        }
    }

    return result;
}

static void AddMaterial(MaterialLibrary &library, const String &tag)
{
    String unique_tag(tag);
    int counter = 0;

    while (library.materials.Any([&unique_tag](const MaterialDef &material_def) { return material_def.tag == unique_tag; })) {
        unique_tag = tag + String::ToString(++counter);
    }

    library.materials.PushBack(MaterialDef { .tag = unique_tag });
}

static auto &LastMaterial(MaterialLibrary &library)
{
    if (library.materials.Empty()) {
        AddMaterial(library, "default");
    }

    return library.materials.Back();
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

    const FlatMap<String, TextureMapping> texture_keys {
        Pair<String, TextureMapping> { "map_kd", TextureMapping { .key = Material::MATERIAL_TEXTURE_ALBEDO_MAP, .srgb = true, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "map_bump", TextureMapping { .key = Material::MATERIAL_TEXTURE_NORMAL_MAP, .srgb = false, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "bump", TextureMapping { .key = Material::MATERIAL_TEXTURE_NORMAL_MAP, .srgb = false, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "map_ka", TextureMapping { .key = Material::MATERIAL_TEXTURE_METALNESS_MAP, .srgb = false, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "map_ks", TextureMapping { .key = Material::MATERIAL_TEXTURE_METALNESS_MAP, .srgb = false, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "map_ns", TextureMapping { .key = Material::MATERIAL_TEXTURE_ROUGHNESS_MAP, .srgb = false, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "map_height", TextureMapping { .key = Material::MATERIAL_TEXTURE_PARALLAX_MAP, .srgb = false, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP } }, /* custom */
        Pair<String, TextureMapping> { "map_ao", TextureMapping { .key = Material::MATERIAL_TEXTURE_AO_MAP, .srgb = false, .filter_mode = FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP } }       /* custom */
    };

    Tokens tokens;
    tokens.Reserve(5);
    
    state.stream.ReadLines([&](const String &line, bool *) {
        tokens.Clear();

        const auto trimmed = line.Trimmed();

        if (trimmed.Empty() || trimmed.Front() == '#') {
            return;
        }

        tokens = trimmed.Split(' ');
        
        if (tokens.Empty()) {
            return;
        }

        tokens[0] = tokens[0].ToLower();

        if (tokens[0] == "newmtl") {
            String name = "default";

            if (tokens.Size() >= 2) {
                name = tokens[1];
            } else {
                DebugLog(LogType::Warn, "Obj Mtl loader: material arg name missing\n");
            }

            AddMaterial(library, name);

            return;
        }

        if (tokens[0] == "kd") {
            auto color = ReadVector<Vector4>(tokens);

            if (tokens.Size() < 5) {
                color.w = 1.0f;
            }

            LastMaterial(library).parameters[Material::MATERIAL_KEY_ALBEDO] = ParameterDef {
                FixedArray<Float, 4> { color[0], color[1], color[2], color[3] }
            };

            return;
        }

        if (tokens[0] == "ns") {
            if (tokens.Size() < 2) {
                DebugLog(LogType::Warn, "Obj Mtl loader: spec value missing\n");

                return;
            }

            const auto spec = StringUtil::Parse<Int>(tokens[1].Data());

            LastMaterial(library).parameters[Material::MATERIAL_KEY_ROUGHNESS] = ParameterDef {
                .values = { 1.0f - MathUtil::Clamp(Float(spec) / 1000.0f, 0.0f, 1.0f) }
            };

            return;
        }

        if (tokens[0] == "illum") {
            if (tokens.Size() < 2) {
                DebugLog(LogType::Warn, "Obj Mtl loader: illum value missing\n");

                return;
            }

            const auto illum_model = StringUtil::Parse<Int>(tokens[1].Data());

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

        const auto texture_it = texture_keys.Find(tokens[0]);

        if (texture_it != texture_keys.end()) {
            String name;

            if (tokens.Size() >= 2) {
                name = tokens.Back();
            } else {
                DebugLog(LogType::Warn, "Obj Mtl loader: texture arg name missing\n");
            }

            LastMaterial(library).textures.PushBack(TextureDef {
                .mapping = texture_it->second,
                .name = name
            });

            return;
        }

        DebugLog(LogType::Warn, "Obj Mtl loader: Unable to parse mtl material line: %s\n", trimmed.Data());
    });

    auto material_group_handle = UniquePtr<Handle<MaterialGroup>>::Construct(CreateObject<MaterialGroup>());

    HashMap<String, String> texture_names_to_path;
    FlatSet<String> unique_paths;

    for (const auto &item : library.materials) {
        for (const auto &it : item.textures) {
            const auto texture_path = String(FileSystem::Join(
                FileSystem::RelativePath(
                    StringUtil::BasePath(library.filepath.Data()),
                    FileSystem::CurrentPath()
                ),
                it.name.Data()
            ).c_str());

            texture_names_to_path[it.name] = texture_path;
            unique_paths.Insert(texture_path);
        }
    }

    HashMap<String, Handle<Texture>> texture_refs;
    
    AssetMap loaded_textures;
    Array<String> all_filepaths;
    
    {
        if (!texture_names_to_path.Empty()) {
            UInt num_enqueued = 0;
            String paths_string;

            auto textures_batch = state.asset_manager->CreateBatch();
            
            for (auto &it : texture_names_to_path) {
                all_filepaths.PushBack(it.second);

                ++num_enqueued;
                textures_batch->Add<Texture>(
                    it.first,
                    it.second
                );
                if (paths_string.Any()) {
                    paths_string += ", ";
                }

                paths_string += it.second;
            }

            if (num_enqueued != 0) {
                DebugLog(
                    LogType::Info,
                    "Loading %u textures async:\n\t%s\n",
                    num_enqueued,
                    paths_string.Data()
                );

                textures_batch->LoadAsync();
                loaded_textures = textures_batch->AwaitResults();
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
            // Handle<Texture> texture = loaded_textures[String(texture_path.c_str())].Get<Texture>();
            auto texture = loaded_textures[it.name].ExtractAs<Texture>();

            if (!texture) {
                DebugLog(
                    LogType::Warn,
                    "OBJ MTL loader: Texture %s could not be used because it could not be loaded!\n",
                    it.name.Data()
                );

                continue;
            }

            texture->GetImage()->SetIsSRGB(it.mapping.srgb);
            texture->GetImage()->SetFilterMode(it.mapping.filter_mode);

            texture->SetName(CreateNameFromDynamicString(it.name.Data()));

            textures.Set(it.mapping.key, std::move(texture));
        }

        Handle<Material> material = g_material_system->GetOrCreate(
            attributes,
            parameters,
            textures
        );

        (*material_group_handle)->Add(item.tag, std::move(material));
    }

    return { { LoaderResult::Status::OK }, material_group_handle.Cast<void>() };
}

} // namespace hyperion::v2