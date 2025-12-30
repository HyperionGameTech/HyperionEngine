/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

namespace Hyperion {
namespace memory {

class Pool;

template <class AllocatorType>
class TArena;

template <class AllocatorType, AllocatorType** GlobalInstance>
struct AllocatorInstance;

} // namespace memory

using memory::AllocatorInstance;
using memory::Pool;
using memory::TArena;

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
