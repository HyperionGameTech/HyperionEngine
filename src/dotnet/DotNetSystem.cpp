/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/DotNetSystem.hpp>

#include <asset/ByteWriter.hpp>
#include <asset/Assets.hpp>

#include <core/system/AppContext.hpp>
#include <core/dll/DynamicLibrary.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

#include <util/fs/FsUtil.hpp>
#include <util/json/JSON.hpp>

#include <dotnet/Class.hpp>

#ifdef HYP_DOTNET
#include <dotnetcore/hostfxr.h>
#include <dotnetcore/nethost.h>
#include <dotnetcore/coreclr_delegates.h>
#endif

namespace hyperion {
namespace dotnet {
namespace detail {

class DotNetImplBase
{
public:
    virtual ~DotNetImplBase() = default;

    virtual void Initialize() = 0;
    virtual RC<Assembly> LoadAssembly(const char *path) const = 0;
    virtual bool UnloadAssembly(ManagedGuid guid) const = 0;

    virtual void AddMethodToCache(ManagedGuid assembly_guid, ManagedGuid method_guid, void *method_info_ptr) const = 0;
    virtual void AddObjectToCache(ManagedGuid assembly_guid, ManagedGuid object_guid, void *object_ptr, ManagedObject *out_managed_object) const = 0;

    virtual void *GetDelegate(
        const TChar *assembly_path,
        const TChar *type_name,
        const TChar *method_name,
        const TChar *delegate_type_name
    ) const = 0;
};

using InitializeAssemblyDelegate = void(*)(ManagedGuid *, ClassHolder *, const char *);
using UnloadAssemblyDelegate = void(*)(ManagedGuid *, int32 *);

using AddMethodToCacheDelegate = void(*)(ManagedGuid *, ManagedGuid *, void *);
using AddObjectToCacheDelegate = void(*)(ManagedGuid *, ManagedGuid *, void *, ManagedObject *);

static Optional<FilePath> FindAssemblyFilePath(const char *path)
{
    FilePath filepath = FilePath::Current() / path;

    if (!filepath.Exists() && g_engine->GetAppContext() != nullptr) {
        HYP_LOG(DotNET, LogLevel::WARNING, "Failed to load .NET assembly at path: {}. Trying next path...", filepath);

        filepath = FilePath(g_engine->GetAppContext()->GetArguments().GetCommand()).BasePath() / path;
    }
    
    if (!filepath.Exists()) {
        HYP_LOG(DotNET, LogLevel::WARNING, "Failed to load .NET assembly at path: {}. Trying next path...", filepath);

        filepath = g_asset_manager->GetBasePath() / "scripts" / "bin" / path;
    }
    
    if (!filepath.Exists()) {
        HYP_LOG(DotNET, LogLevel::ERROR, "Failed to load .NET assembly at path: {}. Could not locate an assembly for {}.", filepath, path);

        return { };
    }

    return filepath;
}

#ifdef HYP_DOTNET
class DotNetImpl : public DotNetImplBase
{
public:
    DotNetImpl()
        : m_dll(nullptr),
          m_initialize_assembly_fptr(nullptr),
          m_unload_assembly_fptr(nullptr),
          m_add_method_to_cache_fptr(nullptr),
          m_add_object_to_cache_fptr(nullptr),
          m_cxt(nullptr),
          m_init_fptr(nullptr),
          m_get_delegate_fptr(nullptr),
          m_close_fptr(nullptr)
    {
    }

