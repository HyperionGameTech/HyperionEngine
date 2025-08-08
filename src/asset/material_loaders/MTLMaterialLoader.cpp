/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <asset/material_loaders/MTLMaterialLoader.hpp>
#include <asset/AssetBatch.hpp>
#include <asset/AssetRegistry.hpp>
#include <asset/TextureAsset.hpp>

#include <rendering/Texture.hpp>

#include <core/logging/Logger.hpp>

#include <core/filesystem/FsUtil.hpp>

#ifdef HYP_EDITOR
#include <editor/EditorState.hpp>
#include <editor/EditorProject.hpp>
#endif

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

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
static Vector ReadVector(const Tokens& tokens, uint32 offset = 1)
{
    Vector result { 0.0f };

    int valueIndex = 0;

    for (uint32 i = offset; i < tokens.Size(); i++)
    {
        const String& token = tokens[i];

        if (token.Empty())
        {
            continue;
        }

        result.values[valueIndex++] = float(std::atof(token.Data()));

        if (valueIndex == std::size(result.values))
        {
            break;
        }
    }

    return result;
}

static void AddMaterial(MaterialLibrary& library, const String& tag)
{
    String uniqueTag(tag);
    int counter = 0;

    while (AnyOf(library.materials, [&uniqueTag](const MaterialDef& materialDef)
        {
            return materialDef.tag == uniqueTag;
        }))
    {
        uniqueTag = tag + String::ToString(++counter);
    }

    library.materials.PushBack(MaterialDef { .tag = uniqueTag });
}

static auto& LastMaterial(MaterialLibrary& library)
{
    if (library.materials.Empty())
    {
        AddMaterial(library, "default");
    }

    return library.materials.Back();
}

static bool IsTransparencyModel(IlluminationModel illumModel)
{
    return illumModel == ILLUM_TRANSPARENT_GLASS_RAYTRACED
        || illumModel == ILLUM_TRANSPARENT_REFRACTION_RAYTRACED
        || illumModel == ILLUM_TRANSPARENT_FRESNEL_REFRACTION_RAYTRACED
        || illumModel == ILLUM_TRANSPARENT_REFLECTIVE_GLASS;
}

