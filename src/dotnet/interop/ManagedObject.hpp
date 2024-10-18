/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_INTEROP_MANAGED_OBJECT_HPP
#define HYPERION_DOTNET_INTEROP_MANAGED_OBJECT_HPP

#include <dotnet/interop/ManagedGuid.hpp>

#include <Types.hpp>

namespace hyperion::dotnet {

extern "C" {

struct ObjectReference
{
    ManagedGuid guid;
    void        *ptr;

    bool operator==(const ObjectReference &other) const = default;
    bool operator!=(const ObjectReference &other) const = default;
};

static_assert(sizeof(ObjectReference) == 24, "ObjectReference size mismatch with C#");

} // extern "C"

} // namespace hyperion::dotnet

#endif
