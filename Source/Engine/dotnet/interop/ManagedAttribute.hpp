/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/memory/UniquePtr.hpp>

#include <dotnet/interop/ManagedGuid.hpp>
#include <dotnet/ManagedObject.hpp>

#include <Core/Types.hpp>

namespace Hyperion::dotnet {

class ManagedClass;

extern "C"
{
    struct ManagedAttribute
    {
        ManagedClass* classPtr;
        ObjectReference objectReference;
    };

    static_assert(sizeof(ManagedAttribute) == 24, "sizeof(ManagedAttribute) must match C# struct size");

    struct ManagedAttributeHolder
    {
        uint32 managedAttributesSize;
        ManagedAttribute* managedAttributesPtr;
    };

    static_assert(sizeof(ManagedAttributeHolder) == 16, "sizeof(ManagedAttributeHolder) must match C# struct size");
} // extern "C"

} // namespace Hyperion::dotnet
