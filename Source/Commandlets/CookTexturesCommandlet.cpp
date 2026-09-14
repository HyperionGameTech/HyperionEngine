/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 *
 *  Usage: CookTexturesCommandlet [--input-dir=Data/Textures]
 *    Imports source textures (png/jpg/tga/...) into the engine asset registry and
 *    saves them out as cooked assets (.hmf manifests + blob storage) so the
 *    original files never have to ship with the engine.
 */

#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Core/CLI/CommandLine.hpp>
#include <Core/Core.hpp>
#include <Core/FileSystem/FilePath.hpp>
#include <Core/Utilities/GlobalContext.hpp>
#include <Core/Utilities/StringUtil.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Rendering/Texture.hpp>

namespace Hyperion {

static const char* s_cookedTextureExtensions[] = {
    "png", "jpg", "jpeg", "tga", "bmp", "psd", "gif", "hdr", "tif"
};

static bool IsLinearTextureName(const String& lowercaseStem)
{
    const char* keywords[] = {
        "normal", "rough", "metal", "ambient", "_ao", "height", "mask", "splat"
    };

    for (const char* keyword : keywords)
    {
        if (lowercaseStem.Contains(keyword))
        {
            return true;
        }
    }

    return false;
}

struct TerrainLayerPackSpec
{
    uint32 layerIndex;
    const char* colorFile;
    const char* normalFile;
    const char* aoFile;
    const char* heightFile;
    bool flipNormalGreen;
};

static Handle<Texture> LoadPackSourceTexture(const FilePath& inputDir, const char* filename, AssetLoadHint hint)
{
    if (filename == nullptr)
    {
        return Handle<Texture>::Null();
    }

    const FilePath path = inputDir / filename;

    if (!path.Exists())
    {
        HYP_LOG(Assets, Warning, "Terrain pack source '{}' not found, using neutral value", filename);

        return Handle<Texture>::Null();
    }

    auto loadResult = g_assetManager->Load<Texture>(path, String::empty, hint);

    if (loadResult.HasError())
    {
        HYP_LOG(Assets, Warning, "Failed to load terrain pack source '{}': {}", filename, loadResult.GetError().GetMessage());

        return Handle<Texture>::Null();
    }

    return loadResult.GetValue().ExtractAs<Handle<Texture>>();
}

static void PackTerrainLayerTextures(const FilePath& inputDir, Handle<AssetRegistry> registry)
{
    static const TerrainLayerPackSpec s_specs[] = {
        { 0, "patchy-meadow1_albedo.png", "patchy-meadow1_normal-ogl.png", "patchy-meadow1_ao.png", "patchy-meadow1_height.png", false },
        { 2, "dirtwithrocks_Base_Color.png", "dirtwithrocks_Normal-dx.png", "dirtwithrocks_Ambient_Occlusion.png", "dirtwithrocks_Height.png", true }
    };

    for (const TerrainLayerPackSpec& spec : s_specs)
    {
        Handle<Texture> color = LoadPackSourceTexture(inputDir, spec.colorFile, AssetLoadHint::TextureLoader_LoadAsSRGB);
        Handle<Texture> normal = LoadPackSourceTexture(inputDir, spec.normalFile, AssetLoadHint::NoHint);

        if (!color.IsValid() || !normal.IsValid())
        {
            HYP_LOG(Assets, Warning, "Skipping terrain layer {} pack (missing color/normal source)", spec.layerIndex);

            continue;
        }

        // AO / height are optional; missing files stay neutral white
        Handle<Texture> aoMask = LoadPackSourceTexture(inputDir, spec.aoFile, AssetLoadHint::NoHint);
        Handle<Texture> heightMask = LoadPackSourceTexture(inputDir, spec.heightFile, AssetLoadHint::NoHint);

        const uint32 width = color->GetExtent().x;
        const uint32 height = color->GetExtent().y;

        ByteBuffer albedoBytes(size_t(width) * size_t(height) * 4u);
        ByteBuffer normalBytes(size_t(width) * size_t(height) * 4u);

        ubyte* albedoDst = albedoBytes.Data();
        ubyte* normalDst = normalBytes.Data();

        for (uint32 y = 0; y < height; y++)
        {
            for (uint32 x = 0; x < width; x++)
            {
                const Vec2f uv { (float(x) + 0.5f) / float(width), (float(y) + 0.5f) / float(height) };

                // nearest-sampled; sizes may differ between sources, mip chain smooths the rest
                const Vec4f c = color->Sample2D(uv);  // linear (sRGB decoded)
                const Vec4f n = normal->Sample2D(uv); // linear
                const float ao = aoMask.IsValid() ? aoMask->Sample2D(uv).x : 1.0f;
                const float h = heightMask.IsValid() ? heightMask->Sample2D(uv).x : 1.0f;

                const float g = spec.flipNormalGreen ? 1.0f - n.y : n.y;

                const size_t i = (size_t(y) * size_t(width) + size_t(x)) * 4u;

                // engine decodes sRGB with pow 2.2, so encode back the same way (alpha stays linear)
                albedoDst[i + 0] = ubyte(MathUtil::Clamp(MathUtil::Pow(c.x, 1.0f / 2.2f), 0.0f, 1.0f) * 255.0f);
                albedoDst[i + 1] = ubyte(MathUtil::Clamp(MathUtil::Pow(c.y, 1.0f / 2.2f), 0.0f, 1.0f) * 255.0f);
                albedoDst[i + 2] = ubyte(MathUtil::Clamp(MathUtil::Pow(c.z, 1.0f / 2.2f), 0.0f, 1.0f) * 255.0f);
                albedoDst[i + 3] = ubyte(MathUtil::Clamp(ao, 0.0f, 1.0f) * 255.0f);

                normalDst[i + 0] = ubyte(MathUtil::Clamp(n.x, 0.0f, 1.0f) * 255.0f);
                normalDst[i + 1] = ubyte(MathUtil::Clamp(g, 0.0f, 1.0f) * 255.0f);
                normalDst[i + 2] = ubyte(MathUtil::Clamp(n.z, 0.0f, 1.0f) * 255.0f);
                normalDst[i + 3] = ubyte(MathUtil::Clamp(h, 0.0f, 1.0f) * 255.0f);
            }
        }

        const Name albedoName = NAME_FMT("Terrain_Layer{}_Albedo", spec.layerIndex);
        const Name normalName = NAME_FMT("Terrain_Layer{}_Normal", spec.layerIndex);

        TextureDesc albedoDesc {
            TextureType::Texture2D,
            TextureFormat::RGBA8_SRGB,
            Vec3u { width, height, 1 },
            TextureFilterMode::LinearMipmap,
            TextureFilterMode::Linear,
            TextureWrapMode::Repeat
        };

        Texture::GenerateMipmaps(albedoDesc, albedoBytes);

        Handle<Texture> albedoTexture = MakeHandle<Texture>(albedoDesc, albedoBytes.ToByteView());
        albedoTexture->SetName(albedoName);

        TextureDesc packedNormalDesc {
            TextureType::Texture2D,
            TextureFormat::RGBA8,
            Vec3u { width, height, 1 },
            TextureFilterMode::LinearMipmap,
            TextureFilterMode::Linear,
            TextureWrapMode::Repeat
        };

        Texture::GenerateMipmaps(packedNormalDesc, normalBytes);

        Handle<Texture> normalTexture = MakeHandle<Texture>(packedNormalDesc, normalBytes.ToByteView());
        normalTexture->SetName(normalName);

        // replace the generic-cooked assets from the loop above (same names)
        registry->RemoveAsset(AssetBuckets::Textures, albedoName);
        registry->RemoveAsset(AssetBuckets::Textures, normalName);

        registry->PutAsset(albedoTexture);
        registry->PutAsset(normalTexture);

        HYP_LOG(Assets, Info, "Packed terrain layer {} ({}x{}, ao: {}, height: {})",
            spec.layerIndex, width, height,
            aoMask.IsValid() ? "yes" : "neutral",
            heightMask.IsValid() ? "yes" : "neutral");
    }
}

class CookTexturesCommandlet : public CommandletBase
{
    HYP_OBJECT_BODY(CookTexturesCommandlet);

public:
    virtual ~CookTexturesCommandlet() override = default;

