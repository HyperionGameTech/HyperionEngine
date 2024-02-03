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

    RC<dotnet::Assembly> script_assembly = dotnet::DotNetSystem::GetInstance().LoadAssembly("csharp/bin/Debug/net8.0/csharp.dll");

    if (!script_assembly) {
        DebugLog(LogType::Error, "Failed to load script assembly\n");
        return;
    }

    struct MyTestStruct
    {
        UInt32 value;
        float x;
    };

    if (dotnet::ClassObject *class_object = script_assembly->GetClassObjectHolder().FindClassByName("MyClass")) {
        class_object->InvokeStaticMethod<void>("MyMethod");

        dotnet::ManagedObject instance_object = class_object->NewObject();
        DebugLog(LogType::Info, "New object: %p\n", instance_object.ptr);

        Int32 invoke_method_result = class_object->InvokeMethod<Int32>("InstanceMethod", instance_object);

        class_object->FreeObject(instance_object);

        DebugLog(LogType::Info, "Successfully invoked MyClass.InstanceMethod, returned: %d\n", invoke_method_result);
    } else {
        DebugLog(LogType::Error, "Failed to find MyClass in script assembly\n");
    }
}

} // namespace hyperion