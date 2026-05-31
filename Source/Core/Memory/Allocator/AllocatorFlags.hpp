/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>
#include <Core/Utilities/EnumFlags.hpp>

namespace Hyperion {

enum AllocatorFlags : uint32
{
    AF_NONE = 0x0,
    AF_THREAD_SAFE = 0x1, //!< allocator is thread-safe
};

HYP_MAKE_ENUM_FLAGS(AllocatorFlags);

} // namespace Hyperion
