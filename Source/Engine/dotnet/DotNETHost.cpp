/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <dotnet/DotNETHost.hpp>

#include <Core/io/ByteWriter.hpp>

#include <Core/cli/CommandLine.hpp>

#include <Core/dll/DynamicLibrary.hpp>

#include <Core/memory/pool/Pool.hpp>

#include <Core/logging/LogChannels.hpp>
#include <Core/logging/Logger.hpp>

#include <Core/filesystem/FsUtil.hpp>
#include <Core/json/JSON.hpp>

#include <system/AppContext.hpp>

#include <dotnet/ManagedClass.hpp>

#ifdef HYP_DOTNET
#include <dotnetcore/hostfxr.h>
#include <dotnetcore/nethost.h>
#include <dotnetcore/coreclr_delegates.h>
#endif

#include <HyperionEngine.hpp>

#include <iostream>

namespace Hyperion {

using namespace dotnet;

class DotNetImplBase
{
public:
    virtual ~DotNetImplBase() = default;

    virtual void Initialize(const FilePath& basePath, bool initFromManaged = false, InitFromManagedCallback initFromManagedCb = nullptr) = 0;
    virtual RC<Assembly> LoadAssembly(const char* path) = 0;
    virtual bool UnloadAssembly(ManagedGuid guid) = 0;
    virtual bool IsCoreAssembly(ManagedGuid guid) const = 0;
    virtual bool IsCoreAssembly(const Assembly* assembly) const = 0;

    virtual void* GetDelegate(
        const TChar* assemblyPath,
        const TChar* typeName,
        const TChar* methodName,
        const TChar* delegateTypeName) const = 0;
};

static Optional<FilePath> FindAssemblyFilePath(const FilePath& basePath, const char* path)
{
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
        : m_managedDelegates {},
          m_cxt(nullptr),
          m_initFptr(nullptr),
          m_getDelegateFptr(nullptr),
          m_closeFptr(nullptr),
          m_shouldInitializeRuntime(true)
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
        return GetCacheDirectory() / "DotNET";
    }

    FilePath GetLibraryPath() const
    {
        return GetDotNetPath() / "lib";
    }

    FilePath GetRuntimeConfigPath() const
    {
        return GetDotNetPath() / "runtimeconfig.json";
    }

