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

using AddObjectToCacheFunction = void(*)(void *ptr, Class **out_class_object_ptr, ObjectReference *out_object_reference);

class DotNetSystem
{
public:
    static DotNetSystem &GetInstance();

    DotNetSystem();

    DotNetSystem(const DotNetSystem &)                  = delete;
    DotNetSystem &operator=(const DotNetSystem &)       = delete;
    DotNetSystem(DotNetSystem &&) noexcept              = delete;
    DotNetSystem &operator=(DotNetSystem &&) noexcept   = delete;
    ~DotNetSystem();

    UniquePtr<Assembly> LoadAssembly(const char *path) const;
    bool UnloadAssembly(ManagedGuid guid) const;

    HYP_FORCE_INLINE AddObjectToCacheFunction GetAddObjectToCacheFunction() const
        { return m_add_object_to_cache_fptr; }

    HYP_FORCE_INLINE void SetAddObjectToCacheFunction(AddObjectToCacheFunction add_object_to_cache_fptr)
        { m_add_object_to_cache_fptr = add_object_to_cache_fptr; }

    bool IsEnabled() const;

    bool IsInitialized() const;

    void Initialize(const FilePath &base_path);
    void Shutdown();

private:
    bool EnsureInitialized() const;

    bool                        m_is_initialized;
    RC<detail::DotNetImplBase>  m_impl;

    AddObjectToCacheFunction    m_add_object_to_cache_fptr;
};

} // namespace dotnet
} // namespace hyperion

#endif