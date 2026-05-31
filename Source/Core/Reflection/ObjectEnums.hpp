#pragma once
#include <Core/Types.hpp>

namespace Hyperion {

enum class ClassAllocationMethod : uint8
{
    INVALID = uint8(-1),

    NONE = 0,
    HANDLE = 1
};

} // namespace Hyperion