    virtual void Initialize(const FilePath& basePath, bool initFromManaged = false, InitFromManagedCallback initFromManagedCb = nullptr) override
    {
        m_basePath = basePath;

        if (initFromManaged)
        {
            Assert(initFromManagedCb != nullptr);
            initFromManagedCb(&m_managedDelegates);

            // @NOTE initializeRuntime will be null when initializing from managed code

            Assert(m_managedDelegates.initializeAssembly != nullptr);
            Assert(m_managedDelegates.unloadAssembly != nullptr);

            return;
        }

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

        const Optional<FilePath> interopAssemblyPath = FindAssemblyFilePath(m_basePath, "Hyperion.NET.Interop.dll");

        if (!interopAssemblyPath.HasValue())
        {
            HYP_LOG(DotNET, Fatal, "Could not initialize .NET runtime: Could not locate Hyperion.NET.Interop.dll! Base path: {}", m_basePath);
        }

        PlatformString interopAssemblyPathPlatform;

#ifdef HYP_WINDOWS
        interopAssemblyPathPlatform = interopAssemblyPath->ToWide();
#else
        interopAssemblyPathPlatform = *interopAssemblyPath;
#endif

        m_managedDelegates.initializeRuntime = (InitializeRuntimeDelegate)GetDelegate(
            interopAssemblyPathPlatform.Data(),
            HYP_TEXT("Hyperion.NativeInterop, Hyperion.NET.Interop"),
            HYP_TEXT("InitializeRuntime"),
            UNMANAGEDCALLERSONLY_METHOD);

        Assert(
            m_managedDelegates.initializeRuntime != nullptr,
            "InitializeRuntime could not be found in Hyperion.NET.Interop.dll! Ensure .NET libraries are properly compiled.");

        m_managedDelegates.initializeAssembly = (InitializeAssemblyDelegate)GetDelegate(
            interopAssemblyPathPlatform.Data(),
            HYP_TEXT("Hyperion.NativeInterop, Hyperion.NET.Interop"),
            HYP_TEXT("InitializeAssembly"),
            UNMANAGEDCALLERSONLY_METHOD);

        Assert(
            m_managedDelegates.initializeAssembly != nullptr,
            "InitializeAssembly could not be found in Hyperion.NET.Interop.dll! Ensure .NET libraries are properly compiled.");

        m_managedDelegates.unloadAssembly = (UnloadAssemblyDelegate)GetDelegate(
            interopAssemblyPathPlatform.Data(),
            HYP_TEXT("Hyperion.NativeInterop, Hyperion.NET.Interop"),
            HYP_TEXT("UnloadAssembly"),
            UNMANAGEDCALLERSONLY_METHOD);

        Assert(
            m_managedDelegates.unloadAssembly != nullptr,
            "UnloadAssembly could not be found in Hyperion.NET.Interop.dll! Ensure .NET libraries are properly compiled.");

        static const Array<Pair<String, FilePath>> s_coreAssemblies = {
            Pair<String, FilePath> { "interop", *interopAssemblyPath }
            //Pair<String, FilePath> { "runtime", FindAssemblyFilePath(m_basePath, "Hyperion.NET.Runtime.dll").GetOr(FilePath()) }
        };

        int result = int(LoadAssemblyResult::OK);

        Assert(m_managedDelegates.initializeRuntime != nullptr);
        result = m_managedDelegates.initializeRuntime();

        if (result != int(LoadAssemblyResult::OK))
        {
            HYP_FAIL("Failed to initialize Hyperion .NET interop: Got error code {}", result);
        }

        for (const Pair<String, FilePath>& entry : s_coreAssemblies)
        {
            RC<Assembly> assembly = MakeRefCountedPtr<Assembly>(
                ManagedGuid { 0, 0 },
                AssemblyFlags::CORE_ASSEMBLY);

            HYP_LOG(DotNET, Verbose, "Loading core assembly: {}", entry.first);

            // Initialize all core assemblies
            result = m_managedDelegates.initializeAssembly(
                &assembly->GetGuid(),
                assembly.Get(),
                entry.second.Data(),
                /* isCoreAssembly */ 1);

            if (result != int(LoadAssemblyResult::OK))
            {
                HYP_FAIL("Failed to load assembly `{}`: Got error code {}", entry.first.Data(), result);
            }

            m_coreAssemblies.Insert(entry.first, assembly->GetGuid());
            m_assembliesByPath.Insert(entry.second, assembly);
        }

        LoadAssembly("Hyperion.NET.Runtime.dll");
    }

    virtual RC<Assembly> LoadAssembly(const char* path) override
    {
        constexpr ManagedGuid EmptyGuid { 0, 0 };

        Optional<FilePath> filepath = FindAssemblyFilePath(m_basePath, path);

        auto it = m_assembliesByPath.Find(*filepath);

        if (it != m_assembliesByPath.End())
        {
            return it->second;
        }

        if (!filepath.HasValue())
        {
            HYP_LOG(DotNET, Error, "Failed to load assembly {}: Could not find assembly DLL (base path: {})", path, m_basePath);

            return nullptr;
        }

        RC<Assembly> assembly = MakeRefCountedPtr<Assembly>(EmptyGuid);

        Assert(m_managedDelegates.initializeAssembly != nullptr);

        int result = m_managedDelegates.initializeAssembly(
            &assembly->GetGuid(),
            assembly.Get(),
            filepath->Data(),
            /* isCoreAssembly */ 0);

        if (result != int(LoadAssemblyResult::OK))
        {
            HYP_LOG(DotNET, Error, "Failed to load assembly {}: Got error code {}", path, result);

            return nullptr;
        }

        m_assembliesByPath[*filepath] = assembly;

        return assembly;
    }

    virtual bool UnloadAssembly(ManagedGuid assemblyGuid) override
    {
        if (IsCoreAssembly(assemblyGuid))
        {
            return false;
        }

        HYP_LOG(DotNET, Verbose, "Unloading assembly...");

        int32 result;

        Assert(m_managedDelegates.unloadAssembly != nullptr);
        m_managedDelegates.unloadAssembly(&assemblyGuid, &result);

        return bool(result);
    }

    virtual bool IsCoreAssembly(ManagedGuid assemblyGuid) const override
    {
        if (!assemblyGuid.IsValid())
        {
            return false;
        }

        for (const auto& pair : m_coreAssemblies)
        {
            if (pair.second == assemblyGuid)
            {
                return true;
            }
        }

        return false;
    }

    virtual bool IsCoreAssembly(const Assembly* assembly) const override
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

