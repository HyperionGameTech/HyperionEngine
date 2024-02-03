#ifndef HYPERION_V2_DOTNET_INTEROP_MANAGED_METHOD_HPP
#define HYPERION_V2_DOTNET_INTEROP_MANAGED_METHOD_HPP

#include <dotnet/interop/ManagedGuid.hpp>

#include <Engine.hpp>
#include <Types.hpp>

namespace hyperion::dotnet {

extern "C" {

struct ManagedMethod
{
    ManagedGuid guid;
};

}

} // namespace hyperion::dotnet

#endif