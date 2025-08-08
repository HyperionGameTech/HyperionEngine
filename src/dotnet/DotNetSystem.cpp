/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/DotNetSystem.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/dll/DynamicLibrary.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/filesystem/FsUtil.hpp>
#include <core/json/JSON.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <system/AppContext.hpp>
#include <system/App.hpp>

#include <dotnet/Class.hpp>

#ifdef HYP_DOTNET
#include <dotnetcore/hostfxr.h>
#include <dotnetcore/nethost.h>
#include <dotnetcore/coreclr_delegates.h>
#endif

#include <HyperionEngine.hpp>

namespace hyperion {

enum class LoadAssemblyResult : int32
{
    UNKNOWN_ERROR = -100,
    VERSION_MISMATCH = -2,
    NOT_FOUND = -1,
    OK = 0
};

namespace dotnet {
class DotNetImplBase
{
public:
    virtual ~DotNetImplBase() = default;

    virtual void Initialize(const FilePath& basePath) = 0;
    virtual RC<Assembly> LoadAssembly(const char* path) const = 0;
    virtual bool UnloadAssembly(ManagedGuid guid) const = 0;
    virtual bool IsCoreAssembly(ManagedGuid guid) const = 0;
    virtual bool IsCoreAssembly(const Assembly* assembly) const = 0;

    virtual void* GetDelegate(
        const TChar* assemblyPath,
        const TChar* typeName,
        const TChar* methodName,
        const TChar* delegateTypeName) const = 0;
};

using InitializeRuntimeDelegate = int (*)();
using InitializeAssemblyDelegate = int (*)(ManagedGuid*, Assembly*, const char*, int32);
using UnloadAssemblyDelegate = void (*)(ManagedGuid*, int32*);

static Optional<FilePath> FindAssemblyFilePath(const FilePath& basePath, const char* path)
{
    HYP_NAMED_SCOPE("Find .NET Assembly File Path");

    const FilePath filepath = basePath / path;

    if (!filepath.Exists())
    {
        return {};
    }

    return filepath;
}

#ifdef HYP_DOTNET
class DotNetImpl : public DotNetImplBase
{
public:
    DotNetImpl()
        : m_dll(nullptr),
          m_initializeAssemblyFptr(nullptr),
          m_unloadAssemblyFptr(nullptr),
          m_cxt(nullptr),
          m_initFptr(nullptr),
          m_getDelegateFptr(nullptr),
          m_closeFptr(nullptr)
    {
    }

    virtual ~DotNetImpl() override
    {
        if (!ShutdownDotNetRuntime())
        {
            HYP_LOG(DotNET, Error, "Failed to shutdown .NET runtime");
        }
    }

    FilePath GetDotNetPath() const
    {
        return GetResourceDirectory() / "data/dotnet";
    }

    FilePath GetLibraryPath() const
    {
        return GetDotNetPath() / "lib";
    }

    FilePath GetRuntimeConfigPath() const
    {
        return GetDotNetPath() / "runtimeconfig.json";
    }

