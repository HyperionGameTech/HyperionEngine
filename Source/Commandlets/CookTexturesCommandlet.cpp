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

// Cooked layout, shared with Terrain.hlsl:
//   albedo  rgb = albedo (sRGB), a = roughness (linear; sRGB formats decode rgb only)
//   normal  rg  = tangent normal xy, b = ambient occlusion, a = height
// Normal z is reconstructed in the shader, which frees the blue channel for AO.
struct TerrainLayerPackSpec
{
    uint32 layerIndex;
    const char* colorFile;
    const char* normalFile;
    const char* aoFile;
    const char* heightFile;
    const char* roughnessFile;
    bool flipNormalGreen;
};

static Handle<Texture> LoadPackSourceTexture(
    const FilePath& inputDir,
    const char* filename,
    EnumFlags<AssetLoadHint> hint)
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

static float SnowHash(int32 x, int32 y, uint32 periodX, uint32 periodY)
{
    const uint32 wrappedX = uint32(((x % int32(periodX)) + int32(periodX)) % int32(periodX));
    const uint32 wrappedY = uint32(((y % int32(periodY)) + int32(periodY)) % int32(periodY));

    uint32 h = wrappedX * 374761393u + wrappedY * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;

    return float(h ^ (h >> 16)) / float(0xFFFFFFFFu);
}

// lattice coordinates wrap at the period on each axis, so the cooked layer stays seamless
static float SnowValueNoise(float x, float y, uint32 periodX, uint32 periodY)
{
    const int32 ix = int32(MathUtil::Floor(x));
    const int32 iy = int32(MathUtil::Floor(y));

    const float fx = x - float(ix);
    const float fy = y - float(iy);

    const float sx = fx * fx * (3.0f - 2.0f * fx);
    const float sy = fy * fy * (3.0f - 2.0f * fy);

    const float a = SnowHash(ix, iy, periodX, periodY);
    const float b = SnowHash(ix + 1, iy, periodX, periodY);
    const float c = SnowHash(ix, iy + 1, periodX, periodY);
    const float d = SnowHash(ix + 1, iy + 1, periodX, periodY);

    return MathUtil::Lerp(MathUtil::Lerp(a, b, sx), MathUtil::Lerp(c, d, sx), sy);
}

// frequencies stay integral and double per octave, which keeps every octave tiling on the unit square
static float SnowFbm(float u, float v, uint32 frequencyX, uint32 frequencyY, uint32 octaves)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    float totalAmplitude = 0.0f;

    for (uint32 octave = 0; octave < octaves; octave++)
    {
        value += amplitude * SnowValueNoise(u * float(frequencyX), v * float(frequencyY), frequencyX, frequencyY);
        totalAmplitude += amplitude;

        frequencyX *= 2u;
        frequencyY *= 2u;
        amplitude *= 0.5f;
    }

    return value / MathUtil::Max(totalAmplitude, 1e-6f);
}

// Wind-packed snow: broad drifts stretched across the prevailing wind, with a fine grain on top.
static float SnowHeightAt(float u, float v)
{
    // a low x frequency against a high y frequency draws drift rows running along x
    const float drifts = SnowFbm(u, v, 2u, 8u, 4u);
    const float grain = SnowFbm(u, v, 48u, 48u, 3u);

    return MathUtil::Clamp(drifts * 0.82f + grain * 0.18f, 0.0f, 1.0f);
}