AssetLoadResult MTLMaterialLoader::LoadAsset(LoaderState& state) const
{
    Assert(state.assetManager != nullptr);

    MaterialLibrary library;
    library.filepath = state.filepath;

    const FlatMap<String, TextureMapping> textureKeys {
        Pair<String, TextureMapping> { "map_kd", TextureMapping { .key = MaterialTextureKey::ALBEDO_MAP, .srgb = true, .filterMode = TFM_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "map_bump", TextureMapping { .key = MaterialTextureKey::NORMAL_MAP, .srgb = false, .filterMode = TFM_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "bump", TextureMapping { .key = MaterialTextureKey::NORMAL_MAP, .srgb = false, .filterMode = TFM_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "map_ka", TextureMapping { .key = MaterialTextureKey::METALNESS_MAP, .srgb = false, .filterMode = TFM_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "map_ks", TextureMapping { .key = MaterialTextureKey::METALNESS_MAP, .srgb = false, .filterMode = TFM_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "map_ns", TextureMapping { .key = MaterialTextureKey::ROUGHNESS_MAP, .srgb = false, .filterMode = TFM_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "map_height", TextureMapping { .key = MaterialTextureKey::PARALLAX_MAP, .srgb = false, .filterMode = TFM_LINEAR_MIPMAP } }, /* custom */
        Pair<String, TextureMapping> { "map_ao", TextureMapping { .key = MaterialTextureKey::AO_MAP, .srgb = false, .filterMode = TFM_LINEAR_MIPMAP } }            /* custom */
    };

    Tokens tokens;
    tokens.Reserve(5);

    state.stream.ReadLines([&](const String& line, bool*)
        {
            tokens.Clear();

            const auto trimmed = line.Trimmed();

            if (trimmed.Empty() || trimmed.Front() == '#')
            {
                return;
            }

            tokens = trimmed.Split(' ');

            if (tokens.Empty())
            {
                return;
            }

            tokens[0] = tokens[0].ToLower();

            if (tokens[0] == "newmtl")
            {
                String name = "default";

                if (tokens.Size() >= 2)
                {
                    name = tokens[1];
                }
                else
                {
                    HYP_LOG(Assets, Warning, "OBJ material loader: material arg name missing");
                }

                AddMaterial(library, name);

                return;
            }

            if (tokens[0] == "kd")
            {
                auto color = ReadVector<Vector4>(tokens);

                if (tokens.Size() < 5)
                {
                    color.w = 1.0f;
                }

                LastMaterial(library).parameters[Material::MATERIAL_KEY_ALBEDO] = ParameterDef {
                    FixedArray<float, 4> { color[0], color[1], color[2], color[3] }
                };

                return;
            }

            // if (tokens[0] == "ka") {
            //     if (tokens.Size() < 2) {
            //         HYP_LOG(Assets, Warning, "OBJ material loader: metalness value missing");

            //         return;
            //     }

            //     const float metalness = StringUtil::Parse<float>(tokens[1].Data());

            //     LastMaterial(library).parameters[Material::MATERIAL_KEY_METALNESS] = ParameterDef {
            //         .values = { metalness }
            //     };

            //     return;
            // }

            if (tokens[0] == "ns")
            {
                if (tokens.Size() < 2)
                {
                    HYP_LOG(Assets, Warning, "OBJ material loader: spec value missing");

                    return;
                }

                const int spec = StringUtil::Parse<int>(tokens[1].Data());

                LastMaterial(library).parameters[Material::MATERIAL_KEY_ROUGHNESS] = ParameterDef {
                    .values = { MathUtil::Sqrt(2.0f / (MathUtil::Clamp(float(spec) / 1000.0f, 0.0f, 1.0f) + 2.0f)) }
                };

                return;
            }

            if (tokens[0] == "illum")
            {
                if (tokens.Size() < 2)
                {
                    HYP_LOG(Assets, Warning, "OBJ material loader: illum value missing");

                    return;
                }

                const int illumModel = StringUtil::Parse<int>(tokens[1].Data());

                if (IsTransparencyModel(static_cast<IlluminationModel>(illumModel)))
                {
                    LastMaterial(library).parameters[Material::MATERIAL_KEY_TRANSMISSION] = ParameterDef {
                        .values = FixedArray<float, 4> { 0.95f, 0.0f, 0.0f, 0.0f }
                    };
                    // TODO: Bucket, alpha blend
                }

                return;
            }

            const auto textureIt = textureKeys.Find(tokens[0]);

            if (textureIt != textureKeys.end())
            {
                String name;

                if (tokens.Size() >= 2)
                {
                    name = tokens.Back();
                }
                else
                {
                    HYP_LOG(Assets, Warning, "OBJ material loader: texture arg name missing");
                }

                LastMaterial(library).textures.PushBack(TextureDef {
                    .mapping = textureIt->second,
                    .name = name });

                return;
            }
        });

    Handle<MaterialGroup> materialGroupHandle = CreateObject<MaterialGroup>();

    HashMap<String, String> textureNamesToPath;

    for (const auto& item : library.materials)
    {
        for (const auto& it : item.textures)
        {
            const auto texturePath = String(FileSystem::Join(
                FileSystem::RelativePath(
                    StringUtil::BasePath(library.filepath.Data()),
                    FileSystem::CurrentPath()),
                it.name.Data())
                    .c_str());

            textureNamesToPath[it.name] = texturePath;
        }
    }

    HashMap<String, Handle<Texture>> textureRefs;

    AssetMap loadedTextures;
    Array<String> allFilepaths;

    {
        if (!textureNamesToPath.Empty())
        {
            uint32 numEnqueued = 0;
            String pathsString;

            RC<AssetBatch> texturesBatch = state.assetManager->CreateBatch(state.batchIdentifier);

            for (auto& it : textureNamesToPath)
            {
                allFilepaths.PushBack(it.second);

                ++numEnqueued;
                
                texturesBatch->Add(it.first, it.second);
                
                if (pathsString.Any())
                {
                    pathsString += ", ";
                }

                pathsString += it.second;
            }

            if (numEnqueued != 0)
            {
                texturesBatch->LoadAsync();
                loadedTextures = texturesBatch->AwaitResults();
            }
        }
    }

    for (auto& item : library.materials)
    {
        MaterialAttributes attributes;
        Material::ParameterTable parameters = Material::DefaultParameters();
        Material::TextureSet textures;

        for (auto& it : item.parameters)
        {
            parameters.Set(it.first, Material::Parameter(it.second.values.Data(), it.second.values.Size()));

            if (it.first == Material::MATERIAL_KEY_TRANSMISSION && AnyOf(it.second.values, [](float value)
                    {
                        return value > 0.0f;
                    }))
            {
                attributes.blendFunction = BlendFunction::AlphaBlending();
                attributes.bucket = RB_TRANSLUCENT;
            }
        }

        for (auto& it : item.textures)
        {
            if (!loadedTextures[it.name].IsValid())
            {
                HYP_LOG(Assets, Warning, "OBJ material loader: Texture {} could not be used because it could not be loaded!", it.name);

                continue;
            }

            Handle<Texture> texture = loadedTextures[it.name].ExtractAs<Texture>();

            if (it.name.Any())
            {
                texture->SetName(CreateNameFromDynamicString(it.name.Split('/', '\\').Back()));
            }

            TextureDesc textureDesc = texture->GetTextureDesc();
            textureDesc.filterModeMin = it.mapping.filterMode;
            textureDesc.filterModeMag = TFM_LINEAR;

            if (it.mapping.srgb)
            {
                textureDesc.format = ChangeFormatSrgb(textureDesc.format, /* makeSrgb */ true);
            }

            texture->SetTextureDesc(textureDesc);

            textures.Set(it.mapping.key, std::move(texture));
        }

        Handle<Material> material = MaterialCache::GetInstance()->GetOrCreate(
            CreateNameFromDynamicString(item.tag),
            attributes,
            parameters,
            textures);

        materialGroupHandle->Add(item.tag, std::move(material));
    }

    return LoadedAsset { materialGroupHandle };
}

} // namespace hyperion
