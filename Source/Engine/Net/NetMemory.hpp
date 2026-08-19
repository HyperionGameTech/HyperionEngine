/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Memory/Allocator/AllocatorFwd.hpp>

namespace Hyperion {
namespace net {

NET_API extern Pool* g_netPool;

using NetAllocator = AllocatorInstance<Pool, &g_netPool>;

} // namespace net
} // namespace Hyperion
