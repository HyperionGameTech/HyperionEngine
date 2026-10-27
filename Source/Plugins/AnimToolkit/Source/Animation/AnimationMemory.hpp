/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Memory/Allocator/AllocatorFwd.hpp>

namespace Hyperion {

ANIMATION_API extern Pool* g_animPool;

using AnimAllocator = AllocatorInstance<Pool, &g_animPool>;

} // namespace Hyperion