    virtual ~DotNetImpl() override
    {
        if (!ShutdownDotNetRuntime()) {
            HYP_LOG(DotNET, LogLevel::ERROR, "Failed to shutdown .NET runtime");
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

        const Optional<FilePath> interop_assembly_path = FindAssemblyFilePath("HyperionInterop.dll");

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

        m_unload_assembly_fptr = (UnloadAssemblyDelegate)GetDelegate(
            interop_assembly_path_platform.Data(),
            HYP_TEXT("Hyperion.NativeInterop, HyperionInterop"),
            HYP_TEXT("UnloadAssembly"),
            UNMANAGEDCALLERSONLY_METHOD
        );

        AssertThrowMsg(
            m_unload_assembly_fptr != nullptr,
            "UnloadAssembly could not be found in HyperionInterop.dll! Ensure .NET libraries are properly compiled."
        );

        m_add_method_to_cache_fptr = (AddMethodToCacheDelegate)GetDelegate(
            interop_assembly_path_platform.Data(),
            HYP_TEXT("Hyperion.NativeInterop, HyperionInterop"),
            HYP_TEXT("AddMethodToCache"),
            UNMANAGEDCALLERSONLY_METHOD
        );

        AssertThrowMsg(
            m_add_method_to_cache_fptr != nullptr,
            "AddMethodToCache could not be found in HyperionInterop.dll! Ensure .NET libraries are properly compiled."
        );

        m_add_object_to_cache_fptr = (AddObjectToCacheDelegate)GetDelegate(
            interop_assembly_path_platform.Data(),
            HYP_TEXT("Hyperion.NativeInterop, HyperionInterop"),
            HYP_TEXT("AddObjectToCache"),
            UNMANAGEDCALLERSONLY_METHOD
        );

        AssertThrowMsg(
            m_add_object_to_cache_fptr != nullptr,
            "AddObjectToCache could not be found in HyperionInterop.dll! Ensure .NET libraries are properly compiled."
        );

        // Call the Initialize method in the NativeInterop class directly,
        // to load all the classes and methods into the class object holder
        m_initialize_assembly_fptr(
            &m_root_assembly->GetGuid(),
            &m_root_assembly->GetClassObjectHolder(),
            interop_assembly_path->Data()
        );
    }

    virtual RC<Assembly> LoadAssembly(const char *path) const override
    {
        RC<Assembly> assembly(new Assembly());

        Optional<FilePath> filepath = FindAssemblyFilePath(path);

        if (!filepath.HasValue()) {
            return nullptr;
        }

        AssertThrow(m_root_assembly != nullptr);

        m_initialize_assembly_fptr(
            &assembly->GetGuid(),
            &assembly->GetClassObjectHolder(),
            filepath->Data()
        );

        return assembly;
    }

    virtual bool UnloadAssembly(ManagedGuid assembly_guid) const override
    {
        HYP_LOG(DotNET, LogLevel::INFO, "Unloading assembly...");

        // if (Class *native_interop_class = m_root_assembly->GetClassObjectHolder().FindClassByName("NativeInterop")) {
        //     return native_interop_class->InvokeStaticMethod<bool>("UnloadAssembly", &assembly_guid);
        // }
        // return false;

        int32 result;
        m_unload_assembly_fptr(&assembly_guid, &result);

        return bool(result);
    }

    virtual void AddMethodToCache(ManagedGuid assembly_guid, ManagedGuid method_guid, void *method_info_ptr) const
    {
        m_add_method_to_cache_fptr(&assembly_guid, &method_guid, method_info_ptr);
    }

    virtual void AddObjectToCache(ManagedGuid assembly_guid, ManagedGuid object_guid, void *object_ptr, ManagedObject *out_managed_object) const
    {
        m_add_object_to_cache_fptr(&assembly_guid, &object_guid, object_ptr, out_managed_object);
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
            HYP_LOG(DotNET, LogLevel::ERROR, "Failed to get delegate: Failed to get function pointer");

            return nullptr;
        }

        HYP_LOG(DotNET, LogLevel::INFO, "Loading .NET assembly: {}\tType Name: {}\tMethod Name: {}", assembly_path, type_name, method_name);

        void *delegate_ptr = nullptr;
        
        auto load_assembly_and_get_function_pointer = (load_assembly_and_get_function_pointer_fn)load_assembly_and_get_function_pointer_fptr;
        bool result = load_assembly_and_get_function_pointer(assembly_path, type_name, method_name, delegate_type_name, nullptr, &delegate_ptr) == 0;

        if (!result) {
            HYP_LOG(DotNET, LogLevel::ERROR, "Failed to get delegate: Failed to load assembly and get function pointer");

            return nullptr;
        }

        return delegate_ptr;
    }

private:
    void InitRuntimeConfig()
    {
        const FilePath filepath = GetRuntimeConfigPath();

        const FilePath current_path = FilePath::Current();

        Array<json::JSONValue> probing_paths;

        probing_paths.PushBack(FilePath::Relative(GetLibraryPath(), current_path));

        if (g_engine->GetAppContext() != nullptr) {
            probing_paths.PushBack(FilePath::Relative(FilePath(g_engine->GetAppContext()->GetArguments().GetCommand()).BasePath(), current_path));
        }

        const json::JSONValue runtime_config_json(json::JSONObject {
            { "runtimeOptions", json::JSONObject({
                { "tfm", "net8.0" },
                { "framework", json::JSONObject({
                    { "name", "Microsoft.NETCore.App" },
                    { "version", "8.0.1" }
                }) },
                { "additionalProbingPaths", json::JSONArray(probing_paths) }
            }) }
        });

        String str = runtime_config_json.ToString(true);

        FileByteWriter writer(filepath.Data());
        writer.WriteString(str);
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

        HYP_LOG(DotNET, LogLevel::DEBUG, "Loading hostfxr from: {}", &buffer[0]);

        // Load hostfxr and get desired exports
        m_dll = DynamicLibrary::Load(buffer);

        if (!m_dll) {
            return false;
        }

        m_init_fptr = (hostfxr_initialize_for_runtime_config_fn)m_dll->GetFunction("hostfxr_initialize_for_runtime_config");
        m_get_delegate_fptr = (hostfxr_get_runtime_delegate_fn)m_dll->GetFunction("hostfxr_get_runtime_delegate");
        m_close_fptr = (hostfxr_close_fn)m_dll->GetFunction("hostfxr_close");

        HYP_LOG(DotNET, LogLevel::DEBUG, "Loaded hostfxr functions");

        return m_init_fptr && m_get_delegate_fptr && m_close_fptr;
    }

