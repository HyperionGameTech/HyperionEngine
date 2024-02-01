#include <dotnet_support/DotNetSystem.hpp>

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

        Delegate test_delegate = GetDelegate(
            (FilePath::Current() / "csharp/bin/Debug/net8.0/csharp.dll").Data(),
            "MyNamespace.MyClass, csharp",
            "MyMethod",
            "MyDelegate, csharp"
        );
        AssertThrow(test_delegate != nullptr);

        test_delegate();
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

    virtual Delegate GetDelegate(
        const char *assembly_path,
        const char *type_name,
        const char *method_name,
        const char *delegate_type_name
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

        auto load_assembly_and_get_function_pointer = (load_assembly_and_get_function_pointer_fn)load_assembly_and_get_function_pointer_fptr;

        void *delegate_ptr = nullptr;

        bool result = load_assembly_and_get_function_pointer(assembly_path, type_name, method_name, delegate_type_name, nullptr, &delegate_ptr) == 0;

        if (!result) {
            DebugLog(LogType::Error, "Failed to get delegate: Failed to load assembly and get function pointer\n");

            return nullptr;
        }

        return (Delegate)delegate_ptr;
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
        
        if (m_init_fptr(GetRuntimeConfigPath().Data(), nullptr, &m_cxt) != 0) {
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

    hostfxr_handle                              m_cxt;
    hostfxr_initialize_for_runtime_config_fn    m_init_fptr;
    hostfxr_get_runtime_delegate_fn             m_get_delegate_fptr;
    hostfxr_close_fn                            m_close_fptr;
};

#else

class DotNotImpl : public DotNetImplBase
{
public:
    DotNotImpl()                    = default;
    virtual ~DotNotImpl() override  = default;
};

#endif

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