    HYP_METHOD()
    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;
        static bool s_initialized = false;

        if (!s_initialized)
        {
            s_initialized = true;

            s_definitions.Add(
                "input-dir",
                "i",
                "Directory containing source textures to cook",
                CommandLineArgumentFlags::NONE,
                CommandLineArgumentType::STRING,
                JSON::Value("Data/Textures"));
        }

        return s_definitions;
    }

protected:
    virtual Result Run(const CommandLineArguments& args) override
    {
        FilePath inputDir = FilePath(args["input-dir"].ToString());

        if (inputDir.Empty())
        {
            inputDir = "Data/Textures";
        }

        if (!inputDir.IsAbsolute())
        {
            inputDir = CoreApi::GetBaseDirectory() / inputDir;
        }

        HYP_LOG(Assets, Info, "CookTexturesCommandlet running (input dir: {})", inputDir);

        if (!inputDir.Exists() || !inputDir.IsDirectory())
        {
            return HYP_MAKE_ERROR(Error, "Input directory does not exist: {}", inputDir);
        }

        Handle<AssetRegistry> registry = GetEngineAssetRegistry();

        if (!registry.IsValid())
        {
            registry = MakeHandle<AssetRegistry>(
                AssetRegistryId::Engine,
                EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Engine")>());

            registry->Initialize(nullptr);

            // set globally so Engine-id assets can resolve their owning registry for blob persistence
            SetEngineAssetRegistry(registry);
        }

        GlobalContextScope contextScope { AssetRegistryContext { registry } };

        uint32 numCooked = 0;
        uint32 numFailed = 0;

        for (const FilePath& filepath : inputDir.GetAllFilesInDirectory())
        {
            const String extensionLower = filepath.GetExtension().ToLower();

            bool isSupported = false;

            for (const char* cookedExtension : s_cookedTextureExtensions)
            {
                if (extensionLower == cookedExtension)
                {
                    isSupported = true;
                    break;
                }
            }

            if (!isSupported)
            {
                continue;
            }

            const bool isLinear = IsLinearTextureName(String(filepath.Basename()).ToLower());

            // color maps are imported as sRGB; data maps (normals, masks, etc) stay linear
            const AssetLoadHint hint = isLinear
                ? AssetLoadHint::NoHint
                : AssetLoadHint::TextureLoader_LoadAsSRGB;

            auto loadResult = g_assetManager->Load<Texture>(filepath, String::empty, hint);

            if (loadResult.HasError())
            {
                HYP_LOG(Assets, Warning, "Failed to cook '{}': {}", filepath, loadResult.GetError().GetMessage());
                numFailed++;
            }
            else
            {
                Handle<Texture> texture = loadResult.GetValue().ExtractAs<Handle<Texture>>();

                HYP_LOG(Assets, Info, "Cooked '{}' ({}x{}, srgb: {})", filepath,
                    texture->GetTextureDesc().extent.x, texture->GetTextureDesc().extent.y,
                    texture->GetTextureDesc().IsSrgb());
                numCooked++;
            }
        }

        HYP_LOG(Assets, Info, "Cook loop done: {} cooked, {} failed", numCooked, numFailed);

        if (numCooked == 0)
        {
            return HYP_MAKE_ERROR(Error, "No textures were cooked - check the input directory");
        }

        registry->SaveDirtyAssets();

        PackTerrainLayerTextures(inputDir, registry);

        registry->SaveDirtyAssets();

        HYP_LOG(Assets, Info, "Cooked {} textures into the engine asset registry ({} failed)", numCooked, numFailed);

        return {};
    }
};

const Class* g_clsCookTexturesCommandlet = nullptr;

const Class* CookTexturesCommandlet::StaticClass()
{
    return g_clsCookTexturesCommandlet;
}

// clang-format off

HYP_BEGIN_CLASS(CookTexturesCommandlet, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "cooktextures"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

// clang-format on

HYP_REGISTER_STATIC_CLASS(CookTexturesCommandlet);

} // namespace Hyperion
