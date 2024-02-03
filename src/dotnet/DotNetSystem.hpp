#ifndef HYPERION_DOTNET_DOT_NET_SYSTEM_HPP
#define HYPERION_DOTNET_DOT_NET_SYSTEM_HPP

#include <core/lib/RefCountedPtr.hpp>

#include <dotnet/Types.hpp>
#include <dotnet/Assembly.hpp>
#include <dotnet/Class.hpp>
#include <dotnet/Object.hpp>
#include <dotnet/interop/ManagedObject.hpp>

namespace hyperion {
namespace dotnet {
namespace detail {

class DotNetImplBase
{
public:
    virtual ~DotNetImplBase() = default;

    virtual RC<Assembly> LoadAssembly(const char *path) const = 0;

    virtual void *GetDelegate(
        const char *assembly_path,
        const char *type_name,
        const char *method_name,
        const char *delegate_type_name
    ) const = 0;
};

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

    bool IsEnabled() const;

    bool IsInitialized() const;

    void Initialize();
    void Shutdown();

private:
    bool                        m_is_initialized;
    RC<detail::DotNetImplBase>  m_impl;
};

} // namespace dotnet
} // namespace hyperion

#endif