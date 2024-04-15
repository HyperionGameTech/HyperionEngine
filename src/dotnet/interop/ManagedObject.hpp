/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_DOTNET_INTEROP_MANAGED_OBJECT_HPP
#define HYPERION_V2_DOTNET_INTEROP_MANAGED_OBJECT_HPP

#include <dotnet/interop/ManagedGuid.hpp>

#include <Engine.hpp>
#include <Types.hpp>

namespace hyperion::dotnet {

extern "C" {

struct ManagedObject
{
    ManagedGuid guid;
    void        *ptr;
};

static_assert(sizeof(ManagedObject) == 24, "ManagedObject size mismatch with C#");

} // extern "C"

} // namespace hyperion::dotnet

#endif
