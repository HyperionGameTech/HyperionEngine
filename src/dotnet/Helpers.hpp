#pragma once
#include <core/Defines.hpp>

#include <core/containers/String.hpp>

#include <core/reflection/ObjectFwd.hpp>
#include <core/reflection/BoxedValue.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/reflection/Handle.hpp>
#include <core/reflection/ObjId.hpp>

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
static inline const BoxedValue* SetArg_HypData(BoxedValue* arr, SizeType index, T&& arg)
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
static inline void SetArgs_HypData(std::index_sequence<Indices...>, BoxedValue* arr, const BoxedValue* (&arrayPtr)[sizeof...(Args) + 1], Args&&... args)
{
    ((arrayPtr[Indices] = SetArg_HypData(arr, Indices, std::forward<Args>(args))), ...);
    arrayPtr[sizeof...(Args)] = nullptr;
}

// NOLINTEND

} // namespace Hyperion::dotnet
