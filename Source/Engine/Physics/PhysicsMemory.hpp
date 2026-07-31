/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

namespace Hyperion {

#pragma region Fwd declarations

namespace memory {

class Pool;

template <class AllocatorType> class TArena;

using Arena = TArena<DynamicAllocator>;

} // namespace memory

using memory::Pool;
using memory::Arena;

#pragma endregion Fwd declarations

extern Pool* g_physicsPool;
extern Arena* g_physicsArena;

using PhysicsAllocator = AllocatorInstance<Pool, &g_physicsPool>;
using PhysicsTempAllocator = AllocatorInstance<Arena, &g_physicsArena>;

} // namespace Hyperion
