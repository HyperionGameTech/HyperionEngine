/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Framework/EngineGlobals.hpp>

#include <DotNET/DotNETHost.hpp>

#include <Core/CLI/CommandLine.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Core/Logging/LogChannels.hpp>
#include <Core/Logging/Logger.hpp>

#include <Core/FileSystem/FsUtil.hpp>

#include <System/AppContext.hpp>

#include <DotNET/ManagedClass.hpp>

#include <Scripting/ScriptObjectResource.hpp>

#include <Core/Reflection/ObjectPool.hpp>

#include <HyperionEngine.hpp>

namespace Hyperion {

using namespace dotnet;

// a wrapper's strong ref is only given back by its Dispose or finalizer, neither of which runs once the host is shut down
static void ReleaseManagedObjectRefs()
{
    Array<ObjectBase*> objects;

    auto collectObject = [&objects](const ObjectHeader* header)
    {
        if (header->GetRefCountStrong() == 0)
        {
            return;
        }

        ObjectBase* ptr = ObjectHeader::GetObjectPointer(const_cast<ObjectHeader*>(header));
        ScriptObjectResource* scriptObjectResource = ptr->GetScriptObjectResource();

        if (scriptObjectResource == nullptr)
        {
            return;
        }

        if (ScriptObjectData_DotNet* dotNetData = scriptObjectResource->GetScriptObjectData_DotNet(); dotNetData && dotNetData->objectPtr)
        {
            objects.PushBack(ptr);
        }
    };

    auto collectFromContainer = [&collectObject](TypeId, ObjectContainerBase* container)
    {
        if (container != nullptr)
        {
            container->ForEachHeader(collectObject);
        }
    };

    GetObjectContainerMap().ForEachContainer(collectFromContainer);

    for (ObjectBase* ptr : objects)
    {
        ptr->GetScriptObjectResource()->DetachManagedObject();
        ptr->Release();
    }
}

class DotNetImpl
{
public:
    void Initialize(const FilePath& basePath, InitFromManagedCallback initFromManagedCb)
    {
        m_basePath = basePath;

        Assert(initFromManagedCb != nullptr);
        initFromManagedCb(&m_managedDelegates);

        Assert(m_managedDelegates.initializeAssembly != nullptr);
        Assert(m_managedDelegates.unloadAssembly != nullptr);
    }

    SharedPtr<Assembly> LoadAssembly(const char* path)
    {
        constexpr ManagedGuid EmptyGuid { 0, 0 };

        Optional<FilePath> filepath = FindAssemblyFilePath(m_basePath, path);

        if (!filepath.HasValue())
        {
            HYP_LOG(DotNET, Error, "Failed to load assembly {}: Could not find assembly DLL (base path: {})", path, m_basePath);

            return nullptr;
        }

        auto it = m_assembliesByPath.Find(*filepath);

        if (it != m_assembliesByPath.End())
        {
            return it->second;
        }

        SharedPtr<Assembly> assembly = MakeShared<Assembly>(EmptyGuid);

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

    PluginModuleLoadedDelegate GetPluginModuleLoadedDelegate() const
    {
        return m_managedDelegates.pluginModuleLoaded;
    }

    UnloadAssemblyDelegate GetUnloadDelegate(ManagedGuid assemblyGuid) const
    {
        if (IsCoreAssembly(assemblyGuid))
        {
            return nullptr;
        }

        Assert(m_managedDelegates.unloadAssembly != nullptr);

        return m_managedDelegates.unloadAssembly;
    }

    bool IsCoreAssembly(ManagedGuid assemblyGuid) const
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

    bool IsCoreAssembly(const Assembly* assembly) const
    {
        if (!assembly)
        {
            return false;
        }

        return IsCoreAssembly(assembly->GetGuid());
    }

    void ReleaseAssemblies()
    {
        Map<FilePath, SharedPtr<Assembly>> assembliesByPath = std::move(m_assembliesByPath);
        assembliesByPath.Clear();
    }

    static Optional<FilePath> FindAssemblyFilePath(const FilePath& basePath, const char* path)
    {
        const FilePath filepath = FilePath(path).IsAbsolute() ? FilePath(path) : basePath / path;

        if (!filepath.Exists())
        {
            return {};
        }

        return filepath;
    }

private:
    FilePath m_basePath;
    ManagedDelegates m_managedDelegates {};

    Map<FilePath, SharedPtr<Assembly>> m_assembliesByPath;
    Map<String, ManagedGuid> m_coreAssemblies;
};

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
    if (!IsInitialized())
    {
        return false;
    }

    Assert(m_impl != nullptr);

    return true;
}

SharedPtr<Assembly> DotNETHost::LoadAssembly(const char* path) const
{
    if (!m_impl)
    {
        return nullptr;
    }

    // a hosted game has its scripts compiled in, and may have no way to load another assembly at all (NativeAOT)
    if (m_hostedScriptAssembly)
    {
        return m_hostedScriptAssembly;
    }

    return m_impl->LoadAssembly(path);
}

void DotNETHost::SetHostedScriptAssembly(Assembly* assembly)
{
    m_hostedScriptAssembly = assembly ? assembly->SharedThis() : nullptr;
}

bool DotNETHost::UnloadAssembly(ManagedGuid guid) const
{
    UnloadAssemblyDelegate unloadAssembly = nullptr;

    {
        Mutex::Guard guard(m_mutex);

        if (!m_impl)
        {
            return false;
        }

        unloadAssembly = m_impl->GetUnloadDelegate(guid);
    }

    if (!unloadAssembly)
    {
        return false;
    }

    HYP_LOG(DotNET, Verbose, "Unloading assembly...");

    int32 result = 0;
    unloadAssembly(&guid, &result);

    return bool(result);
}

void DotNETHost::NotifyPluginModuleLoaded(ANSIStringView moduleName) const
{
    PluginModuleLoadedDelegate pluginModuleLoaded = nullptr;

    {
        Mutex::Guard guard(m_mutex);

        if (!m_impl)
        {
            return;
        }

        pluginModuleLoaded = m_impl->GetPluginModuleLoadedDelegate();
    }

    if (pluginModuleLoaded)
    {
        pluginModuleLoaded(ANSIString(moduleName).Data());
    }
}

bool DotNETHost::IsCoreAssembly(const Assembly* assembly) const
{
    Mutex::Guard guard(m_mutex);

    if (!m_impl)
    {
        return false;
    }

    return m_impl->IsCoreAssembly(assembly);
}

bool DotNETHost::IsInitialized() const
{
    return m_isInitialized;
}

void DotNETHost::Initialize(const FilePath& basePath, InitFromManagedCallback initFromManagedCb)
{
    if (IsInitialized())
    {
        return;
    }

    Assert(m_impl == nullptr);

    m_impl = new DotNetImpl();
    m_impl->Initialize(basePath, initFromManagedCb);

    m_isInitialized = true;
}

void DotNETHost::Shutdown()
{
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

    m_hostedScriptAssembly.Reset();

    m_impl->ReleaseAssemblies();

    DotNetImpl* impl = nullptr;

    {
        Mutex::Guard guard(m_mutex);

        impl = m_impl;
        m_impl = nullptr;
    }

    delete impl;

    m_isInitialized = false;
    m_isShuttingDown = false;

    ReleaseManagedObjectRefs();
}

} // namespace Hyperion
