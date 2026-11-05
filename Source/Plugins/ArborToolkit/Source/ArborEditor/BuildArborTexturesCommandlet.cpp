/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 *
 *  Run: Hyperion.Editor.exe --exec BuildArborTexturesCommandlet [--arbor=<dir>] [--output=<dir>] [--max-size=1024]
 */

#include <EditorPch.hpp>

#include <ArborEditor/ArborShared.hpp>

#include <Framework/Commandlet/Commandlet.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Rendering/Texture.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/GlobalContext.hpp>
#include <Core/Utilities/Uuid.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/Core.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Core/Logging/Logger.hpp>

namespace Hyperion {

ARBOREDITOR_API HYP_DECLARE_LOG_CHANNEL(Arbor);

#ifdef HYP_EDITOR

namespace {

enum class ArborMapKind
{
    Color,
    Normal,
    Grey
};

ArborMapKind GetArborMapKind(const String& mapName)
{
    if (mapName.EndsWith("_normal"))
    {
        return ArborMapKind::Normal;
    }

    if (mapName.EndsWith("_roughness"))
    {
        return ArborMapKind::Grey;
    }

    // albedo and translucency
    return ArborMapKind::Color;
}

float SrgbToLinear(float value)
{
    return value <= 0.04045f ? value / 12.92f : MathUtil::Pow((value + 0.055f) / 1.055f, 2.4f);
}

float LinearToSrgb(float value)
{
    return value <= 0.0031308f ? value * 12.92f : 1.055f * MathUtil::Pow(value, 1.0f / 2.4f) - 0.055f;
}

ubyte ToByte(float value)
{
    return ubyte(MathUtil::Clamp(MathUtil::Round(value * 255.0f), 0.0f, 255.0f));
}

void Halve(ArborMapKind kind, Vec2u& extent, ByteBuffer& rgba)
{
    const Vec2u halfExtent { extent.x / 2, extent.y / 2 };

    ByteBuffer half(size_t(halfExtent.x) * size_t(halfExtent.y) * 4);

    const ubyte* src = rgba.Data();
    ubyte* dst = half.Data();

    for (uint32 y = 0; y < halfExtent.y; y++)
    {
        for (uint32 x = 0; x < halfExtent.x; x++)
        {
            float sum[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

            for (uint32 dy = 0; dy < 2; dy++)
            {
                for (uint32 dx = 0; dx < 2; dx++)
                {
                    const ubyte* texel = src + ((size_t(y * 2 + dy) * extent.x) + (x * 2 + dx)) * 4;

                    for (uint32 channel = 0; channel < 4; channel++)
                    {
                        const float value = float(texel[channel]) / 255.0f;

                        sum[channel] += (kind == ArborMapKind::Color && channel < 3) ? SrgbToLinear(value) : value;
                    }
                }
            }

            ubyte* out = dst + (size_t(y) * halfExtent.x + x) * 4;

            if (kind == ArborMapKind::Normal)
            {
                Vec3f normal { sum[0] * 0.5f - 1.0f, sum[1] * 0.5f - 1.0f, sum[2] * 0.5f - 1.0f };
                normal = normal.Length() > 1e-5f ? normal.Normalized() : Vec3f { 0.0f, 0.0f, 1.0f };

                out[0] = ToByte(normal.x * 0.5f + 0.5f);
                out[1] = ToByte(normal.y * 0.5f + 0.5f);
                out[2] = ToByte(normal.z * 0.5f + 0.5f);
            }
            else
            {
                for (uint32 channel = 0; channel < 3; channel++)
                {
                    const float average = sum[channel] * 0.25f;

                    out[channel] = ToByte(kind == ArborMapKind::Color ? LinearToSrgb(average) : average);
                }
            }

            out[3] = ToByte(sum[3] * 0.25f);
        }
    }

    extent = halfExtent;
    rgba = std::move(half);
}

FilePath GetArborPluginDirectory()
{
    return CoreApi::GetBaseDirectory() / "Source" / "Plugins" / "ArborToolkit";
}

Array<String> FindSpeciesTextureSets(const FilePath& speciesDirectory)
{
    Array<String> names;

    for (const FilePath& path : speciesDirectory.GetAllFilesInDirectory())
    {
        if (path.GetExtension().ToLower() != "ron")
        {
            continue;
        }

        FileByteReader reader { path };
        const ByteBuffer bytes = reader.Read();

        const ANSIStringView text(bytes.ToByteView());
        const ANSIStringView key = "texture: \"";

        for (size_t at = text.FindFirstIndex(key); at != ANSIStringView::NotFound; at = text.FindFirstIndex(key, at + key.Length()))
        {
            const size_t start = at + key.Length();
            const size_t end = text.FindFirstIndex('"', start);

            if (end == ANSIStringView::NotFound)
            {
                break;
            }

            const String name(text.Substr(start, end));

            if (name.Any() && !names.Contains(name))
            {
                names.PushBack(name);
            }
        }
    }

    return names;
}

bool IsMapOfSets(const String& mapName, const Array<String>& sets)
{
    for (const String& set : sets)
    {
        if (mapName.StartsWith(set + "_") && mapName.Length() > set.Length() + 1
            && !String(mapName.Substr(set.Length() + 1)).Contains("_"))
        {
            return true;
        }
    }

    return false;
}

Result BuildArborTexture(const FilePath& path, uint32 maxShortSide, const Handle<AssetRegistry>& outputRegistry)
{
    const String mapName = FilePath(path.Basename()).StripExtension();
    const ArborMapKind kind = GetArborMapKind(mapName);

    const EnumFlags<AssetLoadHint> hint = kind == ArborMapKind::Color
        ? (AssetLoadHint::Transient | AssetLoadHint::TextureSRGB)
        : EnumFlags<AssetLoadHint>(AssetLoadHint::Transient);

    auto loadResult = g_assetManager->Load<Texture>(path, String::empty, hint);

    if (loadResult.HasError())
    {
        return HYP_MAKE_ERROR(Error, "could not load {}: {}", path, loadResult.GetError().GetMessage());
    }

    const Handle<Texture> source = loadResult.GetValue().ExtractAs<Handle<Texture>>();

    Vec2u extent;
    ByteBuffer rgba;

    {
        auto readScope = source->GetReadScope();

        const TextureDesc& desc = source->GetTextureDesc();

        if (desc.format != TextureFormat::RGBA8 && desc.format != TextureFormat::RGBA8_SRGB)
        {
            return HYP_MAKE_ERROR(Error, "{} loaded as {}, not RGBA8", path, EnumToString(desc.format));
        }

        extent = Vec2u { desc.extent.x, desc.extent.y };

        const size_t baseSize = size_t(extent.x) * size_t(extent.y) * 4;

        if (source->GetImageData().Size() < baseSize)
        {
            return HYP_MAKE_ERROR(Error, "{} has no image data", path);
        }

        rgba = ByteBuffer(baseSize, source->GetImageData().Data());
    }

    while (MathUtil::Min(extent.x, extent.y) > maxShortSide && extent.x % 8 == 0 && extent.y % 8 == 0)
    {
        Halve(kind, extent, rgba);
    }

    TextureDesc desc;
    desc.type = TextureType::Texture2D;
    desc.format = kind == ArborMapKind::Color ? TextureFormat::RGBA8_SRGB : TextureFormat::RGBA8;
    desc.extent = Vec3u { extent.x, extent.y, 1 };
    desc.filterModeMin = TextureFilterMode::Linear;
    desc.filterModeMag = TextureFilterMode::Linear;
    desc.wrapMode = TextureWrapMode::Repeat;

    Handle<Texture> texture = MakeHandle<Texture>(desc, rgba.ToByteView());
    texture->SetName(GetArborTextureAssetName(mapName));

    texture->SetUUID(UUID::FromName(*texture->GetName()));

    texture->SetCompression(kind == ArborMapKind::Normal
            ? TextureCompressionMode::Normal
            : kind == ArborMapKind::Grey ? TextureCompressionMode::Mask
                                         : TextureCompressionMode::Auto);

    if (Result compressResult = texture->Compress(); compressResult.HasError())
    {
        return compressResult;
    }

    outputRegistry->PutAsset(texture);

    const FilePath manifestPath = outputRegistry->GetManifestPath(texture->GetPath());

    if (!manifestPath.BasePath().Exists() && !manifestPath.BasePath().MkDir())
    {
        return HYP_MAKE_ERROR(Error, "could not create {}", manifestPath.BasePath());
    }

    if (Result saveResult = texture->SaveAs(manifestPath); saveResult.HasError())
    {
        return saveResult;
    }

    HYP_LOG(Arbor, Info, "Arbor texture {}: {}x{} {}", texture->GetName(), extent.x, extent.y, EnumToString(texture->GetFormat()));

    return {};
}

} // namespace

class BuildArborTexturesCommandlet : public CommandletBase
{
    HYP_OBJECT_BODY(BuildArborTexturesCommandlet);

public:
    virtual ~BuildArborTexturesCommandlet() override = default;

    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;
        static bool s_initialized = false;

