/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>
#include <core/Defines.hpp>
#include <core/Constants.hpp>

namespace hyperion {

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

HYP_API extern Pool* const* g_enginePools[EPN_MAX];

HYP_API extern Pool* g_objectPool;                  // Pool for object allocations - not thread safe per se, but used with proper locking in HypObjectPool
HYP_API extern Pool* g_renderPool;                  // Pool for rendering allocations (render thread only)
HYP_API extern Pool* g_framePools[RingBufferDepth]; // Pools for per-frame allocations, on either game or render thread for their given frame index.
HYP_API extern Pool* g_scenePool;                   // Pool for scene-related allocations (thread safe)
HYP_API extern Pool* g_taskPool;                    // Pool for task system allocations (thread safe)
HYP_API extern Pool* g_resourcePool;                // Pool for IResource-derived allocations (thread safe)
HYP_API extern Pool* g_streamingPool;               // Pool for streaming system allocations (thread safe)

using SceneAllocator = AllocatorInstance<Pool, &g_scenePool>;
using RenderAllocator = AllocatorInstance<Pool, &g_renderPool>;
using StreamingAllocator = AllocatorInstance<Pool, &g_streamingPool>;

HYP_API extern TArena<RenderAllocator>* g_renderArena; // Arena for scene-related temporary allocations during the frame (render thread only)
HYP_API extern TArena<SceneAllocator>* g_sceneArena;   // Arena for scene-related temporary allocations during the frame (game thread only)
HYP_API extern TArena<StreamingAllocator>* g_streamingArena;

using SceneTempAllocator = AllocatorInstance<TArena<SceneAllocator>, &g_sceneArena>;
using StreamingTempAllocator = AllocatorInstance<TArena<StreamingAllocator>, &g_streamingArena>;

} // namespace hyperion
