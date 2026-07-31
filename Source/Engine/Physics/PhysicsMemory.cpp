/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Physics/PhysicsMemory.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>
#include <Core/Memory/Allocator/ArenaAllocator.hpp>

#include <Core/Memory/Pool/Pool.hpp>

namespace Hyperion {

static constexpr size_t PhysicsPoolBlockSize = 4 * 1024 * 1024; // 4 MiB
static constexpr size_t PhysicsArenaBlockSize = 16 * 1024; // 16 KiB

static Pool s_physicsPool { PhysicsPoolBlockSize, PF_DEFAULT };
Pool* g_physicsPool = &s_physicsPool;

static Arena s_physicsArena { PhysicsArenaBlockSize };
Arena* g_physicsArena = &s_physicsArena;

} // namespace Hyperion