    bool InitDotNetRuntime()
    {
        AssertThrow(m_cxt == nullptr);

        HYP_LOG(DotNET, LogLevel::DEBUG, "Initializing .NET runtime");

        if (m_init_fptr(PlatformString(GetRuntimeConfigPath()).Data(), nullptr, &m_cxt) != 0) {
            HYP_LOG(DotNET, LogLevel::ERROR, "Failed to initialize .NET runtime");

            return false;
        }

        HYP_LOG(DotNET, LogLevel::DEBUG, "Initialized .NET runtime");

        return true;
    }

    bool ShutdownDotNetRuntime()
    {
        AssertThrow(m_cxt != nullptr);

        HYP_LOG(DotNET, LogLevel::DEBUG, "Shutting down .NET runtime");

        m_close_fptr(m_cxt);
        m_cxt = nullptr;

        HYP_LOG(DotNET, LogLevel::DEBUG, "Shut down .NET runtime");

        return true;
    }

    UniquePtr<DynamicLibrary>                   m_dll;

    RC<Assembly>                                m_root_assembly;

    InitializeAssemblyDelegate                  m_initialize_assembly_fptr;
    UnloadAssemblyDelegate                      m_unload_assembly_fptr;
    AddMethodToCacheDelegate                    m_add_method_to_cache_fptr;
    AddObjectToCacheDelegate                    m_add_object_to_cache_fptr;

    hostfxr_handle                              m_cxt;
    hostfxr_initialize_for_runtime_config_fn    m_init_fptr;
    hostfxr_get_runtime_delegate_fn             m_get_delegate_fptr;
    hostfxr_close_fn                            m_close_fptr;
};

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

    virtual bool UnloadAssembly(ManagedGuid guid) const override
    {
        return false;
    }

    virtual void AddMethodToCache(ManagedGuid assembly_guid, ManagedGuid method_guid, void *method_info_ptr) const override {}
    virtual void AddObjectToCache(ManagedGuid assembly_guid, ManagedGuid object_guid, void *object_ptr, ManagedObject *out_managed_object) const override {}

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

bool DotNetSystem::EnsureInitialized() const
{
    if (!IsEnabled()) {
        HYP_LOG(DotNET, LogLevel::ERROR, "DotNetSystem not enabled, cannot load/unload assemblies");

        return false;
    }

    if (!IsInitialized()) {
        HYP_LOG(DotNET, LogLevel::ERROR, "DotNetSystem not initialized, call Initialize() before attempting to load/unload assemblies");

        return false;
    }

    AssertThrow(m_impl != nullptr);

    return true;
}

RC<Assembly> DotNetSystem::LoadAssembly(const char *path) const
{
    if (!EnsureInitialized()) {
        return nullptr;
    }

    return m_impl->LoadAssembly(path);
}

bool DotNetSystem::UnloadAssembly(ManagedGuid guid) const
{
    if (!EnsureInitialized()) {
        return false;
    }

    return m_impl->UnloadAssembly(guid);
}

void DotNetSystem::AddMethodToCache(ManagedGuid assembly_guid, ManagedGuid method_guid, void *method_info_ptr) const
{
    if (!EnsureInitialized()) {
        return;
    }

    m_impl->AddMethodToCache(assembly_guid, method_guid, method_info_ptr);
}

void DotNetSystem::AddObjectToCache(ManagedGuid assembly_guid, ManagedGuid object_guid, void *object_ptr, ManagedObject *out_managed_object) const
{
    if (!EnsureInitialized()) {
        return;
    }

    m_impl->AddObjectToCache(assembly_guid, object_guid, object_ptr, out_managed_object);
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

} // namespace dotnet
} // namespace hyperion
