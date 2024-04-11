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

using InitializeAssemblyDelegate = void (*)(void *, const void *);

static Optional<FilePath> FindAssemblyFilePath(const char *path)
{
    FilePath filepath(path);
    
    if (!filepath.Exists()) {
        filepath = FilePath::Current() / path;
    }
    
    if (!filepath.Exists()) {
        filepath = g_asset_manager->GetBasePath() / path;
    }
    
    if (!filepath.Exists()) {
        DebugLog(
            LogType::Error,
            "Failed to load .NET assembly: File not found: %s\n",
            path
        );

        return { };
    }

    return filepath;
}

#ifdef HYP_DOTNET
class DotNetImpl : public DotNetImplBase
{
    static const String runtime_config;

public:
    DotNetImpl()
        : m_dll(nullptr),
          m_initialize_assembly_fptr(nullptr),
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

        const Optional<FilePath> interop_assembly_path = FindAssemblyFilePath("bin/HyperionInterop.dll");

        if (!interop_assembly_path.HasValue()) {
            HYP_THROW("Could not initialize .NET runtime: Could not locate HyperionInterop.dll!");
        }

        const PlatformString interop_assembly_path_platform = *interop_assembly_path;

        m_root_assembly.Reset(new Assembly());

       m_initialize_assembly_fptr = (InitializeAssemblyDelegate)GetDelegate(
            interop_assembly_path_platform.Data(),
            HYP_TEXT("Hyperion.NativeInterop, HyperionInterop"),
            HYP_TEXT("InitializeAssembly"),
            UNMANAGEDCALLERSONLY_METHOD
        );

        AssertThrowMsg(
            m_initialize_assembly_fptr != nullptr,
            "InitializeAssembly could not be found in HyperionInterop.dll! Ensure .NET libraries are properly compiled."
        );

        // Call the Initialize method in the NativeInterop class directly,
        // to load all the classes and methods into the class object holder
        m_initialize_assembly_fptr(&m_root_assembly->GetClassObjectHolder(), interop_assembly_path->Data());
    }

    virtual RC<Assembly> LoadAssembly(const char *path) const override
    {
        RC<Assembly> assembly(new Assembly());

        Optional<FilePath> filepath = FindAssemblyFilePath(path);

        if (!filepath.HasValue()) {
            return nullptr;
        }

        AssertThrow(m_root_assembly != nullptr);

        m_initialize_assembly_fptr(&assembly->GetClassObjectHolder(), filepath->Data());

        return assembly;
    }

    virtual void *GetDelegate(
        const TChar *assembly_path,
        const TChar *type_name,
        const TChar *method_name,
        const TChar *delegate_type_name
    ) const override
    {
        if (!m_cxt) {
            HYP_THROW("Failed to get delegate: .NET runtime not initialized");
        }

        // Get the delegate for the managed function
        void *load_assembly_and_get_function_pointer_fptr = nullptr;

        if (m_get_delegate_fptr(m_cxt, hdt_load_assembly_and_get_function_pointer, &load_assembly_and_get_function_pointer_fptr) != 0) {
            DebugLog(LogType::Error, "Failed to get delegate: Failed to get function pointer\n");

            return nullptr;
        }

#ifdef HYP_WINDOWS
        DebugLog(LogType::Info, "Loading .NET assembly: %S\tType Name: %S\tMethod Name: %S\n", assembly_path, type_name, method_name);
#else
        DebugLog(LogType::Info, "Loading .NET assembly: %s\tType Name: %s\tMethod Name: %s\n", assembly_path, type_name, method_name);
#endif

        void *delegate_ptr = nullptr;
        
        auto load_assembly_and_get_function_pointer = (load_assembly_and_get_function_pointer_fn)load_assembly_and_get_function_pointer_fptr;
        bool result = load_assembly_and_get_function_pointer(assembly_path, type_name, method_name, delegate_type_name, nullptr, &delegate_ptr) == 0;

        if (!result) {
            DebugLog(LogType::Error, "Failed to get delegate: Failed to load assembly and get function pointer\n");

            return nullptr;
        }

        DebugLog(LogType::Info, "Loaded delegate: %p\n", delegate_ptr);

        return delegate_ptr;
    }

private:
    void InitRuntimeConfig()
    {
        const FilePath filepath = GetRuntimeConfigPath();

        // Write the runtime config file if it doesn't exist

        FileByteWriter writer(filepath.Data());
        writer.WriteString(runtime_config);
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

#ifdef HYP_WINDOWS
        DebugLog(LogType::Debug, "Loading hostfxr from: %S\n", &buffer[0]);
#else
        DebugLog(LogType::Debug, "Loading hostfxr from: %s\n", &buffer[0]);
#endif

        // Load hostfxr and get desired exports
        m_dll = DynamicLibrary::Load(buffer);

        if (!m_dll) {
            return false;
        }

        m_init_fptr = (hostfxr_initialize_for_runtime_config_fn)m_dll->GetFunction("hostfxr_initialize_for_runtime_config");
        m_get_delegate_fptr = (hostfxr_get_runtime_delegate_fn)m_dll->GetFunction("hostfxr_get_runtime_delegate");
        m_close_fptr = (hostfxr_close_fn)m_dll->GetFunction("hostfxr_close");

        DebugLog(LogType::Debug, "Loaded hostfxr functions\n");

        return m_init_fptr && m_get_delegate_fptr && m_close_fptr;
    }

    bool InitDotNetRuntime()
    {
        AssertThrow(m_cxt == nullptr);

        DebugLog(LogType::Debug, "Initializing .NET runtime\n");

        if (m_init_fptr(PlatformString(GetRuntimeConfigPath()).Data(), nullptr, &m_cxt) != 0) {
            DebugLog(LogType::Error, "Failed to initialize .NET runtime\n");

            return false;
        }

        DebugLog(LogType::Debug, "Initialized .NET runtime\n");

        return true;
    }

    bool ShutdownDotNetRuntime()
    {
        AssertThrow(m_cxt != nullptr);

        DebugLog(LogType::Debug, "Shutting down .NET runtime\n");

        m_close_fptr(m_cxt);
        m_cxt = nullptr;

        DebugLog(LogType::Debug, "Shut down .NET runtime\n");

        return true;
    }

    UniquePtr<DynamicLibrary>                   m_dll;

    RC<Assembly>                                m_root_assembly;

    InitializeAssemblyDelegate                  m_initialize_assembly_fptr;

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
        DebugLog(LogType::Warn, "DotNetSystem not enabled, cannot load assemblies\n");

        return nullptr;
    }

    if (!IsInitialized()) {
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

    if (IsInitialized()) {
        return;
    }

    AssertThrow(m_impl == nullptr);

    m_impl.Reset(new detail::DotNetImpl());
    m_impl->Initialize();

    m_is_initialized = true;
}

void DotNetSystem::Shutdown()
{
    if (!IsEnabled()) {
        return;
    }

    if (!IsInitialized()) {
        return;
    }

    m_impl.Reset();

    m_is_initialized = false;
}

} // namespace hyperion::dotnet