    virtual void Initialize(const FilePath& basePath) override
    {
        m_basePath = basePath;

        // ensure the mono directories exists
        FileSystem::MkDir(GetDotNetPath().Data());
        FileSystem::MkDir(GetLibraryPath().Data());

        InitRuntimeConfig();

        // Load the .NET Core runtime
        if (!LoadHostFxr())
        {
            HYP_LOG(DotNET, Fatal, "Could not initialize .NET runtime: Failed to load hostfxr");
        }

        if (!InitDotNetRuntime())
        {
            HYP_LOG(DotNET, Fatal, "Could not initialize .NET runtime: Failed to initialize runtime");
        }

        const Optional<FilePath> interopAssemblyPath = FindAssemblyFilePath(m_basePath, "HyperionInterop.dll");

        if (!interopAssemblyPath.HasValue())
        {
            HYP_LOG(DotNET, Fatal, "Could not initialize .NET runtime: Could not locate HyperionInterop.dll!");
        }

        PlatformString interopAssemblyPathPlatform;

#ifdef HYP_WINDOWS
        interopAssemblyPathPlatform = interopAssemblyPath->ToWide();
#else
        interopAssemblyPathPlatform = *interopAssemblyPath;
#endif

        m_initializeRuntimeFptr = (InitializeRuntimeDelegate)GetDelegate(
            interopAssemblyPathPlatform.Data(),
            HYP_TEXT("Hyperion.NativeInterop, HyperionInterop"),
            HYP_TEXT("InitializeRuntime"),
            UNMANAGEDCALLERSONLY_METHOD);

        Assert(
            m_initializeRuntimeFptr != nullptr,
            "InitializeRuntime could not be found in HyperionInterop.dll! Ensure .NET libraries are properly compiled.");

        m_initializeAssemblyFptr = (InitializeAssemblyDelegate)GetDelegate(
            interopAssemblyPathPlatform.Data(),
            HYP_TEXT("Hyperion.NativeInterop, HyperionInterop"),
            HYP_TEXT("InitializeAssembly"),
            UNMANAGEDCALLERSONLY_METHOD);

        Assert(
            m_initializeAssemblyFptr != nullptr,
            "InitializeAssembly could not be found in HyperionInterop.dll! Ensure .NET libraries are properly compiled.");

        m_unloadAssemblyFptr = (UnloadAssemblyDelegate)GetDelegate(
            interopAssemblyPathPlatform.Data(),
            HYP_TEXT("Hyperion.NativeInterop, HyperionInterop"),
            HYP_TEXT("UnloadAssembly"),
            UNMANAGEDCALLERSONLY_METHOD);

        Assert(
            m_unloadAssemblyFptr != nullptr,
            "UnloadAssembly could not be found in HyperionInterop.dll! Ensure .NET libraries are properly compiled.");

        static const FixedArray<Pair<String, FilePath>, 3> coreAssemblies = {
            Pair<String, FilePath> { "interop", *interopAssemblyPath },
            Pair<String, FilePath> { "core", FindAssemblyFilePath(m_basePath, "HyperionCore.dll").GetOr([]() -> FilePath
                                                 {
                                                     HYP_FAIL("Failed to get HyperionCore.dll");
                                                     return {};
                                                 }) },
            Pair<String, FilePath> { "runtime", FindAssemblyFilePath(m_basePath, "HyperionRuntime.dll").GetOr([]() -> FilePath
                                                    {
                                                        HYP_FAIL("Failed to get HyperionRuntime.dll");
                                                        return {};
                                                    }) }
        };

        int result = m_initializeRuntimeFptr();

        if (result != int(LoadAssemblyResult::OK))
        {
            HYP_FAIL("Failed to initialize Hyperion .NET interop: Got error code %d", result);
        }

        for (const Pair<String, FilePath>& entry : coreAssemblies)
        {
            RC<Assembly> assembly = MakeRefCountedPtr<Assembly>(AssemblyFlags::CORE_ASSEMBLY);

            auto it = m_coreAssemblies.Insert(entry.first, assembly).first;

            // Initialize all core assemblies
            result = m_initializeAssemblyFptr(
                &assembly->GetGuid(),
                assembly.Get(),
                entry.second.Data(),
                /* isCoreAssembly */ 1);

            if (result != int(LoadAssemblyResult::OK))
            {
                HYP_FAIL("Failed to load core assembly %s: Got error code %d", entry.first.Data(), result);
            }
        }
    }

    virtual RC<Assembly> LoadAssembly(const char* path) const override
    {
        RC<Assembly> assembly = MakeRefCountedPtr<Assembly>();

        Optional<FilePath> filepath = FindAssemblyFilePath(m_basePath, path);

        if (!filepath.HasValue())
        {
            HYP_LOG(DotNET, Error, "Failed to load assembly {}: Could not find assembly DLL (base path: {})", path, m_basePath);
            
            return nullptr;
        }

        int result = m_initializeAssemblyFptr(
            &assembly->GetGuid(),
            assembly.Get(),
            filepath->Data(),
            /* isCoreAssembly */ 0);

        if (result != int(LoadAssemblyResult::OK))
        {
            HYP_LOG(DotNET, Error, "Failed to load assembly {}: Got error code {}", path, result);

            return nullptr;
        }

        return assembly;
    }

    virtual bool UnloadAssembly(ManagedGuid assemblyGuid) const override
    {
        if (IsCoreAssembly(assemblyGuid))
        {
            return false;
        }

        HYP_LOG(DotNET, Info, "Unloading assembly...");

        int32 result;
        m_unloadAssemblyFptr(&assemblyGuid, &result);

        return bool(result);
    }

    virtual bool IsCoreAssembly(ManagedGuid assemblyGuid) const
    {
        if (!assemblyGuid.IsValid())
        {
            return false;
        }

        auto it = m_coreAssemblies.FindIf([assemblyGuid](const auto& it)
            {
                return Memory::MemCmp(&it.second->GetGuid(), &assemblyGuid, sizeof(ManagedGuid)) == 0;
            });

        return it != m_coreAssemblies.End();
    }

