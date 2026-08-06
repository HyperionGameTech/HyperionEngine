/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

namespace Hyperion {
namespace memory {

template <class AllocatorType, AllocatorType** GlobalInstance>
struct AllocatorInstance;

class Pool;
struct DynamicAllocator;

template <class AllocatorType>
class TArena;

using Arena = TArena<DynamicAllocator>;

} // namespace memory

using memory::Pool;
using memory::Arena;
using memory::AllocatorInstance;

} // namespace Hyperion