        HYP_LOG(DotNET, Verbose, "Loading .NET assembly: {}\tType Name: {}\tMethod Name: {}", assemblyPath, typeName, methodName);

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

        Array<JSON::Value> probingPaths;

        probingPaths.PushBack(FilePath::Relative(GetLibraryPath(), currentPath));
        probingPaths.PushBack(FilePath::Relative(m_basePath, currentPath));

        // clang-format off
        const JSON::Value runtimeConfigJson(JSON::Object {
            {
                "runtimeOptions", JSON::Object({
                    { "tfm", "net9.0" },
                    { "framework", JSON::Object({
                        { "name", "Microsoft.NETCore.App" },
                        { "version", "9.0.3" } })
                    },
                    { "additionalProbingPaths", JSON::JArray(probingPaths) }
                })
            }
        });
        // clang-format on

        String str = runtimeConfigJson.ToString(true);

        FileByteWriter writer(filepath.Data());
        writer.WriteString(str);
        writer.Close();
    }

    bool LoadHostFxr()
    {
        TChar wbuffer[2048];
        size_t bufferSize = sizeof(wbuffer) / sizeof(TChar);

        PlatformString wpath;

        int rc = get_hostfxr_path(wbuffer, &bufferSize, nullptr);

        if (rc == 0)
        {
            wpath = PlatformString(wbuffer, wbuffer + bufferSize - 1);
        }
        else if (rc == -1)
        {
            // try again with a larger buffer
            Array<TChar> dyn_wbuffer;
            dyn_wbuffer.Resize(bufferSize);

            rc = get_hostfxr_path(dyn_wbuffer.Data(), &bufferSize, nullptr);

            if (rc != 0)
            {
                HYP_LOG(DotNET, Error, "Failed to load hostfxr: get_hostfxr_path failed with error code {}", rc);

                return false;
            }

            wpath = PlatformString(dyn_wbuffer.Data(), dyn_wbuffer.Data() + bufferSize - 1);
        }
        else
        {
            HYP_LOG(DotNET, Error, "Failed to load hostfxr: get_hostfxr_path failed with error code {}", rc);

            return false;
        }

        HYP_LOG(DotNET, Verbose, "Loading hostfxr from: {}", wpath);

        if (!FilePath(wpath).Exists())
        {
            HYP_LOG(DotNET, Error, "Failed to load hostfxr: hostfxr does not exist at path {}", wpath);
            return false;
        }

        // Load hostfxr and get desired exports
        m_dll = DynamicLibrary(wpath);

        if (!m_dll.Open())
        {
            AssertDebug(false, "Failed to load hostfxr library at {}", wpath);
            return false;
        }

        m_initFptr = (hostfxr_initialize_for_runtime_config_fn)m_dll.GetFunction("hostfxr_initialize_for_runtime_config");
        m_getDelegateFptr = (hostfxr_get_runtime_delegate_fn)m_dll.GetFunction("hostfxr_get_runtime_delegate");
        m_closeFptr = (hostfxr_close_fn)m_dll.GetFunction("hostfxr_close");

        AssertDebug(m_initFptr && m_getDelegateFptr && m_closeFptr);

        HYP_LOG(DotNET, Verbose, "Loaded hostfxr functions");

        return m_initFptr && m_getDelegateFptr && m_closeFptr;
    }

    bool InitDotNetRuntime()
    {
        Assert(m_cxt == nullptr);

        HYP_LOG(DotNET, Verbose, "Initializing .NET runtime");

        PlatformString runtimeConfigPath;

#ifdef HYP_WINDOWS
        runtimeConfigPath = GetRuntimeConfigPath().ToWide();

        std::wcout << L".NET Runtime path = " << runtimeConfigPath.Data() << L"\n";
#else
        runtimeConfigPath = GetRuntimeConfigPath();
#endif

        const TChar* runtimeConfigPathCStr = runtimeConfigPath.Data();

        int res = m_initFptr(runtimeConfigPathCStr, nullptr, &m_cxt);

        // https://github.com/dotnet/runtime/blob/main/docs/design/features/host-error-codes.md
        switch (res)
        {
        case /* Success */ 0:
            HYP_LOG(DotNET, Verbose, "Initialized .NET runtime");

            m_shouldInitializeRuntime = true;

            return true;
        case /* Success_HostAlreadyInitialized */ 1: // fallthrough
        case /* Success_DifferentRuntimeProperties */ 2:
            HYP_LOG(DotNET, Verbose, "Initialized .NET runtime, hostfxr_initialize_for_runtime_config returned {}", res);

            m_shouldInitializeRuntime = false;

            return true;
        default:
            HYP_LOG(DotNET, Error, "Failed to initialize .NET runtime: hostfxr_initialize_for_runtime_config failed with error code {}", res);

            return false;
        }
    }

    bool ShutdownDotNetRuntime()
    {
        m_assembliesByPath.Clear();
        m_coreAssemblies.Clear();

        // can be nullptr if init from managed
        if (m_cxt != nullptr)
        {
            HYP_LOG(DotNET, Verbose, "Shutting down .NET runtime");

            Assert(m_closeFptr != nullptr);
            m_closeFptr(m_cxt);

            m_cxt = nullptr;

            m_shouldInitializeRuntime = true;

            HYP_LOG(DotNET, Verbose, "Shut down .NET runtime");
        }

        m_dll.Close();

        return true;
    }

    FilePath m_basePath;

    DynamicLibrary m_dll;

    ManagedDelegates m_managedDelegates;

    HashMap<FilePath, RC<Assembly>> m_assembliesByPath;
    HashMap<String, ManagedGuid> m_coreAssemblies;

    hostfxr_handle m_cxt;
    hostfxr_initialize_for_runtime_config_fn m_initFptr;
    hostfxr_get_runtime_delegate_fn m_getDelegateFptr;
    hostfxr_close_fn m_closeFptr;

    bool m_shouldInitializeRuntime;
};

