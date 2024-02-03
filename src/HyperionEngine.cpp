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

    // RC<dotnet::Assembly> script_assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly("csharp/bin/Debug/net8.0/csharp.dll");

    // if (!script_assembly) {
    //     DebugLog(LogType::Error, "Failed to load script assembly\n");
    //     return;
    // }

    // struct MyTestStruct
    // {
    //     UInt32 value;
    //     float x;
    // };

    // if (dotnet::Class *class_object = script_assembly->GetClassObjectHolder().FindClassByName("TestGame")) {
    //     // class_object->InvokeStaticMethod<void>("MyMethod");

    //     auto instance_object = class_object->NewObject();
    //     instance_object->InvokeMethod<void>("Initialize");

    //     // testing
    //     for (int i = 0; i < 10; ++i) {
    //         instance_object->InvokeMethod<void, float>("Update", 0.166f);
    //     }

    //     // Free the instance object
    //     instance_object.Reset();
    // } else {
    //     DebugLog(LogType::Error, "Failed to find MyClass in script assembly\n");
    // }
}

} // namespace hyperion