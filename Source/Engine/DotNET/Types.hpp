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

///Returns false if the managed method threw an exception
using InvokeMethodFunction = bool (*)(ObjectReference*, const BoxedValue**, BoxedValue*);

///Returns false if the property accessor threw an exception
using InvokeGetterFunction = bool (*)(ManagedGuid, ObjectReference*, const BoxedValue**, BoxedValue*);
using InvokeSetterFunction = bool (*)(ManagedGuid, ObjectReference*, const BoxedValue**, BoxedValue*);

} // namespace dotnet
} // namespace Hyperion