        if (!s_initialized)
        {
            s_initialized = true;

            s_definitions.Add(
                "arbor",
                "a",
                "The Arbor checkout to read assets/species and assets/textures from. The plugin's External/arbor submodule when empty",
                CommandLineArgumentFlags::NONE,
                CommandLineArgumentType::STRING,
                JSON::Value(""));

            s_definitions.Add(
                "output",
                "o",
                "Directory the texture assets are written to. The ArborToolkit plugin's Content when empty",
                CommandLineArgumentFlags::NONE,
                CommandLineArgumentType::STRING,
                JSON::Value(""));

            s_definitions.Add(
                "max-size",
                "m",
                "Max size, textures are halved until they fit this dimension on largest side.",
                CommandLineArgumentFlags::NONE,
                CommandLineArgumentType::INTEGER,
                JSON::Value(1024));
        }

        return s_definitions;
    }

protected:
    virtual Result Run(const CommandLineArguments& args) override
    {
        const String inputPath = args["arbor"].ToString();
        const String outputPath = args["output"].ToString();
        const uint32 maxShortSide = uint32(MathUtil::Max(args["max-size"].ToInt32(), 4));

        if (IsOnThread(g_simThread))
        {
            RunStatic(inputPath, outputPath, maxShortSide);
        }
        else
        {
            GetThreadById(g_simThread)->GetScheduler().Enqueue(
                [inputPath, outputPath, maxShortSide]()
                {
                    RunStatic(inputPath, outputPath, maxShortSide);
                },
                TaskEnqueueFlags::FIRE_AND_FORGET);
        }

        return {};
    }

