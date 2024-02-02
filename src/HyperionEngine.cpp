#include <HyperionEngine.hpp>
#include <dotnet_support/DotNetSystem.hpp>

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

    RC<dotnet::Assembly> script_assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly("csharp/bin/Debug/net8.0/csharp.dll");

    if (!script_assembly) {
        DebugLog(LogType::Error, "Failed to load script assembly\n");
        return;
    }

    if (dotnet::ClassObject *class_object = script_assembly->GetClassObjectHolder().FindClassByName("MyClass")) {
        class_object->InvokeMethod<void *>("MyMethod");
    } else {
        DebugLog(LogType::Error, "Failed to find MyClass in script assembly\n");
    }
}

} // namespace hyperion