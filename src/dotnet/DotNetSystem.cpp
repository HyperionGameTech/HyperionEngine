#include <dotnet/DotNetSystem.hpp>

#include <util/fs/FsUtil.hpp>
#include <asset/ByteWriter.hpp>
#include <asset/Assets.hpp>
#include <Engine.hpp>

#include <core/dll/DynamicLibrary.hpp>

#ifdef HYP_DOTNET
#include <dotnetcore/hostfxr.h>
#include <dotnetcore/nethost.h>
#include <dotnetcore/coreclr_delegates.h>
#endif

namespace hyperion::dotnet {

namespace detail {

using InitializeAssemblyDelegate = void (*)(void *, const char *);

#ifdef HYP_DOTNET
class DotNetImpl : public DotNetImplBase
{
    static const String runtime_config;

public:
    DotNetImpl()
        : m_dll(nullptr),
          m_cxt(nullptr),
          m_init_fptr(nullptr),
          m_get_delegate_fptr(nullptr),
          m_close_fptr(nullptr)
    {
    }

    virtual ~DotNetImpl() override
    {
        if (!ShutdownDotNetRuntime()) {
            DebugLog(LogType::Error, "Failed to shutdown .NET runtime\n");
        }
    }

    FilePath GetDotNetPath() const
        { return g_asset_manager->GetBasePath() / "data/dotnet"; }

    FilePath GetLibraryPath() const
        { return GetDotNetPath() / "lib"; }

    FilePath GetRuntimeConfigPath() const
        { return GetDotNetPath() / "runtimeconfig.json"; }

    virtual void Initialize() override
    {
        // ensure the mono directories exists
        FileSystem::MkDir(GetDotNetPath().Data());
        FileSystem::MkDir(GetLibraryPath().Data());

        InitRuntimeConfig();

        // Load the .NET Core runtime
        if (!LoadHostFxr()) {
            HYP_THROW("Could not initialize .NET runtime: Failed to load hostfxr");
        }

        if (!InitDotNetRuntime()) {
            HYP_THROW("Could not initialize .NET runtime: Failed to initialize runtime");
        }

        // // TEMP! Find a better way to do this.
        const FilePath hyperion_runtime_path = g_asset_manager->GetBasePath() / ".." / "build" / "HyperionInterop.dll";

        m_root_assembly.Reset(new Assembly());

        const PlatformString runtime_path_wide = hyperion_runtime_path;

        InitializeAssemblyDelegate initialize_assembly = (InitializeAssemblyDelegate)GetDelegate(
            runtime_path_wide.Data(),
            HYP_TEXT("Hyperion.NativeInterop, HyperionInterop"),
            HYP_TEXT("InitializeAssembly"),
            HYP_TEXT("InitializeAssemblyDelegate, HyperionInterop")
        );
        AssertThrow(initialize_assembly != nullptr);

        // Call the Initialize method in the NativeInterop class directly,
        // to load all the classes and methods into the class object holder
        initialize_assembly(&m_root_assembly->GetClassObjectHolder(), hyperion_runtime_path.Data());

        // AssertThrowMsg(
        //     is_initialized,
        //     "Failed to initialize NativeInterop class in HyperionInterop.dll assembly"
        // );

        Class *native_interop_class_object = m_root_assembly->GetClassObjectHolder().FindClassByName("NativeInterop");

        AssertThrowMsg(
            native_interop_class_object != nullptr,
            "Failed to find NativeInterop class in HyperionInterop.dll assembly"
        );

        AssertThrowMsg(
            native_interop_class_object->HasMethod("InitializeAssembly"),
            "Failed to find InitializeAssembly() method in NativeInterop class in HyperionInterop.dll assembly"
        );
    }

    virtual RC<Assembly> LoadAssembly(const char *path) const override
    {
        RC<Assembly> assembly(new Assembly());

        FilePath filepath(path);
        
        if (!filepath.Exists()) {
            filepath = FilePath::Current() / path;
        }
        
        if (!filepath.Exists()) {
            return nullptr;
        }

        Class *native_interop_class_object = m_root_assembly->GetClassObjectHolder().FindClassByName("NativeInterop");
        AssertThrow(native_interop_class_object != nullptr);

        // Call our InitializeAssembly method to load all the classes and methods into the class object holder for the assembly
        DebugLog(
            LogType::Debug,
            "Calling InitializeAssembly for assembly: %s\n",
            filepath.Data()
        );

        // static constexpr uint32 major_minor_mask = (0xffu << 16u) | (0xffu << 8u);
        // const uint32 engine_version_major_minor = engine_version & major_minor_mask;
        // const uint32 assembly_engine_version = native_interop_class_object->InvokeStaticMethod<uint32>("GetEngineVersion");

        // if ((assembly_engine_version & major_minor_mask) != engine_version_major_minor) {
        //     DebugLog(
        //         LogType::Error,
        //         "Assembly engine version mismatch: Assembly version: %u.%u, Engine version: %u.%u\n",
        //         (assembly_engine_version >> 16u) & 0xffu,
        //         (assembly_engine_version >> 8u) & 0xffu,
        //         (engine_version >> 16u) & 0xffu,
        //         (engine_version >> 8u) & 0xffu
        //     );

        //     return nullptr;
        // }

        native_interop_class_object->InvokeStaticMethod<void, void *, char *>("InitializeAssembly", reinterpret_cast<void *>(&assembly->GetClassObjectHolder()), filepath.Data());

        return assembly;
    }

