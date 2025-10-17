/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/RefCountedPtr.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/Types.hpp>

#include <dotnet/Assembly.hpp>

namespace hyperion {

namespace dotnet {

struct ObjectReference;
class ManagedClass;

class DotNetImplBase;
class DotNetImpl;

using AddObjectToCacheFunction = void (*)(void* ptr, ManagedClass** outClass, ObjectReference* outObjectReference, int8 isWeak);
using SetKeepAliveFunction = void (*)(ObjectReference* objectReference, int32* keepAlive);
using TriggerGCFunction = void (*)();
using GetAssemblyPointerFunction = void (*)(ObjectReference* assemblyObjectReference, Assembly** outAssemblyPtr);

class DotNetSystem
{
public:
    struct GlobalFunctions
    {
        AddObjectToCacheFunction addObjectToCacheFunction = nullptr;
        SetKeepAliveFunction setKeepAliveFunction = nullptr;
        TriggerGCFunction triggerGcFunction = nullptr;
        GetAssemblyPointerFunction getAssemblyPointerFunction = nullptr;
    };

    static DotNetSystem& GetInstance();

    DotNetSystem();

    DotNetSystem(const DotNetSystem&) = delete;
    DotNetSystem& operator=(const DotNetSystem&) = delete;
    DotNetSystem(DotNetSystem&&) noexcept = delete;
    DotNetSystem& operator=(DotNetSystem&&) noexcept = delete;
    ~DotNetSystem();

    HYP_FORCE_INLINE GlobalFunctions& GetGlobalFunctions()
    {
        return m_globalFunctions;
    }

    HYP_FORCE_INLINE const GlobalFunctions& GetGlobalFunctions() const
    {
        return m_globalFunctions;
    }

    RC<Assembly> LoadAssembly(const char* path) const;
    bool UnloadAssembly(ManagedGuid guid) const;
    bool IsCoreAssembly(const Assembly* assembly) const;

    bool IsEnabled() const;

    bool IsInitialized() const;

    void Initialize(const FilePath& basePath);
    void Shutdown();

private:
    bool EnsureInitialized() const;

    bool m_isInitialized;
    RC<DotNetImplBase> m_impl;

    GlobalFunctions m_globalFunctions;
};

} // namespace dotnet
} // namespace hyperion
