#ifndef HYP_DOTNET_SUPPORT_ASSEMBLY_HPP
#define HYP_DOTNET_SUPPORT_ASSEMBLY_HPP

#include <core/lib/RefCountedPtr.hpp>

#include <dotnet_support/Types.hpp>

namespace hyperion {
namespace dotnet {

namespace detail {
class DotNetImplBase;
} // namespace detail

class Assembly
{
public:
    // Dependency injection
    Assembly(
        RC<detail::DotNetImplBase> impl
    );

    Delegate GetDelegate(const char *name) const;

private:
    RC<detail::DotNetImplBase>  m_impl;
};

} // namespace dotnet
} // namespace hyperion

#endif