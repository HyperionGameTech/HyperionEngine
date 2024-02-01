#include <dotnet_support/DotNetSystem.hpp>
#include <dotnet_support/Assembly.hpp>

namespace hyperion {
namespace dotnet {

Assembly::Assembly(RC<detail::DotNetImplBase> impl)
    : m_impl(std::move(impl))
{
}

} // namespace dotnet
} // namespace hyperion