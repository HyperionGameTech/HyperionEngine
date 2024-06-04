/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_DOT_NET_SYSTEM_HPP
#define HYPERION_DOTNET_DOT_NET_SYSTEM_HPP

#include <core/memory/RefCountedPtr.hpp>

#include <Types.hpp>

#include <dotnet/Assembly.hpp>

namespace hyperion {
namespace dotnet {

struct ManagedObject;

namespace detail {

class DotNetImplBase;
class DotNetImpl;

} // namespace detail

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

    RC<Assembly> LoadAssembly(const char *path) const;
    bool UnloadAssembly(ManagedGuid guid) const;

    void AddMethodToCache(ManagedGuid assembly_guid, ManagedGuid method_guid, void *method_info_ptr) const;
    void AddObjectToCache(ManagedGuid assembly_guid, ManagedGuid object_guid, void *object_ptr, ManagedObject *out_managed_object) const;

    bool IsEnabled() const;

    bool IsInitialized() const;

    void Initialize();
    void Shutdown();

private:
    bool EnsureInitialized() const;

    bool                        m_is_initialized;
    RC<detail::DotNetImplBase>  m_impl;
};

} // namespace dotnet
} // namespace hyperion

#endif