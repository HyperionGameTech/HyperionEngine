/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_INTEROP_MANAGED_ATTRIBUTE_HPP
#define HYPERION_DOTNET_INTEROP_MANAGED_ATTRIBUTE_HPP

#include <core/memory/UniquePtr.hpp>

#include <dotnet/interop/ManagedGuid.hpp>
#include <dotnet/interop/ManagedObject.hpp>

#include <Types.hpp>

namespace hyperion::dotnet {

class Class;

extern "C" {

struct ManagedAttribute
{
    Class           *class_ptr;
    ObjectReference object_reference;
};

static_assert(sizeof(ManagedAttribute) == 24, "sizeof(ManagedAttribute) must match C# struct size");

struct ManagedAttributeHolder
{
    uint32              managed_attributes_size;
    ManagedAttribute    *managed_attributes_ptr;
};

static_assert(sizeof(ManagedAttributeHolder) == 16, "sizeof(ManagedAttributeHolder) must match C# struct size");

} // extern "C"

} // namespace hyperion::dotnet

#endif