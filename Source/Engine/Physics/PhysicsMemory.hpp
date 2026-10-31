/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Memory/Allocator/AllocatorFwd.hpp>

namespace Hyperion {

ENGINE_API extern Pool* g_physicsPool;
ENGINE_API extern Arena* g_physicsArena;

using PhysicsAllocator = AllocatorInstance<Pool, &g_physicsPool>;
using PhysicsTempAllocator = AllocatorInstance<Arena, &g_physicsArena>;

} // namespace Hyperion
