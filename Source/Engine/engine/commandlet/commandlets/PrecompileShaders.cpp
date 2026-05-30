/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <engine/commandlet/Commandlet.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <Core/reflection/ClassUtils.hpp>
#include <Core/reflection/ClassRegistry.hpp>

#include <Core/cli/CommandLine.hpp>

#include <Core/threading/TaskSystem.hpp>

namespace Hyperion {

ENGINE_API extern ShaderCompiler* g_shaderCompiler;

ENGINE_API extern const FilePath& GetLibraryDirectory();

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

            s_definitions.Add(
                "platform",
                "p",
                "Target platforms to compile for (comma-separated: windows,mac,linux,android,ios)", // @TODO add 'all'
                CommandLineArgumentFlags::ALLOW_MULTIPLE,
                Array<String> { "windows", "mac", "linux", "android", "ios" },
                JSON::Value("windows,mac,linux,android,ios"));

            s_definitions.Add(
                "api",
                "a",
                "Target rendering backends to compile for (comma-separated: vulkan,dx12)", // @TODO add 'all'
                CommandLineArgumentFlags::ALLOW_MULTIPLE,
                Array<String> { "vulkan", "dx12" },
                JSON::Value("vulkan,dx12"));

            s_definitions.Add(
                "filter",
                "f",
                "Comma-separated list of shader name filters (only shaders containing filter string are compiled)",
                CommandLineArgumentFlags::NONE,
                {},
                JSON::Value(""));
        }

        return s_definitions;
    }

protected:
    static Array<String> GetEnumValues(const CommandLineArgumentValue& value)
    {
        Array<String> result;

        if (value.IsArray())
        {
            for (const JSON::Value& element : value.AsArray())
            {
                result.PushBack(element.ToString());
            }
        }
        else if (value.IsString())
        {
            // Single value or comma-separated list
            Array<String> parts = value.ToString().Split(',');
            for (String& part : parts)
            {
                result.PushBack(part.Trimmed());
            }
        }

        return result;
    }

    static ShaderCompileParams ParseCompileParams(const CommandLineArguments& args)
    {
        ShaderCompileParams params;

        // Parse platforms
        Array<String> platforms = GetEnumValues(args["platform"]);
        EnumFlags<ShaderCompileTargetPlatform> platformFlags = ShaderCompileTargetPlatform::None;

        for (const String& platform : platforms)
        {
            if (platform == "windows")
                platformFlags |= ShaderCompileTargetPlatform::Windows;
            else if (platform == "mac")
                platformFlags |= ShaderCompileTargetPlatform::Mac;
            else if (platform == "linux")
                platformFlags |= ShaderCompileTargetPlatform::Linux;
            else if (platform == "android")
                platformFlags |= ShaderCompileTargetPlatform::Android;
            else if (platform == "ios")
                platformFlags |= ShaderCompileTargetPlatform::iOS;
        }

        if (platformFlags != ShaderCompileTargetPlatform::None)
        {
            params.SetTargetPlatforms(platformFlags);
        }
        else
        {
            params.SetTargetPlatforms(ShaderCompileTargetPlatform::AllPlatforms);
        }

        // Parse backends
        Array<String> backends = GetEnumValues(args["backends"]);
        EnumFlags<ShaderCompileTargetBackend> backendFlags = ShaderCompileTargetBackend::None;

        for (const String& backend : backends)
        {
            if (backend == "vulkan")
                backendFlags |= ShaderCompileTargetBackend::Vulkan;
            else if (backend == "dx12")
                backendFlags |= ShaderCompileTargetBackend::DX12;
        }

        if (backendFlags != ShaderCompileTargetBackend::None)
        {
            params.SetTargetBackends(backendFlags);
        }
        else
        {
            params.SetTargetBackends(ShaderCompileTargetBackend::AllBackends);
        }

        // Parse shader filters
        Array<String> filters = GetEnumValues(args["filter"]);
        params.shaderFilters = std::move(filters);

        return params;
    }

    virtual Result Run_Impl(const CommandLineArguments& args) override
    {
        if (!TaskSystem::GetInstance().IsRunning())
        {
            TaskSystem::GetInstance().Start();
        }

        if (!g_shaderCompiler)
        {
            HYP_LOG(Engine, Error, "ShaderCompiler is not initialized");
            return HYP_MAKE_ERROR(Error, "ShaderCompiler is not initialized");
        }

        const ShaderCompileParams params = ParseCompileParams(args);

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

ENGINE_API const Class* g_clsPrecompileShaders = nullptr;

const Class* PrecompileShaders::StaticClass()
{
    return g_clsPrecompileShaders;
}

HYP_BEGIN_CLASS(PrecompileShaders, -1, 0, NAME("CommandletBase"), ClassAttribute("command", "precompileshaders"))
    Method(NAME("GetArgumentDefinitions"), &Type::GetArgumentDefinitions)
HYP_END_CLASS

HYP_REGISTER_STATIC_CLASS(PrecompileShaders);

} // namespace Hyperion