    virtual bool IsCoreAssembly(const Assembly* assembly) const
    {
        if (!assembly)
        {
            return false;
        }

        return IsCoreAssembly(assembly->GetGuid());
    }

    virtual void* GetDelegate(
        const TChar* assemblyPath,
        const TChar* typeName,
        const TChar* methodName,
        const TChar* delegateTypeName) const override
    {
        if (!m_cxt)
        {
            HYP_LOG(DotNET, Fatal, "Failed to get delegate: .NET runtime not initialized");
        }

        // Get the delegate for the managed function
        void* loadAssemblyAndGetFunctionPointerFptr = nullptr;

        if (m_getDelegateFptr(m_cxt, hdt_load_assembly_and_get_function_pointer, &loadAssemblyAndGetFunctionPointerFptr) != 0)
        {
            HYP_LOG(DotNET, Error, "Failed to get delegate: Failed to get function pointer");

            return nullptr;
        }

        HYP_LOG(DotNET, Info, "Loading .NET assembly: {}\tType Name: {}\tMethod Name: {}", assemblyPath, typeName, methodName);

        void* delegatePtr = nullptr;

        auto loadAssemblyAndGetFunctionPointer = (load_assembly_and_get_function_pointer_fn)loadAssemblyAndGetFunctionPointerFptr;
        bool result = loadAssemblyAndGetFunctionPointer(assemblyPath, typeName, methodName, delegateTypeName, nullptr, &delegatePtr) == 0;

        if (!result)
        {
            HYP_LOG(DotNET, Error, "Failed to get delegate: Failed to load assembly and get function pointer");

            return nullptr;
        }

        return delegatePtr;
    }

private:
    void InitRuntimeConfig()
    {
        const FilePath filepath = GetRuntimeConfigPath();

        const FilePath currentPath = FilePath::Current();

        Array<json::JSONValue> probingPaths;

        probingPaths.PushBack(FilePath::Relative(GetLibraryPath(), currentPath));
        probingPaths.PushBack(FilePath::Relative(m_basePath, currentPath));

        const json::JSONValue runtimeConfigJson(json::JSONObject {
            { "runtimeOptions", json::JSONObject({ { "tfm", "net8.0" }, { "framework", json::JSONObject({ { "name", "Microsoft.NETCore.App" }, { "version", "8.0.1" } }) }, { "additionalProbingPaths", json::JSONArray(probingPaths) } }) } });

        String str = runtimeConfigJson.ToString(true);

        FileByteWriter writer(filepath.Data());
        writer.WriteString(str);
        writer.Close();
    }

    bool LoadHostFxr()
    {
        // Pre-allocate a large buffer for the path to hostfxr
        char_t buffer[2048];
        size_t bufferSize = sizeof(buffer) / sizeof(char_t);
        int rc = get_hostfxr_path(buffer, &bufferSize, nullptr);
        if (rc != 0)
        {
            return false;
        }

        HYP_LOG(DotNET, Debug, "Loading hostfxr from: {}", &buffer[0]);

        // Load hostfxr and get desired exports
        m_dll = DynamicLibrary::Load(buffer);

        if (!m_dll)
        {
            return false;
        }

        m_initFptr = (hostfxr_initialize_for_runtime_config_fn)m_dll->GetFunction("hostfxr_initialize_for_runtime_config");
        m_getDelegateFptr = (hostfxr_get_runtime_delegate_fn)m_dll->GetFunction("hostfxr_get_runtime_delegate");
        m_closeFptr = (hostfxr_close_fn)m_dll->GetFunction("hostfxr_close");

        HYP_LOG(DotNET, Debug, "Loaded hostfxr functions");

        return m_initFptr && m_getDelegateFptr && m_closeFptr;
    }

    bool InitDotNetRuntime()
    {
        Assert(m_cxt == nullptr);

        HYP_LOG(DotNET, Debug, "Initializing .NET runtime");

        PlatformString runtimeConfigPath;

#ifdef HYP_WINDOWS
        runtimeConfigPath = GetRuntimeConfigPath().ToWide();

        std::wcout << L".NET Runtime path = " << runtimeConfigPath.Data() << L"\n";
#else
        runtimeConfigPath = GetRuntimeConfigPath();
#endif

        if (m_initFptr(runtimeConfigPath.Data(), nullptr, &m_cxt) != 0)
        {
            HYP_LOG(DotNET, Error, "Failed to initialize .NET runtime");

            return false;
        }

        HYP_LOG(DotNET, Debug, "Initialized .NET runtime");

        return true;
    }

