/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <DotNET/Interop/ManagedGuid.hpp>

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
