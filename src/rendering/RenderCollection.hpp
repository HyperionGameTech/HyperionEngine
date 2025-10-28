/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/FlatMap.hpp>
#include <core/containers/ArrayMap.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/memory/allocator/ArenaAllocator.hpp>

#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/reflection/ObjId.hpp>

#include <core/math/Transform.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderStats.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>
#include <rendering/RenderProxyList.hpp>

#include <engine/EngineStats.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Scene;
class Camera;
class Entity;
class Mesh;
class RenderGroup;
struct RenderSetup;
class IndirectRenderer;
class LightmapVolume;
class ReflectionProbe;
class Texture;
class Skeleton;
class RenderCollector;
struct ResourceContainer;
enum class RenderGroupFlags : uint32;
enum LightType : uint32;
enum EnvProbeType : uint32;

// UpdateRenderProxy trait declared in RenderProxyList.hpp

struct DrawCallRange
{
    SizeType start;
    SizeType count;
};

struct ParallelRenderingState_Shared;

struct ParallelRenderingState
{
    static constexpr uint32 MaxBatches = NumAsyncCommandBuffers;

    using LocalQueue = TRenderQueue<TArena<RenderAllocator>>;

    TaskBatch* taskBatch = nullptr;

    uint32 numBatches = 0;

    ParallelRenderingState_Shared* sharedData = nullptr;

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

    explicit ParallelRenderingState(ParallelRenderingState_Shared* sharedData);

    ParallelRenderingState(const ParallelRenderingState&) = delete;
    ParallelRenderingState& operator=(const ParallelRenderingState&) = delete;

    ParallelRenderingState(ParallelRenderingState&&) noexcept = delete;
    ParallelRenderingState& operator=(ParallelRenderingState&&) noexcept = delete;

    ~ParallelRenderingState();
};

// Utility struct that maps attribute sets -> draw call collections that have been written to already and had render groups created.
struct DrawCallCollectionMapping
{
    Handle<RenderGroup> renderGroup;
    // map entity id to mesh proxy
    SparsePagedArray<RenderProxyMesh*, 128, RenderAllocator> meshProxies;
    DrawCallCollection drawCallCollection;
    IndirectRenderer* indirectRenderer = nullptr;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return renderGroup.IsValid();
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return renderGroup.IsValid();
    }
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

#ifdef HYP_DEBUG_MODE
    SizeType NumDrawCallsCollected() const;
#endif

    void Clear(bool freeMemory = true);

    ParallelRenderingState* parallelRenderingStateHead;
    ParallelRenderingState* parallelRenderingStateTail;

    // map entity id to previous attribute set (for draw call collection)
    SparsePagedArray<RenderableAttributeSet, 128, RenderAllocator> previousAttributes;

    FixedArray<FlatMap<RenderableAttributeSet, DrawCallCollectionMapping>, RB_MAX> mappingsByBucket;

    EntityBatchAllocatorBase* batchAllocator;
    EnumFlags<RenderGroupFlags> renderGroupFlags;

    ParallelRenderingState* AcquireNextParallelRenderingState();
    void CommitParallelRenderingState(RenderQueue& renderQueue);

    void PerformOcclusionCulling(FrameBase* frame, const RenderSetup& renderSetup, uint32 bucketBits);

    // Writes commands into the frame's command list to execute the draw calls in the given bucket mask.
    void ExecuteDrawCalls(FrameBase* frame, const RenderSetup& renderSetup, uint32 bucketBits);

    // Writes commands into the frame's command list to execute the draw calls in the given bucket mask.
    void ExecuteDrawCalls(FrameBase* frame, const RenderSetup& renderSetup, const FramebufferRef& framebuffer, uint32 bucketBits);

    void RemoveEmptyRenderGroups();

    /*! \brief Counts the number of render groups in the list. */
    uint32 NumRenderGroups() const;

    /*! \brief Builds RenderGroups for proxies, based on renderable attributes */
    void BuildRenderGroups(View* view, RenderProxyList& renderProxyList);

    void BuildDrawCalls(uint32 bucketBits);
};

} // namespace hyperion
