/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/material_loaders/MTLMaterialLoader.hpp>

#include <core/logging/Logger.hpp>

#include <Engine.hpp>

#include <util/fs/FsUtil.hpp>

namespace hyperion {

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
static Vector ReadVector(const Tokens &tokens, uint32 offset = 1)
{
    Vector result { 0.0f };

    int value_index = 0;

    for (uint32 i = offset; i < tokens.Size(); i++) {
        const String &token = tokens[i];

        if (token.Empty()) {
            continue;
        }

        result.values[value_index++] = float(std::atof(token.Data()));

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
                HYP_LOG(Assets, LogLevel::WARNING, "OBJ material loader: material arg name missing");
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
                FixedArray<float, 4> { color[0], color[1], color[2], color[3] }
            };

            return;
        }

        if (tokens[0] == "ns") {
            if (tokens.Size() < 2) {
                HYP_LOG(Assets, LogLevel::WARNING, "OBJ material loader: spec value missing");

                return;
            }

            const auto spec = StringUtil::Parse<int>(tokens[1].Data());

            LastMaterial(library).parameters[Material::MATERIAL_KEY_ROUGHNESS] = ParameterDef {
                .values = { 1.0f - MathUtil::Clamp(float(spec) / 1000.0f, 0.0f, 1.0f) }
            };

            return;
        }

        if (tokens[0] == "illum") {
            if (tokens.Size() < 2) {
                HYP_LOG(Assets, LogLevel::WARNING, "OBJ material loader: illum value missing");

                return;
            }

            const auto illum_model = StringUtil::Parse<int>(tokens[1].Data());

            if (IsTransparencyModel(static_cast<IlluminationModel>(illum_model))) {
                LastMaterial(library).parameters[Material::MATERIAL_KEY_TRANSMISSION] = ParameterDef {
                    .values = FixedArray<float, 4> { 0.95f, 0.0f, 0.0f, 0.0f }
                };
                // TODO: Bucket, alpha blend
            }

            LastMaterial(library).parameters[Material::MATERIAL_KEY_METALNESS] = ParameterDef {
                .values = { float(illum_model) / 9.0f } /* rough approx */
            };

            return;
        }

        const auto texture_it = texture_keys.Find(tokens[0]);

        if (texture_it != texture_keys.end()) {
            String name;

            if (tokens.Size() >= 2) {
                name = tokens.Back();
            } else {
                HYP_LOG(Assets, LogLevel::WARNING, "OBJ material loader: texture arg name missing");
            }

            LastMaterial(library).textures.PushBack(TextureDef {
                .mapping = texture_it->second,
                .name = name
            });

            return;
        }

        HYP_LOG(Assets, LogLevel::WARNING, "OBJ material loader: Unable to parse mtl material line: {}", trimmed);
    });

    Handle<MaterialGroup> material_group_handle = CreateObject<MaterialGroup>();

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
            uint num_enqueued = 0;
            String paths_string;

            RC<AssetBatch> textures_batch = state.asset_manager->CreateBatch();
            
            for (auto &it : texture_names_to_path) {
                all_filepaths.PushBack(it.second);

                ++num_enqueued;
                textures_batch->Add(
                    it.first,
                    it.second
                );
                if (paths_string.Any()) {
                    paths_string += ", ";
                }

                paths_string += it.second;
            }

            if (num_enqueued != 0) {
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

            if (it.first == Material::MATERIAL_KEY_TRANSMISSION && it.second.values.Any([](float value) { return value > 0.0f; }))
            {
                attributes.blend_function = BlendFunction::AlphaBlending();
                attributes.bucket = BUCKET_TRANSLUCENT;
            }
        }

        for (auto &it : item.textures) {
            if (!loaded_textures[it.name].IsOK()) {
                HYP_LOG(Assets, LogLevel::WARNING, "OBJ material loader: Texture {} could not be used because it could not be loaded!", it.name);

                continue;
            }
            
            Handle<Texture> texture = loaded_textures[it.name].ExtractAs<Texture>();

            texture->GetImage()->SetIsSRGB(it.mapping.srgb);
            texture->GetImage()->SetMinFilterMode(it.mapping.filter_mode);
            texture->GetImage()->SetMagFilterMode(FilterMode::TEXTURE_FILTER_LINEAR);

            texture->SetName(CreateNameFromDynamicString(it.name.Data()));

            textures.Set(it.mapping.key, std::move(texture));
        }

        Handle<Material> material = g_material_system->GetOrCreate(
            attributes,
            parameters,
            textures
        );

        material_group_handle->Add(item.tag, std::move(material));
    }

    return { { LoaderResult::Status::OK }, material_group_handle };
}

} // namespace hyperion