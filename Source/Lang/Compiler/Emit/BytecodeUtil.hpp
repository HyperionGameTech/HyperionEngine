#pragma once

#include <Core/Memory/UniquePtr.hpp>

#include <type_traits>

namespace Hyperion {

// fwd declarations
struct BytecodeChunk;
struct BuildParams;
struct Buildable;

class BytecodeUtil
{
public:
    template <typename T, typename... Args>
    static UniquePtr<T> Make(Args&&... args)
    {
        static_assert(std::is_base_of<Buildable, T>::value, "Must be a Buildable type.");
        return MakeUnique<T>(std::forward<Args>(args)...);
    }
};

} // namespace Hyperion
