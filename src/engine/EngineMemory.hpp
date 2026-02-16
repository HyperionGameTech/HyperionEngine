/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

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

HYP_ENUM()
enum EnginePoolName : int
{
    EPN_INVALID = -1,

    EPN_CORE = 0, // global shared pool
    EPN_RENDER,
    EPN_SCENE,

    EPN_MAX
};

#include <engine/EngineMemory.inc>

} // namespace Hyperion
