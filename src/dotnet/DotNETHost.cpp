/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <dotnet/DotNETHost.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/cli/CommandLine.hpp>

#include <core/dll/DynamicLibrary.hpp>

#include <core/memory/pool/Pool.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/filesystem/FsUtil.hpp>
#include <core/json/JSON.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <system/AppContext.hpp>
#include <system/App.hpp>

#include <dotnet/ManagedClass.hpp>

#ifdef HYP_DOTNET
#include <dotnetcore/hostfxr.h>
#include <dotnetcore/nethost.h>
#include <dotnetcore/coreclr_delegates.h>
#endif

#include <HyperionEngine.hpp>

#include <iostream>

namespace hyperion {

using namespace dotnet;

class DotNetImplBase
{
public:
    virtual ~DotNetImplBase() = default;

    virtual void Initialize(const FilePath& basePath, bool initFromManaged = false, InitFromManagedCallback initFromManagedCb = nullptr) = 0;
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
            Pair<String, FilePath> { "interop", *interopAssemblyPath },
            Pair<String, FilePath> { "shared", FindAssemblyFilePath(m_basePath, "Hyperion.NET.Shared.dll").GetOr([]() -> FilePath
                                                   {
                                                       HYP_FAIL("Failed to get Hyperion.NET.Shared.dll");
                                                       return {};
                                                   }) },
            Pair<String, FilePath> { "runtime", FindAssemblyFilePath(m_basePath, "Hyperion.NET.Runtime.dll").GetOr([]() -> FilePath
                                                    {
                                                        HYP_FAIL("Failed to get Hyperion.NET.Runtime.dll");
                                                        return {};
                                                    }) }
        };

        int result = int(LoadAssemblyResult::OK);

        Assert(m_managedDelegates.initializeRuntime != nullptr);
        result = m_managedDelegates.initializeRuntime();

        if (result != int(LoadAssemblyResult::OK))
        {
            HYP_FAIL("Failed to initialize Hyperion .NET interop: Got error code %d", result);
        }

        constexpr ManagedGuid EmptyGuid { 0, 0 };

        for (const Pair<String, FilePath>& entry : s_coreAssemblies)
        {
            RC<Assembly> assembly = MakeRefCountedPtr<Assembly>(
                EmptyGuid,
                AssemblyFlags::CORE_ASSEMBLY);

            HYP_LOG(DotNET, Info, "Loading core assembly: {}", entry.first);

            auto it = m_coreAssemblies.Insert(entry.first, assembly).first;

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
        }
    }

    virtual RC<Assembly> LoadAssembly(const char* path) const override
    {
        constexpr ManagedGuid EmptyGuid { 0, 0 };

        RC<Assembly> assembly = MakeRefCountedPtr<Assembly>(EmptyGuid);

        Optional<FilePath> filepath = FindAssemblyFilePath(m_basePath, path);

        if (!filepath.HasValue())
        {
            HYP_LOG(DotNET, Error, "Failed to load assembly {}: Could not find assembly DLL (base path: {})", path, m_basePath);

            return nullptr;
        }

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

        auto it = m_coreAssemblies.FindIf([assemblyGuid](const auto& it)
            {
                return Memory::MemCmp(&it.second->GetGuid(), &assemblyGuid, sizeof(ManagedGuid)) == 0;
            });

        return it != m_coreAssemblies.End();
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

        // clang-format off
        const json::JSONValue runtimeConfigJson(json::JSONObject {
            {
                "runtimeOptions", json::JSONObject({
                    { "tfm", "net9.0" },
                    { "framework", json::JSONObject({
                        { "name", "Microsoft.NETCore.App" },
                        { "version", "9.0.3" } })
                    },
                    { "additionalProbingPaths", json::JSONArray(probingPaths) }
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
        SizeType bufferSize = sizeof(wbuffer) / sizeof(TChar);

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

        HYP_LOG(DotNET, Debug, "Loading hostfxr from: {}", wpath);

        if (!FilePath(wpath).Exists())
        {
            HYP_LOG(DotNET, Error, "Failed to load hostfxr: hostfxr does not exist at path {}", wpath);
            return false;
        }

        // Load hostfxr and get desired exports
        m_dll = DynamicLibrary(wpath.ToUTF8());

        if (!m_dll.Load())
        {
            return false;
        }

        m_initFptr = (hostfxr_initialize_for_runtime_config_fn)m_dll.GetFunction("hostfxr_initialize_for_runtime_config");
        m_getDelegateFptr = (hostfxr_get_runtime_delegate_fn)m_dll.GetFunction("hostfxr_get_runtime_delegate");
        m_closeFptr = (hostfxr_close_fn)m_dll.GetFunction("hostfxr_close");

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

        int res = m_initFptr(runtimeConfigPath.Data(), nullptr, &m_cxt);

        // https://github.com/dotnet/runtime/blob/main/docs/design/features/host-error-codes.md
        switch (res)
        {
        case /* Success */ 0:
            HYP_LOG(DotNET, Debug, "Initialized .NET runtime");

            m_shouldInitializeRuntime = true;

            return true;
        case /* Success_HostAlreadyInitialized */ 1: // fallthrough
        case /* Success_DifferentRuntimeProperties */ 2:
            HYP_LOG(DotNET, Debug, "Initialized .NET runtime, hostfxr_initialize_for_runtime_config returned {}", res);

            m_shouldInitializeRuntime = false;

            return true;
        default:
            HYP_LOG(DotNET, Error, "Failed to initialize .NET runtime: hostfxr_initialize_for_runtime_config failed with error code {}", res);

            return false;
        }
    }

    bool ShutdownDotNetRuntime()
    {
        Assert(m_cxt != nullptr);

        HYP_LOG(DotNET, Debug, "Shutting down .NET runtime");

        m_closeFptr(m_cxt);
        m_cxt = nullptr;

        m_shouldInitializeRuntime = true;

        HYP_LOG(DotNET, Debug, "Shut down .NET runtime");

        return true;
    }

    FilePath m_basePath;

    DynamicLibrary m_dll;

    ManagedDelegates m_managedDelegates;

    HashMap<String, RC<Assembly>> m_coreAssemblies;

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

DotNETHost& DotNETHost::GetInstance()
{
    static DotNETHost s_instance;

    return s_instance;
}

DotNETHost::DotNETHost()
    : m_isInitialized(false),
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
        HYP_LOG(DotNET, Error, "DotNETHost not enabled, cannot load/unload assemblies");

        return false;
    }

    if (!IsInitialized())
    {
        HYP_LOG(DotNET, Error, "DotNETHost not initialized, call Initialize() before attempting to load/unload assemblies");

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

    HYP_NAMED_SCOPE("Load .NET Assembly");

    return m_impl->LoadAssembly(path);
}

bool DotNETHost::UnloadAssembly(ManagedGuid guid) const
{
    if (!EnsureInitialized())
    {
        return false;
    }

    HYP_NAMED_SCOPE("Unload .NET Assembly");

    return m_impl->UnloadAssembly(guid);
}

bool DotNETHost::IsCoreAssembly(const Assembly* assembly) const
{
    if (!EnsureInitialized())
    {
        return false;
    }

    HYP_NAMED_SCOPE("Check if .NET Assembly is Core Assembly");

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

    HYP_NAMED_SCOPE("Initialize .NET System");

    Assert(m_impl == nullptr);

    HYP_LOG(DotNET, Info, "Initializing .NET Host with base path: {}", basePath);

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

    HYP_NAMED_SCOPE("Shutdown .NET System");

    delete m_impl;
    m_impl = nullptr;

    m_isInitialized = false;
}

} // namespace hyperion
