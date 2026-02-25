/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <AssetPch.hpp>

#include <asset/material_loaders/MTLMaterialLoader.hpp>
#include <asset/Assets.hpp>
#include <asset/AssetBatch.hpp>
#include <asset/AssetRegistry.hpp>

#include <rendering/Texture.hpp>

#include <Core/filesystem/FsUtil.hpp>

#ifdef HYP_EDITOR
#include <editor/EditorState.hpp>
#include <editor/EditorProject.hpp>
#endif

#include <engine/EngineDriver.hpp>

#include <MTLMaterialLoader.generated.inl>

namespace Hyperion {

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
        Pair<String, TextureMapping> { "map_kd", TextureMapping { .key = MaterialTextureKey::Diffuse, .srgb = true, .filterMode = TFM_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "map_bump", TextureMapping { .key = MaterialTextureKey::Normals, .srgb = false, .filterMode = TFM_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "bump", TextureMapping { .key = MaterialTextureKey::Normals, .srgb = false, .filterMode = TFM_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "map_ka", TextureMapping { .key = MaterialTextureKey::Metalness, .srgb = false, .filterMode = TFM_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "map_ks", TextureMapping { .key = MaterialTextureKey::Metalness, .srgb = false, .filterMode = TFM_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "map_ns", TextureMapping { .key = MaterialTextureKey::Roughness, .srgb = false, .filterMode = TFM_LINEAR_MIPMAP } },
        Pair<String, TextureMapping> { "map_height", TextureMapping { .key = MaterialTextureKey::Parallax, .srgb = false, .filterMode = TFM_LINEAR_MIPMAP } }, /* custom */
        Pair<String, TextureMapping> { "map_ao", TextureMapping { .key = MaterialTextureKey::AmbientOcclusion, .srgb = false, .filterMode = TFM_LINEAR_MIPMAP } }            /* custom */
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
                Vec4f color = ReadVector<Vec4f>(tokens);

                if (tokens.Size() < 5)
                {
                    color.w = 1.0f;
                }

                LastMaterial(library).parameters[MATERIAL_KEY_ALBEDO] = ParameterDef {
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

            //     LastMaterial(library).parameters[MATERIAL_KEY_METALNESS] = ParameterDef {
            //         .values = { metalness }
            //     };

            //     return;
            // }

             /*! Ns exponent
 
              Specifies the specular exponent for the current material.  This defines 
              the focus of the specular highlight.
 
              "exponent" is the value for the specular exponent.  A high exponent 
              results in a tight, concentrated highlight.  Ns values normally range 
              from 0 to 1000. */
            if (tokens[0] == "ns")
            {
                if (tokens.Size() < 2)
                {
                    HYP_LOG(Assets, Warning, "OBJ material loader: spec value missing");

                    return;
                }

                const int spec = StringUtil::Parse<int>(tokens[1].Data());

                LastMaterial(library).parameters[MATERIAL_KEY_ROUGHNESS] = ParameterDef {
                    .values = { MathUtil::Sqrt(2.0f / (MathUtil::Clamp(float(spec) / 1000.0f, 0.0f, 1.0f) + 2.0f)) }
                };

                return;
            }
            
            /*! d factor
 
                Specifies the dissolve for the current material.
 
                "factor" is the amount this material dissolves into the background.  A 
            factor of 1.0 is fully opaque.  This is the default when a new material 
            is created.  A factor of 0.0 is fully dissolved (completely 
            transparent).
 
                Unlike a real transparent material, the dissolve does not depend upon 
            material thickness nor does it have any spectral character.  Dissolve 
            works on all illumination models.
 
                d -halo factor
 
                Specifies that a dissolve is dependent on the surface orientation 
            relative to the viewer.  For example, a sphere with the following 
            dissolve, d -halo 0.0, will be fully dissolved at its center and will 
            appear gradually more opaque toward its edge.
 
                "factor" is the minimum amount of dissolve applied to the material.  
            The amount of dissolve will vary between 1.0 (fully opaque) and the 
            specified "factor".  The formula is:
 
                dissolve = 1.0 - (N*v)(1.0-factor) */
            if (tokens[0] == "d")
            {
                if (tokens.Size() < 2)
                {
                    HYP_LOG(Assets, Warning, "OBJ material loader: disolve value missing");

                    return;
                }

                uint32 valueIndex = 1;

                if (tokens.Size() >= 3 && tokens[1].ToLower() == "-halo")
                {
                    valueIndex = 2;
                }

                if (valueIndex >= tokens.Size())
                {
                    HYP_LOG(Assets, Warning, "OBJ material loader: disolve value missing");

                    return;
                }

                const float dissolve = MathUtil::Clamp(StringUtil::Parse<float>(tokens[valueIndex].Data()), 0.0f, 1.0f);

                auto &material = LastMaterial(library);
                Vec4f albedo(1.0f, 1.0f, 1.0f, 1.0f);

                if (auto it = material.parameters.Find(MATERIAL_KEY_ALBEDO); it != material.parameters.end())
                {
                    albedo = Vec4f(
                        it->second.values[0],
                        it->second.values[1],
                        it->second.values[2],
                        it->second.values[3]);
                }

                albedo.w = dissolve;

                material.parameters[MATERIAL_KEY_ALBEDO] = ParameterDef {
                    FixedArray<float, 4> { albedo[0], albedo[1], albedo[2], albedo[3] }
                };

                return;
            }

            if (tokens[0] == "tr")
            {
                if (tokens.Size() < 2)
                {
                    HYP_LOG(Assets, Warning, "OBJ material loader: transparency value missing");

                    return;
                }

                uint32 valueIndex = 1;

                if (tokens.Size() >= 3 && tokens[1].ToLower() == "-halo")
                {
                    valueIndex = 2;
                }

                if (valueIndex >= tokens.Size())
                {
                    HYP_LOG(Assets, Warning, "OBJ material loader: transparency value missing");

                    return;
                }

                const float transparency = MathUtil::Clamp(StringUtil::Parse<float>(tokens[valueIndex].Data()), 0.0f, 1.0f);
                const float dissolve = 1.0f - transparency;

                auto &material = LastMaterial(library);
                Vec4f albedo(1.0f, 1.0f, 1.0f, 1.0f);

                if (auto it = material.parameters.Find(MATERIAL_KEY_ALBEDO); it != material.parameters.end())
                {
                    albedo = Vec4f(
                        it->second.values[0],
                        it->second.values[1],
                        it->second.values[2],
                        it->second.values[3]);
                }

                albedo.w = dissolve;

                material.parameters[MATERIAL_KEY_ALBEDO] = ParameterDef {
                    FixedArray<float, 4> { albedo[0], albedo[1], albedo[2], albedo[3] }
                };

                return;
            }


            /*! illum illum_#
 
            The "illum" statement specifies the illumination model to use in the 
            material.  Illumination models are mathematical equations that represent 
            various material lighting and shading effects.
            
            "illum_#"can be a number from 0 to 10.  The illumination models are 
            summarized below; for complete descriptions see "Illumination models" on 
            page 5-30.
            
            Illumination    Properties that are turned on in the 
            model           Property Editor
            
            0		Color on and Ambient off
            1		Color on and Ambient on
            2		Highlight on
            3		Reflection on and Ray trace on
            4		Transparency: Glass on
                    Reflection: Ray trace on
            5		Reflection: Fresnel on and Ray trace on
            6		Transparency: Refraction on
                    Reflection: Fresnel off and Ray trace on
            7		Transparency: Refraction on
                    Reflection: Fresnel on and Ray trace on
            8		Reflection on and Ray trace off
            9		Transparency: Glass on
                    Reflection: Ray trace off
            10		Casts shadows onto invisible surfaces */

            if (tokens[0] == "illum")
            {
                if (tokens.Size() < 2)
                {
                    HYP_LOG(Assets, Warning, "OBJ material loader: illum value missing");

                    return;
                }

                const int illumModelValue = StringUtil::Parse<int>(tokens[1].Data());
                const int clampedValue = MathUtil::Clamp(illumModelValue, 0, 10);
                const IlluminationModel illumModel = static_cast<IlluminationModel>(clampedValue);

                if (IsTransparencyModel(illumModel))
                {
                    LastMaterial(library).parameters[MATERIAL_KEY_TRANSMISSION] = ParameterDef {
                        .values = FixedArray<float, 4> { 0.95f, 0.0f, 0.0f, 0.0f }
                    };
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
                    .name = name
                });

                return;
            }
        });

    Handle<MaterialGroup> materialGroupHandle = MakeHandle<MaterialGroup>();

    HashMap<String, String> textureNamesToPath;

    for (const auto& item : library.materials)
    {
        for (const auto& it : item.textures)
        {
            const FilePath texturePath = FilePath::Join(
                FilePath::Relative(FilePath(library.filepath).BasePath(), FilePath::Current()),
                it.name);

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

            AssetBatch* texturesBatch = state.assetManager->CreateBatch(state.batchIdentifier);

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
            else
            {
                // delete if none enqueued, otherwise AssetManager will delete it after its completed
                delete texturesBatch;
            }
        }
    }

    for (auto& item : library.materials)
    {
        MaterialAttributes attributes;
        /*attributes.blendFunction = BlendFunction::AlphaBlending();
        attributes.bucket = RB_TRANSLUCENT;*/
        attributes.flags |= MAF_ALPHA_DISCARD;

        MaterialParameters parameters = Material::DefaultParameters();
        parameters[MATERIAL_KEY_ALPHA_THRESHOLD] = 0.1f;

        for (auto& it : item.parameters)
        {
            parameters[it.first] = MaterialParameter(it.second.values.Data(), it.second.values.Size());

            if (it.first == MATERIAL_KEY_TRANSMISSION && AnyOf(it.second.values, [](float value)
                    {
                        return value > 0.0f;
                    }))
            {
                attributes.blendFunction = BlendFunction::AlphaBlending();
                attributes.bucket = RB_TRANSLUCENT;
            }
        }

        // if (auto it = item.parameters.Find(MATERIAL_KEY_ALBEDO); it != item.parameters.end())
        // {
        //     if (it->second.values[3] < 1.0f)
        //     {
        //         attributes.blendFunction = BlendFunction::AlphaBlending();
        //         attributes.bucket = RB_TRANSLUCENT;
        //     }
        // }

        MaterialTextures textures;

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
            textureDesc.wrapMode = TWM_REPEAT;

            if (it.mapping.srgb)
            {
                textureDesc.format = TextureUtils::ChangeFormatSRGB(textureDesc.format, /* useSRGB */ true);
            }

            texture->SetTextureDesc(textureDesc);

            textures[it.mapping.key] = std::move(texture);
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

} // namespace Hyperion