    bool ShutdownDotNetRuntime()
    {
        Assert(m_cxt != nullptr);

        HYP_LOG(DotNET, Debug, "Shutting down .NET runtime");

        m_closeFptr(m_cxt);
        m_cxt = nullptr;

        HYP_LOG(DotNET, Debug, "Shut down .NET runtime");

        return true;
    }

    FilePath m_basePath;

    UniquePtr<DynamicLibrary> m_dll;

    HashMap<String, RC<Assembly>> m_coreAssemblies;

    InitializeRuntimeDelegate m_initializeRuntimeFptr;
    InitializeAssemblyDelegate m_initializeAssemblyFptr;
    UnloadAssemblyDelegate m_unloadAssemblyFptr;

    hostfxr_handle m_cxt;
    hostfxr_initialize_for_runtime_config_fn m_initFptr;
    hostfxr_get_runtime_delegate_fn m_getDelegateFptr;
    hostfxr_close_fn m_closeFptr;
};

#else

class DotNetImpl : public DotNetImplBase
{
public:
    DotNetImpl() = default;
    virtual ~DotNetImpl() override = default;

    virtual void Initialize(const FilePath& basePath) override
    {
    }

    virtual RC<Assembly> LoadAssembly(const char* path) const override
    {
        return nullptr;
    }

    virtual bool UnloadAssembly(ManagedGuid guid) const override
    {
        return false;
    }

    virtual bool IsCoreAssembly(ManagedGuid guid) const override
    {
        return false;
    }

    virtual bool IsCoreAssembly(const Assembly* assembly) const override
    {
        return false;
    }

    virtual void* GetDelegate(
        const TChar* assemblyPath,
        const TChar* typeName,
        const TChar* methodName,
        const TChar* delegateTypeName) const override
    {
        return nullptr;
    }
};

#endif

DotNetSystem& DotNetSystem::GetInstance()
{
    static DotNetSystem instance;

    return instance;
}

DotNetSystem::DotNetSystem()
    : m_isInitialized(false)
{
}

DotNetSystem::~DotNetSystem() = default;

bool DotNetSystem::EnsureInitialized() const
{
    if (!IsEnabled())
    {
        HYP_LOG(DotNET, Error, "DotNetSystem not enabled, cannot load/unload assemblies");

        return false;
    }

    if (!IsInitialized())
    {
        HYP_LOG(DotNET, Error, "DotNetSystem not initialized, call Initialize() before attempting to load/unload assemblies");

        return false;
    }

    Assert(m_impl != nullptr);

    return true;
}

RC<Assembly> DotNetSystem::LoadAssembly(const char* path) const
{
    if (!EnsureInitialized())
    {
        return nullptr;
    }

    HYP_NAMED_SCOPE("Load .NET Assembly");

    return m_impl->LoadAssembly(path);
}

bool DotNetSystem::UnloadAssembly(ManagedGuid guid) const
{
    if (!EnsureInitialized())
    {
        return false;
    }

    HYP_NAMED_SCOPE("Unload .NET Assembly");

    return m_impl->UnloadAssembly(guid);
}

bool DotNetSystem::IsCoreAssembly(const Assembly* assembly) const
{
    if (!EnsureInitialized())
    {
        return false;
    }

    HYP_NAMED_SCOPE("Check if .NET Assembly is Core Assembly");

    return m_impl->IsCoreAssembly(assembly);
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
    return m_isInitialized;
}

void DotNetSystem::Initialize(const FilePath& basePath)
{
    if (!IsEnabled())
    {
        return;
    }

    if (IsInitialized())
    {
        return;
    }

    HYP_NAMED_SCOPE("Initialize .NET System");

    Assert(m_impl == nullptr);

    m_impl = MakeRefCountedPtr<DotNetImpl>();
    m_impl->Initialize(basePath);

    m_isInitialized = true;
}

void DotNetSystem::Shutdown()
{
    if (!IsEnabled())
    {
        return;
    }

    if (!IsInitialized())
    {
        return;
    }

    HYP_NAMED_SCOPE("Shutdown .NET System");

    m_impl.Reset();

    m_isInitialized = false;
}

} // namespace dotnet
} // namespace hyperion
