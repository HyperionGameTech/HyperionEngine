/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <dotnet/interop/ManagedGuid.hpp>

#include <type_traits>

namespace Hyperion {

struct BoxedValue;

namespace dotnet {

struct ObjectReference;

using Delegate = std::add_pointer_t<void()>;

using InvokeMethodFunction = void (*)(ObjectReference*, const BoxedValue**, BoxedValue*);

using InvokeGetterFunction = void (*)(ManagedGuid, ObjectReference*, const BoxedValue**, BoxedValue*);
using InvokeSetterFunction = void (*)(ManagedGuid, ObjectReference*, const BoxedValue**, BoxedValue*);

} // namespace dotnet
} // namespace Hyperion
