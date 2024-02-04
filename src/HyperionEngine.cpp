#include <HyperionEngine.hpp>
#include <dotnet/DotNetSystem.hpp>

namespace hyperion {

void InitializeApplication(RC<Application> application)
{
    AssertThrowMsg(
        g_engine == nullptr,
        "Hyperion already initialized!"
    );

    g_engine = new Engine;
    g_asset_manager = new AssetManager;
    g_shader_manager = new ShaderManagerSystem;
    g_material_system = new MaterialCache;
    g_safe_deleter = new SafeDeleter;

    g_engine->Initialize(application);

    dotnet::DotNetSystem::GetInstance().Initialize();
}

} // namespace hyperion