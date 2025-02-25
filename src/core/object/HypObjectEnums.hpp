
#ifndef HYPERION_CORE_HYP_OBJECT_ENUMS_HPP
#define HYPERION_CORE_HYP_OBJECT_ENUMS_HPP

#include <Types.hpp>

namespace hyperion {

enum class HypClassAllocationMethod : uint8
{
    NONE = 0,
    
    HANDLE,
    REF_COUNTED_PTR
};

} // namespace hyperion

#endif