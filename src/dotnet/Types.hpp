/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYP_DOTNET_TYPES_HPP
#define HYP_DOTNET_TYPES_HPP

#include <dotnet/interop/ManagedGuid.hpp>

#include <type_traits>

namespace hyperion {

struct HypData;

namespace dotnet {

struct ObjectReference;

using Delegate = std::add_pointer_t<void()>;

using InvokeMethodFunction = void(*)(ObjectReference *, const HypData **, HypData *);

using InvokeGetterFunction = void(*)(ManagedGuid, ObjectReference *, const HypData **, HypData *);
using InvokeSetterFunction = void(*)(ManagedGuid, ObjectReference *, const HypData **, HypData *);

} // namespace dotnet
} // namespace hyperion

#endif