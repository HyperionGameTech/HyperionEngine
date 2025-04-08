/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_DOT_NET_SYSTEM_HPP
#define HYPERION_DOTNET_DOT_NET_SYSTEM_HPP

#include <core/memory/RefCountedPtr.hpp>

#include <core/filesystem/FilePath.hpp>

#include <Types.hpp>

#include <dotnet/Assembly.hpp>

namespace hyperion {

namespace dotnet {

struct ObjectReference;
class Class;

namespace detail {

class DotNetImplBase;
class DotNetImpl;

} // namespace detail

using AddObjectToCacheFunction = void(*)(void *ptr, Class **out_class_object_ptr, ObjectReference *out_object_reference, int8 is_weak);
using SetObjectReferenceTypeFunction = int8(*)(ObjectReference *object_reference, int8 is_weak);
using TriggerGCFunction = void(*)();

class DotNetSystem
{
public:
    struct GlobalFunctions
    {
        AddObjectToCacheFunction        add_object_to_cache_function = nullptr;
        SetObjectReferenceTypeFunction  set_object_reference_type_function = nullptr;
        TriggerGCFunction               trigger_gc_function = nullptr;
    };

    static DotNetSystem &GetInstance();

    DotNetSystem();

    DotNetSystem(const DotNetSystem &)                  = delete;
    DotNetSystem &operator=(const DotNetSystem &)       = delete;
    DotNetSystem(DotNetSystem &&) noexcept              = delete;
    DotNetSystem &operator=(DotNetSystem &&) noexcept   = delete;
    ~DotNetSystem();

    HYP_FORCE_INLINE GlobalFunctions &GetGlobalFunctions()
        { return m_global_functions; }

    HYP_FORCE_INLINE const GlobalFunctions &GetGlobalFunctions() const
        { return m_global_functions; }

    RC<Assembly> LoadAssembly(const char *path) const;
    bool UnloadAssembly(ManagedGuid guid) const;

    bool IsEnabled() const;

    bool IsInitialized() const;

    void Initialize(const FilePath &base_path);
    void Shutdown();

private:
    bool EnsureInitialized() const;

    bool                        m_is_initialized;
    RC<detail::DotNetImplBase>  m_impl;

    GlobalFunctions             m_global_functions;
};

} // namespace dotnet
} // namespace hyperion

#endif