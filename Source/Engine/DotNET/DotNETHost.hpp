/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Memory/SharedPtr.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Types.hpp>

#include <DotNET/Assembly.hpp>

namespace Hyperion {

namespace dotnet {
struct ObjectReference;
class ManagedClass;
class Assembly;
struct ManagedGuid;
} // namespace dotnet

class DotNetImpl;

using AddObjectToCacheFunction = void (*)(void* ptr, dotnet::ManagedClass** outClass, dotnet::ObjectReference* outObjectReference, int8 isWeak);
using SetKeepAliveFunction = void (*)(dotnet::ObjectReference* objectReference, int32* keepAlive);
using TriggerGCFunction = void (*)(void);
using GetAssemblyPointerFunction = void (*)(dotnet::ObjectReference* assemblyObjectReference, dotnet::Assembly** pOutAssembly);
using CleanupOnShutdownFunction = void (*)(void);
using RemoveObjectFromCacheFunction = void (*)(void* classPtr);
using FreeObjectReferenceFunction = void (*)(dotnet::ObjectReference* objectReference);
using QueryManagedObjectCountsFunction = int (*)(void** outPairs, int maxPairs);
using GetTotalMemoryFunction = int64 (*)(void);
using SimTickFunction = void (*)(float deltaTime);

using InitFromManagedCallback = void (*)(struct ManagedDelegates*);

using InitializeAssemblyDelegate = int (*)(dotnet::ManagedGuid*, dotnet::Assembly*, const char*, int32);
using UnloadAssemblyDelegate = void (*)(dotnet::ManagedGuid*, int32*);
using PluginModuleLoadedDelegate = void (*)(const char*);

struct ManagedDelegates
{
    InitializeAssemblyDelegate initializeAssembly;
    UnloadAssemblyDelegate unloadAssembly;
    PluginModuleLoadedDelegate pluginModuleLoaded;
};

enum class LoadAssemblyResult : int32
{
    UNKNOWN_ERROR = -100,
    VERSION_MISMATCH = -2,
    NOT_FOUND = -1,
    OK = 0
};

class DotNETHost
{
public:
    struct GlobalFunctions
    {
        AddObjectToCacheFunction addObjectToCacheFunction = nullptr;
        SetKeepAliveFunction setKeepAliveFunction = nullptr;
        TriggerGCFunction triggerGcFunction = nullptr;
        TriggerGCFunction collectGarbageAndWaitFunction = nullptr;
        GetAssemblyPointerFunction getAssemblyPointerFunction = nullptr;
        CleanupOnShutdownFunction cleanupOnShutdownFunction = nullptr;
        RemoveObjectFromCacheFunction removeObjectFromCacheFptr = nullptr;
        FreeObjectReferenceFunction freeObjectReferenceFptr = nullptr;
        QueryManagedObjectCountsFunction queryManagedObjectCountsFptr = nullptr;
        GetTotalMemoryFunction getTotalMemoryFptr = nullptr;
        SimTickFunction simTickFptr = nullptr;
    };

    static DotNETHost& GetInstance();

    DotNETHost();

    DotNETHost(const DotNETHost&) = delete;
    DotNETHost& operator=(const DotNETHost&) = delete;

    DotNETHost(DotNETHost&&) noexcept = delete;
    DotNETHost& operator=(DotNETHost&&) noexcept = delete;

    ~DotNETHost();

    HYP_FORCE_INLINE GlobalFunctions& GetGlobalFunctions()
    {
        return m_globalFunctions;
    }

    HYP_FORCE_INLINE const GlobalFunctions& GetGlobalFunctions() const
    {
        return m_globalFunctions;
    }

    HYP_FORCE_INLINE bool IsShuttingDown() const
    {
        return m_isShuttingDown;
    }

    SharedPtr<dotnet::Assembly> LoadAssembly(const char* path) const;
    bool UnloadAssembly(dotnet::ManagedGuid guid) const;

    /*! \brief Script assemblies resolve to this one instead of loading from disk: a game hosted by HyperionApp has its scripts compiled in. */
    void SetHostedScriptAssembly(dotnet::Assembly* assembly);
    bool IsCoreAssembly(const dotnet::Assembly* assembly) const;

    void NotifyPluginModuleLoaded(ANSIStringView moduleName) const;

    bool IsInitialized() const;

    /*! \brief Connects to the .NET runtime of the managed code that started the engine.
        \param basePath The base path where assemblies are located.
        \param initFromManagedCb Fills in the managed delegates the host calls.
    */
    void Initialize(const FilePath& basePath, InitFromManagedCallback initFromManagedCb);

    void Shutdown();

private:
    bool EnsureInitialized() const;

    mutable Mutex m_mutex;
    bool m_isInitialized;
    bool m_isShuttingDown;

    DotNetImpl* m_impl;

    SharedPtr<dotnet::Assembly> m_hostedScriptAssembly;

    GlobalFunctions m_globalFunctions;
};
} // namespace Hyperion
