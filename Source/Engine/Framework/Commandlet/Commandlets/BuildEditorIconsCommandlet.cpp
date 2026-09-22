/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 *
 *  Usage: BuildEditorIconsCommandlet [--ui-output-dir=Source/Editor/Managed/Assets/Icons/Generated]
 *    Rasterizes the SDF editor icons ahead of time: sprite textures go into the editor
 *    asset registry, and untinted PNGs are written out for the editor UI.
 */

#include <HyperionPch.hpp>

#include <Framework/Commandlet/Commandlet.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Core/CLI/CommandLine.hpp>
#include <Core/Core.hpp>
#include <Core/FileSystem/FilePath.hpp>
#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Rendering/Texture.hpp>

#include <Scene/Systems/Editor/EditorIcons.hpp>

#include <Util/Img/WritePng.hpp>

namespace Hyperion {

#ifdef HYP_EDITOR

static Result WriteUIIcons(const FilePath& outputDir, uint32 iconSize)
{
    if (!outputDir.MkDir())
    {
        return HYP_MAKE_ERROR(Error, "Failed to create editor icon output directory '{}'", outputDir);
    }

    Result r;

    for (uint32 iconIndex = 0; iconIndex < EditorIcons::NumIcons; iconIndex++)
    {
        const EditorIcons::Icon icon = EditorIcons::Icon(iconIndex);

        const Bitmap_RGBA8 bitmap = EditorIcons::BuildCanvas(icon)
            .Rasterize(iconSize);

        const FilePath iconPath = outputDir / (String(EditorIcons::GetFileStem(icon)) + ".png");

        if (!WritePng::Write(iconPath, bitmap.GetWidth(), bitmap.GetHeight(), bitmap.GetNumComponents(), bitmap.ToByteView().Data()))
        {
            r = HYP_MAKE_ERROR(Error, "Failed to write editor icon '{}'", iconPath);

            continue;
        }

        HYP_LOG(Engine, Info, "Wrote editor icon '{}'", iconPath);
    }

    return r;
}

static void BakeSpriteTextures(const Handle<AssetRegistry>& editorRegistry)
{
    GlobalContextScope assetRegistryScope { AssetRegistryContext { editorRegistry } };

    for (uint32 iconIndex = 0; iconIndex < EditorIcons::NumIcons; iconIndex++)
    {
        Handle<Texture> texture = EditorIcons::CreateSpriteTexture(EditorIcons::Icon(iconIndex), EditorIcons::SpriteTextureSize);

        editorRegistry->PutAssetsDeep(texture, /* overwriteExisting */ true);
    }

    editorRegistry->SaveDirtyAssets();

    HYP_LOG(Engine, Info, "Editor icon sprite textures saved to editor registry");
}

class BuildEditorIconsCommandlet : public CommandletBase
{
    HYP_OBJECT_BODY(BuildEditorIconsCommandlet);

public:
    virtual ~BuildEditorIconsCommandlet() override = default;

    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;
        static bool s_initialized = false;

        if (!s_initialized)
        {
            s_initialized = true;

            s_definitions.Add(
                "ui-output-dir",
                "o",
                "Directory the editor UI icon PNGs are written to",
                CommandLineArgumentFlags::NONE,
                CommandLineArgumentType::STRING,
                JSON::Value("Source/Editor/Managed/Assets/Icons/Generated"));
        }

        return s_definitions;
    }

protected:
    virtual Result Run(const CommandLineArguments& args) override
    {
        FilePath uiOutputDir = FilePath(args["ui-output-dir"].ToString());

        if (uiOutputDir.Empty())
        {
            uiOutputDir = "Source/Editor/Managed/Assets/Icons/Generated";
        }

        if (!uiOutputDir.IsAbsolute())
        {
            uiOutputDir = CoreApi::GetBaseDirectory() / uiOutputDir;
        }

        const Result result = WriteUIIcons(uiOutputDir, EditorIcons::UIImageSize);

        Handle<AssetRegistry> editorRegistry = GetEditorAssetRegistry();

        if (!editorRegistry.IsValid())
        {
            return HYP_MAKE_ERROR(Error, "Editor asset registry invalid");
        }

        BakeSpriteTextures(editorRegistry);

        return result;
    }
};

HYP_EXPORT const Class* g_clsBuildEditorIconsCommandlet = nullptr;

const Class* BuildEditorIconsCommandlet::StaticClass()
{
    return g_clsBuildEditorIconsCommandlet;
}

// clang-format off

HYP_BEGIN_CLASS(BuildEditorIconsCommandlet, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "buildeditoricons"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

// clang-format on

HYP_REGISTER_STATIC_CLASS(BuildEditorIconsCommandlet);

#endif // HYP_EDITOR

} // namespace Hyperion
