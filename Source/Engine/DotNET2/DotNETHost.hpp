/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Memory/RefCountedPtr.hpp>

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

class DotNetImplBase;
class DotNetImpl;

using AddObjectToCacheFunction = void (*)(void* ptr, dotnet::ManagedClass** outClass, dotnet::ObjectReference* outObjectReference, int8 isWeak);
using SetKeepAliveFunction = void (*)(dotnet::ObjectReference* objectReference, int32* keepAlive);
using TriggerGCFunction = void (*)(void);
using GetAssemblyPointerFunction = void (*)(dotnet::ObjectReference* assemblyObjectReference, dotnet::Assembly** pOutAssembly);
using CleanupOnShutdownFunction = void (*)(void);

using InitFromManagedCallback = void (*)(struct ManagedDelegates*);

using InitializeRuntimeDelegate = int (*)(void);
using InitializeAssemblyDelegate = int (*)(dotnet::ManagedGuid*, dotnet::Assembly*, const char*, int32);
using UnloadAssemblyDelegate = void (*)(dotnet::ManagedGuid*, int32*);

struct ManagedDelegates
{
    InitializeRuntimeDelegate initializeRuntime;
    InitializeAssemblyDelegate initializeAssembly;
    UnloadAssemblyDelegate unloadAssembly;
};

enum class LoadAssemblyResult : int32
{
    UNKNOWN_ERROR = -100,
    VERSION_MISMATCH = -2,
    NOT_FOUND = -1,
    OK = 0
};

class ENGINE_API DotNETHost
{
public:
    struct GlobalFunctions
    {
        AddObjectToCacheFunction addObjectToCacheFunction = nullptr;
        SetKeepAliveFunction setKeepAliveFunction = nullptr;
        TriggerGCFunction triggerGcFunction = nullptr;
        GetAssemblyPointerFunction getAssemblyPointerFunction = nullptr;
        CleanupOnShutdownFunction cleanupOnShutdownFunction = nullptr;

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

    RC<dotnet::Assembly> LoadAssembly(const char* path) const;
    bool UnloadAssembly(dotnet::ManagedGuid guid) const;
    bool IsCoreAssembly(const dotnet::Assembly* assembly) const;

    bool IsEnabled() const;

    bool IsInitialized() const;

    /*! \brief Initializes the .NET runtime and loads core assemblies.
        \param basePath The base path where assemblies are located.
        \param initFromManaged Whether this initialization is being initiated from managed code. If true, some initialization steps may be skipped.
        \param callback An optional callback function, used to initialize from managed code (only when \p initFromManaged is true).
    */
    void Initialize(const FilePath& basePath, bool initFromManaged = false, InitFromManagedCallback initFromManagedCb = nullptr);

    void Shutdown();

private:
    bool EnsureInitialized() const;

    mutable Mutex m_mutex;
    bool m_isInitialized;
    bool m_isShuttingDown;

    DotNetImplBase* m_impl;

    GlobalFunctions m_globalFunctions;
};
} // namespace Hyperion
