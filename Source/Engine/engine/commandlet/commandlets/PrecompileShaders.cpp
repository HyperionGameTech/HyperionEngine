/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <engine/commandlet/Commandlet.hpp>

#include <engine/EngineGlobals.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#include <Core/cli/CommandLine.hpp>

namespace Hyperion {

HYP_API extern const FilePath& GetLibraryDirectory();

class PrecompileShaders : public CommandletBase
{
    HYP_OBJECT_BODY(PrecompileShaders);

public:
    virtual ~PrecompileShaders() override = default;

    HYP_METHOD()
    static const CommandLineArgumentDefinitions& GetArgumentDefinitions()
    {
        static CommandLineArgumentDefinitions s_definitions;

        static bool s_initialized = false;
        if (!s_initialized)
        {
            s_initialized = true;

            // Platform arguments
            s_definitions.AddArgument({
                .name = "windows",
                .description = "Include Windows as a target platform",
                .type = CommandLineArgumentType::Bool,
                .category = "Platforms"
            });

            s_definitions.AddArgument({
                .name = "mac",
                .description = "Include macOS as a target platform",
                .type = CommandLineArgumentType::Bool,
                .category = "Platforms"
            });

            s_definitions.AddArgument({
                .name = "linux",
                .description = "Include Linux as a target platform",
                .type = CommandLineArgumentType::Bool,
                .category = "Platforms"
            });

            s_definitions.AddArgument({
                .name = "android",
                .description = "Include Android as a target platform",
                .type = CommandLineArgumentType::Bool,
                .category = "Platforms"
            });

            s_definitions.AddArgument({
                .name = "ios",
                .description = "Include iOS as a target platform",
                .type = CommandLineArgumentType::Bool,
                .category = "Platforms"
            });

            s_definitions.AddArgument({
                .name = "all-platforms",
                .description = "Include all platforms (Windows, Mac, Linux, Android, iOS)",
                .type = CommandLineArgumentType::Bool,
                .category = "Platforms"
            });

            // Backend arguments
            s_definitions.AddArgument({
                .name = "vulkan",
                .description = "Compile shaders for Vulkan backend",
                .type = CommandLineArgumentType::Bool,
                .category = "Backends"
            });

            s_definitions.AddArgument({
                .name = "dx12",
                .description = "Compile shaders for DX12 backend",
                .type = CommandLineArgumentType::Bool,
                .category = "Backends"
            });

            s_definitions.AddArgument({
                .name = "all-backends",
                .description = "Compile shaders for all backends (Vulkan, DX12)",
                .type = CommandLineArgumentType::Bool,
                .category = "Backends"
            });
        }

        return s_definitions;
    }

protected:
    static ShaderCompileParams ParseCompileParams(const CommandLineArguments& args)
    {
        ShaderCompileParams params;

        // Parse platform flags
        const bool allPlatforms = args.GetBool("all-platforms", false);
        
        if (allPlatforms)
        {
            params.SetTargetPlatforms(ShaderCompileTargetPlatform::AllPlatforms);
        }
        else
        {
            EnumFlags<ShaderCompileTargetPlatform> platforms = ShaderCompileTargetPlatform::None;

            if (args.GetBool("windows", false))
            {
                platforms |= ShaderCompileTargetPlatform::Windows;
            }
            if (args.GetBool("mac", false))
            {
                platforms |= ShaderCompileTargetPlatform::Mac;
            }
            if (args.GetBool("linux", false))
            {
                platforms |= ShaderCompileTargetPlatform::Linux;
            }
            if (args.GetBool("android", false))
            {
                platforms |= ShaderCompileTargetPlatform::Android;
            }
            if (args.GetBool("ios", false))
            {
                platforms |= ShaderCompileTargetPlatform::iOS;
            }

            // If no platforms specified, default to all
            if (platforms == ShaderCompileTargetPlatform::None)
            {
                platforms = ShaderCompileTargetPlatform::AllPlatforms;
            }

            params.SetTargetPlatforms(platforms);
        }

        // Parse backend flags
        const bool allBackends = args.GetBool("all-backends", false);
        
        if (allBackends)
        {
            params.SetTargetBackends(ShaderCompileTargetBackend::AllBackends);
        }
        else
        {
            EnumFlags<ShaderCompileTargetBackend> backends = ShaderCompileTargetBackend::None;

            if (args.GetBool("vulkan", false))
            {
                backends |= ShaderCompileTargetBackend::Vulkan;
            }
            if (args.GetBool("dx12", false))
            {
                backends |= ShaderCompileTargetBackend::DX12;
            }

            // If no backends specified, default to all
            if (backends == ShaderCompileTargetBackend::None)
            {
                backends = ShaderCompileTargetBackend::AllBackends;
            }

            params.SetTargetBackends(backends);
        }

        return params;
    }

    virtual Result Run_Impl(const CommandLineArguments& args) override
    {
        //TResult loadShaderPackageResult = GetEngineAssetRegistry()->LoadPackageFromManifest(
        //    GetLibraryDirectory() / "Engine" / "Shaders" / "PackageManifest.json",
        //    true,
        //    true);

        //if (loadShaderPackageResult.HasError())
        //{
        //    return loadShaderPackageResult.GetError();
        //}

        if (!g_shaderCompiler)
        {
            HYP_LOG(Engine, Error, "ShaderCompiler is not initialized");
            return HYP_MAKE_ERROR(Error, "ShaderCompiler is not initialized");
        }

        // Parse compile parameters from command line
        const ShaderCompileParams params = ParseCompileParams(args);

        // Log the targets we're compiling for
        HYP_LOG(Engine, Info, "Shader compilation targets:");
        HYP_LOG(Engine, Info, "  Platforms: {:#010x}", uint32(params.targetPlatforms));
        HYP_LOG(Engine, Info, "  Backends:  {:#010x}", uint32(params.targetBackends));

        // Check if we can compile for the requested targets
        if (!g_shaderCompiler->CanCompileShaders(params))
        {
            HYP_LOG(Engine, Error, "Cannot compile shaders for the requested targets. "
                "Ensure the engine was compiled with the necessary compiler support (HYP_GLSLANG for Vulkan, HYP_DXC for DX12)");
            return HYP_MAKE_ERROR(Error, "Shader compilation not supported for requested targets");
        }

        HYP_LOG(Engine, Info,  "Precompiling shaders...");

        const bool success = g_shaderCompiler->LoadShaderDefinitions(/* precompileShaders */ true, params);

        if (!success)
        {
            HYP_LOG(Engine, Error, "Shader precompilation failed");
            return HYP_MAKE_ERROR(Error, "Shader precompilation failed");
        }

        HYP_LOG(Engine, Info, "Shader precompilation complete");

        return {};
    }
};

HYP_API const Class* g_clsPrecompileShaders = nullptr;

HYP_BEGIN_CLASS(PrecompileShaders, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "precompileshaders"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(PrecompileShaders);

} // namespace Hyperion
