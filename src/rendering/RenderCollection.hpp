/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/FlatMap.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/memory/allocator/ArenaAllocator.hpp>

#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/reflection/ObjId.hpp>

#include <core/Types.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <engine/EngineStats.hpp>

namespace Hyperion {

class Scene;
class Camera;
class Entity;
class View;
class Mesh;
class RenderGroup;
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
    RenderGroup* renderGroup = nullptr;
    // map entity id to mesh proxy
    IndirectRenderer* indirectRenderer = nullptr;
    SparsePagedArray<RenderProxyMesh*, 128, RenderAllocator> meshProxies;
    DrawCallCollection drawCallCollection;

    DrawCallCollectionMapping() = default;

    DrawCallCollectionMapping(const DrawCallCollectionMapping& other) = delete;
    DrawCallCollectionMapping& operator=(const DrawCallCollectionMapping& other) = delete;

    DrawCallCollectionMapping(DrawCallCollectionMapping&& other) noexcept
        : renderGroup(other.renderGroup),
          indirectRenderer(other.indirectRenderer),
          meshProxies(std::move(other.meshProxies)),
          drawCallCollection(std::move(other.drawCallCollection))
    {
        other.renderGroup = nullptr;
        other.indirectRenderer = nullptr;
    }

    DrawCallCollectionMapping& operator=(DrawCallCollectionMapping&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (renderGroup != nullptr && renderGroup != other.renderGroup)
        {
            delete renderGroup;
        }

        renderGroup = other.renderGroup;
        indirectRenderer = other.indirectRenderer;
        meshProxies = std::move(other.meshProxies);
        drawCallCollection = std::move(other.drawCallCollection);

        other.renderGroup = nullptr;
        other.indirectRenderer = nullptr;

        return *this;
    }

    ~DrawCallCollectionMapping()
    {
        delete renderGroup;
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return renderGroup != nullptr;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return renderGroup != nullptr;
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

    FixedArray<HashMap<RenderableAttributeSet, DrawCallCollectionMapping, NodeAllocator<RenderAllocator>>, RB_MAX> mappingsByBucket;

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
        const FramebufferRef& framebuffer,
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
