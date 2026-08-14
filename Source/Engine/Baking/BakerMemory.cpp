/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Baking/BakerMemory.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>
#include <Core/Memory/Allocator/ArenaAllocator.hpp>

#include <Core/Memory/Pool/Pool.hpp>

namespace Hyperion {
namespace Baking {

static constexpr size_t BakerPoolBlockSize = 64 * 1024 * 1024; // 64 MB
static constexpr size_t BakerArenaBlockSize = 1 * 1024 * 1024; // 1 MB

// Use system memory allocator for fallback (we will allocate large mesh data chunks..)
static Pool s_bakerPool { BakerPoolBlockSize, PF_FALLBACK | PF_THREAD_SAFE };
Pool* g_bakerPool = &s_bakerPool;

static Arena s_bakerArena { BakerArenaBlockSize };
Arena* g_bakerArena = &s_bakerArena;

} // namespace Baking
} // namespace Hyperion