    static void RunStatic(const String& inputPath, const String& outputPath, uint32 maxShortSide)
    {
        const FilePath arborDirectory = inputPath.Empty() ? GetArborPluginDirectory() / "External" / "arbor" : FilePath(inputPath);
        const FilePath inputDirectory = arborDirectory / "assets" / "textures";
        const FilePath outputDirectory = outputPath.Empty() ? GetArborPluginDirectory() / "Content" : FilePath(outputPath);

        if (!inputDirectory.IsDirectory())
        {
            HYP_LOG(Arbor, Error, "{} is not a directory. Run: git submodule update --init Source/Plugins/ArborToolkit/External/arbor", inputDirectory);
            return;
        }

        const Array<String> textureSets = FindSpeciesTextureSets(arborDirectory / "assets" / "species");

        if (textureSets.Empty())
        {
            HYP_LOG(Arbor, Error, "no species in {} name a texture set", arborDirectory / "assets" / "species");
            return;
        }

        if (!outputDirectory.Exists() && !outputDirectory.MkDir())
        {
            HYP_LOG(Arbor, Error, "Could not create directory at : {}", outputDirectory);
            return;
        }

        // keeps the Engine id, so the textures resolve once the plugin's Content is mounted on the engine registry. An Engine-id
        // asset saves through whatever the engine registry is, so this one stands in for it while the textures are written
        Handle<AssetRegistry> outputRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Engine, outputDirectory);
        outputRegistry->Initialize(nullptr);

        const Handle<AssetRegistry> engineRegistry = GetEngineAssetRegistry();
        SetEngineAssetRegistry(outputRegistry);

        GlobalContextScope assetRegistryScope { AssetRegistryContext { outputRegistry } };

        uint32 numBuilt = 0;
        uint32 numFailed = 0;

        for (const FilePath& path : inputDirectory.GetAllFilesInDirectory())
        {
            if (path.GetExtension().ToLower() != "png" || !IsMapOfSets(FilePath(path.Basename()).StripExtension(), textureSets))
            {
                continue;
            }

            if (Result result = BuildArborTexture(path, maxShortSide, outputRegistry); result.HasError())
            {
                HYP_LOG(Arbor, Error, "Error: {}", result.GetError().GetMessage());

                numFailed++;

                continue;
            }

            numBuilt++;
        }

        SetEngineAssetRegistry(engineRegistry);

        HYP_LOG(Arbor, Info, "{} built into {}, {} failed", numBuilt, outputRegistry->GetRootPath(), numFailed);
    }
};

HYP_EXPORT const Class* g_clsBuildArborTexturesCommandlet = nullptr;

const Class* BuildArborTexturesCommandlet::StaticClass()
{
    return g_clsBuildArborTexturesCommandlet;
}

HYP_BEGIN_CLASS(BuildArborTexturesCommandlet, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "buildarbortextures"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(BuildArborTexturesCommandlet);

#endif // HYP_EDITOR

} // namespace Hyperion
