/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

namespace Hyperion {
namespace memory {

struct DynamicAllocator;

template <class AllocatorType, AllocatorType** GlobalInstance>
struct AllocatorInstance;

class Pool;

template <class AllocatorType>
class TArena;

using Arena = TArena<DynamicAllocator>;

} // namespace memory

using memory::AllocatorInstance;
using memory::Pool;
using memory::TArena;
using memory::Arena;

namespace threading {
class ThreadId;
} // namespace threading

using threading::ThreadId;

#include <Framework/EngineMemory.inc>

} // namespace Hyperion
