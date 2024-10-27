/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RUNTIME_DOTNET_MANAGED_HANDLE_HPP
#define HYPERION_RUNTIME_DOTNET_MANAGED_HANDLE_HPP

#include <core/utilities/TypeID.hpp>
#include <core/Handle.hpp>
#include <core/ID.hpp>

#include <type_traits>

namespace hyperion {

extern "C" struct ManagedHandle
{
    uint32  id;

    // Called from C# to increment the reference count
    // when an object is constructed with the handle
    void IncRef(uint32 type_id);

    // Called from C# to release the handle
    // and decrement the reference count
    void DecRef(uint32 type_id);

    uint32 GetRefCountStrong(uint32 type_id) const;
    uint32 GetRefCountWeak(uint32 type_id) const;
};

static_assert(sizeof(ManagedHandle) == 4, "ManagedHandle must be 4 bytes to match C# struct size");
static_assert(std::is_trivial_v<ManagedHandle>, "ManagedHandle must be a trivial type to be used in C#");

} // namespace hyperion

#endif