#pragma once
#include <core/Types.hpp>

namespace hyperion {

enum class HypClassAllocationMethod : uint8
{
    INVALID = uint8(-1),

    NONE = 0,
    HANDLE = 1
};

} // namespace hyperion