static void PackProceduralSnowLayer(Handle<AssetRegistry> registry)
{
    constexpr uint32 extent = 1024;
    constexpr uint32 layerIndex = 3;

    // relief is shallow - snow smooths whatever it lies on
    constexpr float normalStrength = 2.5f;

    ByteBuffer albedoBytes(size_t(extent) * size_t(extent) * 4u);
    ByteBuffer normalBytes(size_t(extent) * size_t(extent) * 4u);

    ubyte* albedoDst = albedoBytes.Data();
    ubyte* normalDst = normalBytes.Data();

    Array<float> heights;
    heights.Resize(size_t(extent) * size_t(extent));

    for (uint32 y = 0; y < extent; y++)
    {
        for (uint32 x = 0; x < extent; x++)
        {
            heights[size_t(y) * extent + x] = SnowHeightAt(float(x) / float(extent), float(y) / float(extent));
        }
    }

    for (uint32 y = 0; y < extent; y++)
    {
        for (uint32 x = 0; x < extent; x++)
        {
            const size_t index = size_t(y) * extent + x;

            const float h = heights[index];

            const float hLeft = heights[size_t(y) * extent + ((x + extent - 1) % extent)];
            const float hRight = heights[size_t(y) * extent + ((x + 1) % extent)];
            const float hDown = heights[size_t((y + extent - 1) % extent) * extent + x];
            const float hUp = heights[size_t((y + 1) % extent) * extent + x];

            Vec3f n { (hLeft - hRight) * normalStrength, (hDown - hUp) * normalStrength, 1.0f };
            n.Normalize();

            // crests are wind-scoured and icier; hollows hold softer, slightly bluer powder
            const float exposure = MathUtil::Clamp(h * 1.4f - 0.2f, 0.0f, 1.0f);

            const Vec3f hollowColor { 0.80f, 0.84f, 0.92f };
            const Vec3f crestColor { 0.93f, 0.94f, 0.96f };

            const Vec3f linearColor = MathUtil::Lerp(hollowColor, crestColor, exposure);

            // wind crust is glossier than fresh powder
            const float roughness = MathUtil::Lerp(0.62f, 0.34f, exposure);
            const float ao = MathUtil::Clamp(0.72f + h * 0.28f, 0.0f, 1.0f);

            const size_t i = index * 4u;

            albedoDst[i + 0] = ubyte(MathUtil::Clamp(MathUtil::Pow(linearColor.x, 1.0f / 2.2f), 0.0f, 1.0f) * 255.0f);
            albedoDst[i + 1] = ubyte(MathUtil::Clamp(MathUtil::Pow(linearColor.y, 1.0f / 2.2f), 0.0f, 1.0f) * 255.0f);
            albedoDst[i + 2] = ubyte(MathUtil::Clamp(MathUtil::Pow(linearColor.z, 1.0f / 2.2f), 0.0f, 1.0f) * 255.0f);
            albedoDst[i + 3] = ubyte(roughness * 255.0f);

            normalDst[i + 0] = ubyte(MathUtil::Clamp(n.x * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f);
            normalDst[i + 1] = ubyte(MathUtil::Clamp(n.y * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f);
            normalDst[i + 2] = ubyte(ao * 255.0f);
            normalDst[i + 3] = ubyte(h * 255.0f);
        }
    }

    TextureDesc albedoDesc {
        TextureType::Texture2D,
        TextureFormat::RGBA8_SRGB,
        Vec3u { extent, extent, 1 },
        TextureFilterMode::LinearMipmap,
        TextureFilterMode::Linear,
        TextureWrapMode::Repeat
    };

    Texture::GenerateMipmaps(albedoDesc, albedoBytes);

    Handle<Texture> albedoTexture = MakeHandle<Texture>(albedoDesc, albedoBytes.ToByteView());
    albedoTexture->SetName(NAME_FMT("Terrain_Layer{}_Albedo", layerIndex));

    TextureDesc normalDesc {
        TextureType::Texture2D,
        TextureFormat::RGBA8,
        Vec3u { extent, extent, 1 },
        TextureFilterMode::LinearMipmap,
        TextureFilterMode::Linear,
        TextureWrapMode::Repeat
    };

    Texture::GenerateMipmaps(normalDesc, normalBytes);

    Handle<Texture> normalTexture = MakeHandle<Texture>(normalDesc, normalBytes.ToByteView());
    normalTexture->SetName(NAME_FMT("Terrain_Layer{}_Normal", layerIndex));

    registry->PutAsset(albedoTexture);
    registry->PutAsset(normalTexture);

    HYP_LOG(Assets, Info, "Generated procedural terrain snow layer {} ({}x{})", layerIndex, extent, extent);
}

static void PackTerrainLayerTextures(const FilePath& inputDir, Handle<AssetRegistry> registry)
{
    // Layer roles match TerrainGenerator::SynthesizeSplatWeights: grass, rock, dirt, snow.
    // Layer 1 carries every cliff face, so it takes the source with the deepest relief.
    static const TerrainLayerPackSpec s_specs[] = {
        { 0, "patchy-meadow1_albedo.png", "patchy-meadow1_normal-ogl.png", "patchy-meadow1_ao.png", "patchy-meadow1_height.png", "patchy-meadow1_roughness.png", false },
        { 1, "bumpy_worn_ground_albedo.png", "bumpy_worn_ground_normal-ogl.png", "bumpy_worn_ground_ao.png", "bumpy_worn_ground_height.png", "bumpy_worn_ground_roughness.png", false },
        // channel beds cut across open grass, so this layer has to sit close to grass in value -
        // a bright grey gravel reads as hard white ribbons across the ground
        { 2, "rocky-rugged-terrain_1_albedo.png", "rocky-rugged-terrain_1_normal-ogl.png", "rocky-rugged-terrain_1_ao.png", "rocky-rugged-terrain_1_height.png", "rocky-rugged-terrain_1_roughness.png", false }
    };

    for (const TerrainLayerPackSpec& spec : s_specs)
    {
        Handle<Texture> color = LoadPackSourceTexture(inputDir, spec.colorFile, AssetLoadHint::TextureSRGB | AssetLoadHint::Transient);
        Handle<Texture> normal = LoadPackSourceTexture(inputDir, spec.normalFile, AssetLoadHint::Transient);

        if (!color.IsValid() || !normal.IsValid())
        {
            HYP_LOG(Assets, Warning, "Skipping terrain layer {} pack (missing color/normal source)", spec.layerIndex);

            continue;
        }

        // AO / height / roughness are optional; missing files stay neutral white
        Handle<Texture> aoMask = LoadPackSourceTexture(inputDir, spec.aoFile, AssetLoadHint::Transient);
        Handle<Texture> heightMask = LoadPackSourceTexture(inputDir, spec.heightFile, AssetLoadHint::Transient);
        Handle<Texture> roughnessMask = LoadPackSourceTexture(inputDir, spec.roughnessFile, AssetLoadHint::Transient);

        const uint32 width = color->GetExtent().x;
        const uint32 height = color->GetExtent().y;

        ByteBuffer albedoBytes(size_t(width) * size_t(height) * 4u);
        ByteBuffer normalBytes(size_t(width) * size_t(height) * 4u);

        ubyte* albedoDst = albedoBytes.Data();
        ubyte* normalDst = normalBytes.Data();

        // sources without a roughness map fall back to a dull dielectric rather than a mirror
        const float defaultRoughness = 0.85f;

        // stretch heights to the full 0..1 range so the top of the surface sits at zero parallax depth
        float heightMin = 0.0f;
        float heightMax = 1.0f;

        if (heightMask.IsValid())
        {
            heightMin = 1.0f;
            heightMax = 0.0f;

            for (uint32 y = 0; y < height; y++)
            {
                for (uint32 x = 0; x < width; x++)
                {
                    const float h = heightMask->Sample2D(Vec2f { (float(x) + 0.5f) / float(width), (float(y) + 0.5f) / float(height) }).x;

                    heightMin = MathUtil::Min(heightMin, h);
                    heightMax = MathUtil::Max(heightMax, h);
                }
            }

            if (heightMax - heightMin < 0.001f)
            {
                heightMin = 0.0f;
                heightMax = 1.0f;
            }
        }

        for (uint32 y = 0; y < height; y++)
        {
            for (uint32 x = 0; x < width; x++)
            {
                const Vec2f uv { (float(x) + 0.5f) / float(width), (float(y) + 0.5f) / float(height) };

                // nearest-sampled; sizes may differ between sources, mip chain smooths the rest
                const Vec4f c = color->Sample2D(uv);  // linear (sRGB decoded)
                const Vec4f n = normal->Sample2D(uv); // linear
                const float ao = aoMask.IsValid() ? aoMask->Sample2D(uv).x : 1.0f;
                const float h = heightMask.IsValid() ? (heightMask->Sample2D(uv).x - heightMin) / (heightMax - heightMin) : 1.0f;
                const float roughness = roughnessMask.IsValid() ? roughnessMask->Sample2D(uv).x : defaultRoughness;

                const float g = spec.flipNormalGreen ? 1.0f - n.y : n.y;

                const size_t i = (size_t(y) * size_t(width) + size_t(x)) * 4u;

                albedoDst[i + 0] = ubyte(MathUtil::Clamp(MathUtil::Pow(c.x, 1.0f / 2.2f), 0.0f, 1.0f) * 255.0f);
                albedoDst[i + 1] = ubyte(MathUtil::Clamp(MathUtil::Pow(c.y, 1.0f / 2.2f), 0.0f, 1.0f) * 255.0f);
                albedoDst[i + 2] = ubyte(MathUtil::Clamp(MathUtil::Pow(c.z, 1.0f / 2.2f), 0.0f, 1.0f) * 255.0f);
                albedoDst[i + 3] = ubyte(MathUtil::Clamp(roughness, 0.0f, 1.0f) * 255.0f);

                normalDst[i + 0] = ubyte(MathUtil::Clamp(n.x, 0.0f, 1.0f) * 255.0f);
                normalDst[i + 1] = ubyte(MathUtil::Clamp(g, 0.0f, 1.0f) * 255.0f);
                normalDst[i + 2] = ubyte(MathUtil::Clamp(ao, 0.0f, 1.0f) * 255.0f);
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

        registry->PutAsset(albedoTexture);
        registry->PutAsset(normalTexture);

        registry->RemoveAsset(color);
        registry->RemoveAsset(normal);
        registry->RemoveAsset(aoMask);
        registry->RemoveAsset(heightMask);
        registry->RemoveAsset(roughnessMask);

        HYP_LOG(Assets, Info, "Packed terrain layer {} ({}x{}, ao: {}, height: {}, roughness: {}, height range: {}..{})",
            spec.layerIndex, width, height,
            aoMask.IsValid() ? "yes" : "neutral",
            heightMask.IsValid() ? "yes" : "neutral",
            roughnessMask.IsValid() ? "yes" : "neutral",
            heightMin, heightMax);
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

        // for (const FilePath& filepath : inputDir.GetAllFilesInDirectory())
        // {
        //     const String extensionLower = filepath.GetExtension().ToLower();

        //     bool isSupported = false;

        //     for (const char* cookedExtension : s_cookedTextureExtensions)
        //     {
        //         if (extensionLower == cookedExtension)
        //         {
        //             isSupported = true;
        //             break;
        //         }
        //     }

        //     if (!isSupported)
        //     {
        //         continue;
        //     }

        //     const bool isLinear = IsLinearTextureName(String(filepath.Basename()).ToLower());

        //     // color maps are imported as sRGB; data maps (normals, masks, etc) stay linear
        //     const AssetLoadHint hint = isLinear
        //         ? AssetLoadHint::NoHint
        //         : AssetLoadHint::TextureLoader_LoadAsSRGB;

        //     auto loadResult = g_assetManager->Load<Texture>(filepath, String::empty, hint);

        //     if (loadResult.HasError())
        //     {
        //         HYP_LOG(Assets, Warning, "Failed to cook '{}': {}", filepath, loadResult.GetError().GetMessage());
        //         numFailed++;
        //     }
        //     else
        //     {
        //         Handle<Texture> texture = loadResult.GetValue().ExtractAs<Handle<Texture>>();

        //         HYP_LOG(Assets, Info, "Cooked '{}' ({}x{}, srgb: {})", filepath,
        //             texture->GetTextureDesc().extent.x, texture->GetTextureDesc().extent.y,
        //             texture->GetTextureDesc().IsSrgb());
        //         numCooked++;
        //     }
        // }

        // HYP_LOG(Assets, Info, "Cook loop done: {} cooked, {} failed", numCooked, numFailed);

        // if (numCooked == 0)
        // {
        //     return HYP_MAKE_ERROR(Error, "No textures were cooked - check the input directory");
        // }

        // registry->SaveDirtyAssets();

        PackTerrainLayerTextures(inputDir, registry);
        PackProceduralSnowLayer(registry);

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
