/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Memory/Allocator/AllocatorFwd.hpp>

namespace Hyperion {

namespace memory
{
template <class AllocatorType, size_t BufferAlignment>
class ByteBuffer;

} // namespace memory

namespace net {

NET_API extern Pool* g_netPool;

using NetAllocator = AllocatorInstance<Pool, &g_netPool>;

using NetBuffer = memory::ByteBuffer<NetAllocator, 1>;

} // namespace net
} // namespace Hyperion