    virtual void *GetDelegate(
        const TChar *assembly_path,
        const TChar *type_name,
        const TChar *method_name,
        const TChar *delegate_type_name
    ) const override
    {
        DebugLog(LogType::Info, "Loading .NET assembly: %s\n", assembly_path);

        if (!m_cxt) {
            HYP_THROW("Failed to get delegate: .NET runtime not initialized");
        }

        // Get the delegate for the managed function
        void *load_assembly_and_get_function_pointer_fptr = nullptr;

        if (m_get_delegate_fptr(m_cxt, hdt_load_assembly_and_get_function_pointer, &load_assembly_and_get_function_pointer_fptr) != 0) {
            DebugLog(LogType::Error, "Failed to get delegate: Failed to get function pointer\n");

            return nullptr;
        }

        auto load_assembly_and_get_function_pointer = (load_assembly_and_get_function_pointer_fn)load_assembly_and_get_function_pointer_fptr;

        void *delegate_ptr = nullptr;

        bool result = load_assembly_and_get_function_pointer(assembly_path, type_name, method_name, delegate_type_name, nullptr, &delegate_ptr) == 0;

        if (!result) {
            DebugLog(LogType::Error, "Failed to get delegate: Failed to load assembly and get function pointer\n");

            return nullptr;
        }

        return delegate_ptr;
    }

private:
    void InitRuntimeConfig()
    {
        const FilePath filepath = GetRuntimeConfigPath();

        // Write the runtime config file if it doesn't exist

        FileByteWriter writer(filepath.Data());
        writer.Write(runtime_config.Data(), runtime_config.Size() + 1);
        writer.Close();
    }

    bool LoadHostFxr()
    {
        // Pre-allocate a large buffer for the path to hostfxr
        char_t buffer[2048];
        size_t buffer_size = sizeof(buffer) / sizeof(char_t);
        int rc = get_hostfxr_path(buffer, &buffer_size, nullptr);
        if (rc != 0) {
            return false;
        }

        // Load hostfxr and get desired exports
        m_dll = DynamicLibrary::Load(buffer);

        if (!m_dll) {
            return false;
        }

        m_init_fptr = (hostfxr_initialize_for_runtime_config_fn)m_dll->GetFunction("hostfxr_initialize_for_runtime_config");
        m_get_delegate_fptr = (hostfxr_get_runtime_delegate_fn)m_dll->GetFunction("hostfxr_get_runtime_delegate");
        m_close_fptr = (hostfxr_close_fn)m_dll->GetFunction("hostfxr_close");

        return m_init_fptr && m_get_delegate_fptr && m_close_fptr;
    }

    bool InitDotNetRuntime()
    {
        AssertThrow(m_cxt == nullptr);

        if (m_init_fptr(PlatformString(GetRuntimeConfigPath()).Data(), nullptr, &m_cxt) != 0) {
            return false;
        }

        return true;
    }

    bool ShutdownDotNetRuntime()
    {
        AssertThrow(m_cxt != nullptr);

        m_close_fptr(m_cxt);
        m_cxt = nullptr;

        return true;
    }

    UniquePtr<DynamicLibrary>                   m_dll;

    RC<Assembly>                                m_root_assembly;

    hostfxr_handle                              m_cxt;
    hostfxr_initialize_for_runtime_config_fn    m_init_fptr;
    hostfxr_get_runtime_delegate_fn             m_get_delegate_fptr;
    hostfxr_close_fn                            m_close_fptr;
};

const String DotNetImpl::runtime_config = R"(
{
    "runtimeOptions": {
        "tfm": "net8.0",
        "framework": {
            "name": "Microsoft.NETCore.App",
            "version": "8.0.1"
        }
    }
}
)";

#else

class DotNetImpl : public DotNetImplBase
{
public:
    DotNetImpl()                    = default;
    virtual ~DotNetImpl() override  = default;

    virtual void Initialize() override
    {
    }

    virtual RC<Assembly> LoadAssembly(const char *path) const override
    {
        return nullptr;
    }

    virtual void *GetDelegate(
        const TChar *assembly_path,
        const TChar *type_name,
        const TChar *method_name,
        const TChar *delegate_type_name
    ) const override
    {
        return nullptr;
    }
};

#endif

} // namespace detail

DotNetSystem &DotNetSystem::GetInstance()
{
    static DotNetSystem instance;

    return instance;
}

DotNetSystem::DotNetSystem()
    : m_is_initialized(false)
{
}

DotNetSystem::~DotNetSystem() = default;

RC<Assembly> DotNetSystem::LoadAssembly(const char *path) const
{
    if (!IsEnabled()) {
        DebugLog(LogType::Warn, "DotNetSystem not enabled, call Initialize() before attempting to load assemblies\n");

        return nullptr;
    }

    if (!IsEnabled()) {
        DebugLog(LogType::Warn, "DotNetSystem not initialized, call Initialize() before attempting to load assemblies\n");

        return nullptr;
    }

    AssertThrow(m_impl != nullptr);

    return m_impl->LoadAssembly(path);
}

bool DotNetSystem::IsEnabled() const
{
#ifndef HYP_DOTNET
    return false;
#else
    return true;
#endif
}

bool DotNetSystem::IsInitialized() const
{
    return m_is_initialized;
}

void DotNetSystem::Initialize()
{
    if (!IsEnabled()) {
        return;
    }

    if (m_is_initialized) {
        return;
    }

    AssertThrow(m_impl == nullptr);

    m_impl.Reset(new detail::DotNetImpl());

    m_is_initialized = true;
}

void DotNetSystem::Shutdown()
{
    if (!IsEnabled()) {
        return;
    }

    if (!m_is_initialized) {
        return;
    }

    m_impl.Reset();

    m_is_initialized = false;
}

} // namespace hyperion::dotnet
