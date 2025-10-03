#pragma once
#include <core/Defines.hpp>

#include <core/containers/String.hpp>

#include <core/object/HypObjectFwd.hpp>
#include <core/object/HypData.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/object/Handle.hpp>
#include <core/object/ObjId.hpp>

#include <type_traits>

namespace hyperion {
namespace filesystem {

class FilePath;

} // namespace filesystem

using filesystem::FilePath;

} // namespace hyperion

namespace hyperion::dotnet {

class Object;

// Conditionally construct or reference existing HypData
template <class T>
static inline const HypData* SetArg_HypData(HypData* arr, SizeType index, T&& arg)
{
    if constexpr (IsHypDataV<T>)
    {
        return &arg;
    }
    else
    {
        new (&arr[index]) HypData(std::forward<T>(arg));
        return &arr[index];
    }
}

// NOLINTBEGIN
// ^^^ clang-lint wants to treat this as a global variable?
// Expand over each argument to fill argsArray and argsArrayPtr
template <class... Args, SizeType... Indices>
static inline void SetArgs_HypData(std::index_sequence<Indices...>, HypData* arr, const HypData* (&arrayPtr)[sizeof...(Args) + 1], Args&&... args)
{
    ((arrayPtr[Indices] = SetArg_HypData(arr, Indices, std::forward<Args>(args))), ...);
    arrayPtr[sizeof...(Args)] = nullptr;
}

// NOLINTEND

} // namespace hyperion::dotnet