#else

class DotNetImpl : public DotNetImplBase
{
public:
    DotNetImpl() = default;
    virtual ~DotNetImpl() override = default;

    virtual void Initialize(const FilePath& basePath, bool initFromManaged = false, InitFromManagedCallback initFromManagedCb = nullptr) override
    {
    }

    virtual RC<Assembly> LoadAssembly(const char* path) override
    {
        return nullptr;
    }

    virtual bool UnloadAssembly(ManagedGuid guid) override
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

DotNETHost& DotNETHost::GetInstance()
{
    static DotNETHost s_instance;

    return s_instance;
}

DotNETHost::DotNETHost()
    : m_isInitialized(false),
      m_isShuttingDown(false),
      m_impl(nullptr)
{
}

DotNETHost::~DotNETHost()
{
    if (m_impl != nullptr)
    {
        Shutdown();
    }
}

bool DotNETHost::EnsureInitialized() const
{
    if (!IsEnabled())
    {
        return false;
    }

    if (!IsInitialized())
    {
        return false;
    }

    Assert(m_impl != nullptr);

    return true;
}

RC<Assembly> DotNETHost::LoadAssembly(const char* path) const
{
    if (!EnsureInitialized())
    {
        return nullptr;
    }

    return m_impl->LoadAssembly(path);
}

bool DotNETHost::UnloadAssembly(ManagedGuid guid) const
{
    if (!EnsureInitialized())
    {
        return false;
    }

    return m_impl->UnloadAssembly(guid);
}

bool DotNETHost::IsCoreAssembly(const Assembly* assembly) const
{
    if (!EnsureInitialized())
    {
        return false;
    }

    return m_impl->IsCoreAssembly(assembly);
}

bool DotNETHost::IsEnabled() const
{
#ifndef HYP_DOTNET
    return false;
#else
    return true;
#endif
}

bool DotNETHost::IsInitialized() const
{
    return m_isInitialized;
}

void DotNETHost::Initialize(const FilePath& basePath, bool initFromManaged, InitFromManagedCallback initFromManagedCb)
{
    if (!IsEnabled())
    {
        return;
    }

    if (IsInitialized())
    {
        return;
    }

    Assert(m_impl == nullptr);

    m_impl = new DotNetImpl();
    m_impl->Initialize(basePath, initFromManaged, initFromManagedCb);

    m_isInitialized = true;
}

void DotNETHost::Shutdown()
{
    if (!IsEnabled())
    {
        return;
    }

    if (!IsInitialized())
    {
        return;
    }

    m_isShuttingDown = true;

    if (m_globalFunctions.cleanupOnShutdownFunction != nullptr)
    {
        m_globalFunctions.cleanupOnShutdownFunction();
    }

    m_globalFunctions = {};

    delete m_impl;
    m_impl = nullptr;

    m_isInitialized = false;
    m_isShuttingDown = false;
}

} // namespace Hyperion
