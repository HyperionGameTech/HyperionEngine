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

namespace Baking {

extern Pool* g_bakerPool;
extern Arena* g_bakerArena;

using BakerAllocator = AllocatorInstance<Pool, &g_bakerPool>;
using BakerTempAllocator = AllocatorInstance<Arena, &g_bakerArena>;

} // namespace Baking
} // namespace Hyperion
