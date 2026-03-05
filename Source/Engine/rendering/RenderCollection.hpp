/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/FlatMap.hpp>

#include <Core/memory/resource/Resource.hpp>

#include <Core/memory/allocator/ThreadAllocator.hpp>

#include <Core/threading/DataRaceDetector.hpp>
#include <Core/threading/Task.hpp>
#include <Core/threading/TaskSystem.hpp>

#include <Core/reflection/ObjId.hpp>

#include <Core/Types.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Shared.hpp>

#include <engine/EngineStats.hpp>

namespace Hyperion {

class Scene;
class Camera;
class Entity;
class View;
class Mesh;
struct RenderSetup;
class IndirectRenderer;
class LightmapVolume;
class ReflectionProbe;
class Texture;
class Skeleton;
class RenderCollector;
class RenderProxyList;
struct ResourceContainer;
struct RenderProxy;
enum class LightType : uint32;
enum EnvProbeType : uint32;

// UpdateRenderProxy trait declared in RenderProxyList.hpp

struct DrawCallRange
{
    size_t start;
    size_t count;
};

struct ParallelRenderingState_Shared;

struct ParallelRenderingState
{
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    static constexpr uint32 MaxBatches = NumAsyncCommandBuffers;

    using LocalQueue = TRenderQueue<ThreadAllocator>;

    TaskBatch* taskBatch = nullptr;

    uint32 numBatches = 0;

    ParallelRenderingState_Shared* sharedData = nullptr;
    bool ownsSharedData = false;

    // Non-async rendering command list - used for binding state at the start of the pass before async stuff (can only be written to from render thread)
    RenderQueue rootQueue;

    // per-thread RenderQueue
    FixedArray<LocalQueue*, MaxBatches> localQueues {};

    FixedArray<EngineStatsValueSet, MaxBatches> statValues {};

    // Temporary storage for data that will be executed in parallel during the frame
    Array<DrawCallRange, FixedAllocator<MaxBatches>> drawCalls;
    Array<DrawCallRange, FixedAllocator<MaxBatches>> instancedDrawCalls;
    Array<Proc<void(DrawCallRange, uint32, uint32)>, FixedAllocator<1>> drawCallProcs;
    Array<Proc<void(DrawCallRange, uint32, uint32)>, FixedAllocator<1>> instancedDrawCallProcs;

    ParallelRenderingState* next = nullptr;

    ParallelRenderingState(ParallelRenderingState_Shared* sharedData, bool ownsSharedData);

    ParallelRenderingState(const ParallelRenderingState&) = delete;
    ParallelRenderingState& operator=(const ParallelRenderingState&) = delete;

    ParallelRenderingState(ParallelRenderingState&&) noexcept = delete;
    ParallelRenderingState& operator=(ParallelRenderingState&&) noexcept = delete;

    ~ParallelRenderingState();
};

/*! \brief A collection of rendering-related objects for a View, populated via View::Collect() and usable for rendering a frame.
 *  Keeps track of which objects are newly added, removed or changed (via render proxy version changing), allowing updates to be applied to only objects that need it. */
// RenderProxyList is declared in rendering/RenderProxyList.hpp

class HYP_API RenderCollector
{
public:
    RenderCollector();
    RenderCollector(const RenderCollector& other) = delete;
    RenderCollector& operator=(const RenderCollector& other) = delete;
    RenderCollector(RenderCollector&& other) noexcept = delete;
    RenderCollector& operator=(RenderCollector&& other) noexcept = delete;
    ~RenderCollector();

#if HYP_DEBUG_MODE
    size_t NumDrawCallsCollected() const;
#endif

    void Clear(bool freeMemory = true);

    ParallelRenderingState* parallelRenderingStateHead;
    ParallelRenderingState* parallelRenderingStateTail;

    // map entity id to previous attribute set (for draw call collection)
    SparsePagedArray<RenderableAttributeSet, 128, RenderAllocator> previousAttributes;

    FixedArray<HashMap<RenderableAttributeSet, DrawCallCollection, NodeAllocator<RenderAllocator>>, NumRenderBuckets> mappingsByBucket;

    EntityBatchAllocatorBase* batchAllocator;
    EnumFlags<RenderGroupFlags> renderGroupFlags;

    ParallelRenderingState* AcquireNextParallelRenderingState();
    void CommitParallelRenderingState(RenderQueue& renderQueue);

    void PerformOcclusionCulling(Frame* frame, const RenderSetup& renderSetup, uint32 bucketBits);

    // Writes commands into the frame's command list to execute the draw calls in the given bucket mask.
    void ExecuteDrawCalls(
        Frame* frame,
        const RenderSetup& renderSetup,
        uint32 bucketBits,
        bool commit = true);

    // Writes commands into the frame's command list to execute the draw calls in the given bucket mask.
    void ExecuteDrawCalls(
        Frame* frame,
        const RenderSetup& renderSetup,
        Framebuffer* framebuffer,
        uint32 bucketBits,
        bool commit = true);

    void BuildDrawCalls(uint32 bucketBits);

    void RemoveEmptyRenderGroups();

    /*! \brief Counts the number of render groups in the list. */
    uint32 NumRenderGroups() const;

    /*! \brief Builds RenderGroups for proxies, based on renderable attributes */
    void BuildRenderGroups(View* view, RenderProxyList& renderProxyList);
};

} // namespace Hyperion
