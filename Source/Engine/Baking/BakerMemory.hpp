/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once


#include <Core/Memory/Allocator/AllocatorFwd.hpp>

namespace Hyperion {
namespace Baking {

extern Pool* g_bakerPool;
extern Arena* g_bakerArena;

using BakerAllocator = AllocatorInstance<Pool, &g_bakerPool>;
using BakerTempAllocator = AllocatorInstance<Arena, &g_bakerArena>;

} // namespace Baking
} // namespace Hyperion
