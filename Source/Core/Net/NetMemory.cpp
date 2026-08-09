/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Net/NetMemory.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>
#include <Core/Memory/Allocator/ArenaAllocator.hpp>

#include <Core/Memory/Pool/Pool.hpp>

namespace Hyperion {
namespace net {

static constexpr size_t NetPoolBlockSize = 32 * 1024 * 1024; // 32 MiB

static Pool s_netPool { NetPoolBlockSize, PF_FALLBACK | PF_THREAD_SAFE };
Pool* g_netPool = &s_netPool;

} // namespace net
} // namespace Hyperion
