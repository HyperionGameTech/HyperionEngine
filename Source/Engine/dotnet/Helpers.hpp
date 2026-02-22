#pragma once
#include <Core/Defines.hpp>

#include <Core/containers/String.hpp>

#include <Core/reflection/ObjectFwd.hpp>
#include <Core/reflection/BoxedValue.hpp>

#include <Core/memory/RefCountedPtr.hpp>

#include <Core/reflection/Handle.hpp>
#include <Core/reflection/ObjId.hpp>

#include <type_traits>

namespace Hyperion {
namespace filesystem {

class FilePath;

} // namespace filesystem

using filesystem::FilePath;

} // namespace Hyperion

namespace Hyperion::dotnet {

class ManagedObject;

// Conditionally construct or reference existing BoxedValue
template <class T>
static inline const BoxedValue* SetArgBoxed(BoxedValue* arr, SizeType index, T&& arg)
{
    if constexpr (IsBoxedValueV<T>)
    {
        return &arg;
    }
    else
    {
        new (&arr[index]) BoxedValue(std::forward<T>(arg));
        return &arr[index];
    }
}

// NOLINTBEGIN
// ^^^ clang-lint wants to treat this as a global variable?
// Expand over each argument to fill argsArray and argsArrayPtr
template <class... Args, SizeType... Indices>
static inline void SetArgsBoxed(std::index_sequence<Indices...>, BoxedValue* arr, const BoxedValue* (&arrayPtr)[sizeof...(Args) + 1], Args&&... args)
{
    ((arrayPtr[Indices] = SetArgBoxed(arr, Indices, std::forward<Args>(args))), ...);
    arrayPtr[sizeof...(Args)] = nullptr;
}

// NOLINTEND

} // namespace Hyperion::dotnet
