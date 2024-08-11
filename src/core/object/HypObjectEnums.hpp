
#ifndef HYPERION_CORE_HYP_OBJECT_ENUMS_HPP
#define HYPERION_CORE_HYP_OBJECT_ENUMS_HPP

#include <Types.hpp>

namespace hyperion {

enum class HypClassAllocationMethod : uint8
{
    OBJECT_POOL_HANDLE,
    REF_COUNTED_PTR,

    DEFAULT = OBJECT_POOL_HANDLE
};

} // namespace hyperion

#endif