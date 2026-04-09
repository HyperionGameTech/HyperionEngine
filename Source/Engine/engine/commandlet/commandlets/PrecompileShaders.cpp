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
        return s_definitions;
    }

protected:
    virtual Result Run_Impl(const CommandLineArguments& args) override
    {
        //TResult loadShaderPackageResult = g_assetManager->GetAssetRegistry()->LoadPackageFromManifest(
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

        HYP_LOG(Engine, Info,  "Precompiling shaders...");

        const bool success = g_shaderCompiler->LoadShaderDefinitions(/* precompileShaders */ true);

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
