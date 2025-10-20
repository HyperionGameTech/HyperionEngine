/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/FlatMap.hpp>
#include <core/containers/ArrayMap.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/memory/pool/LinearPool.hpp>

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

HYP_MAKE_HAS_METHOD(UpdateRenderProxy);

struct DrawCallRange
{
    SizeType start;
    SizeType count;
};

struct ParallelRenderingState_Shared;

struct ParallelRenderingState
{
    static constexpr uint32 MaxBatches = NumAsyncCommandBuffers;

    using LocalQueue = TRenderQueue<TAllocator<TLinearArena<DynamicAllocator>>>;

    TaskBatch* taskBatch = nullptr;

    uint32 numBatches = 0;

    ParallelRenderingState_Shared* sharedData = nullptr;

    // Non-async rendering command list - used for binding state at the start of the pass before async stuff (can only be written to from render thread)
    RenderQueue rootQueue;

    // per-thread RenderQueue
    FixedArray<LocalQueue*, MaxBatches> localQueues {};

    FixedArray<RenderStatsCounts, MaxBatches> renderStatsCounts {};

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
class RenderProxyList
{
    static constexpr uint64 writeFlag = 0x1;
    static constexpr uint64 readMask = uint64(-1) & ~writeFlag;

public:
    using TrackedResourceTypes = Tuple<
        Entity, // mesh entities
        Mesh,
        Camera,
        EnvProbe,
        Light,
        EnvGrid,
        LightmapVolume,
        Material,
        Skeleton,
        Texture>;

    using ResourceTrackerTypes = Tuple<
        ResourceTracker<ObjId<Entity>, Entity*, RenderProxyMesh>,
        ResourceTracker<ObjId<Mesh>, Mesh*>,
        ResourceTracker<ObjId<Camera>, Camera*, RenderProxyCamera>,
        ResourceTracker<ObjId<EnvProbe>, EnvProbe*, RenderProxyEnvProbe>,
        ResourceTracker<ObjId<Light>, Light*, RenderProxyLight>,
        ResourceTracker<ObjId<EnvGrid>, EnvGrid*, RenderProxyEnvGrid>,
        ResourceTracker<ObjId<LightmapVolume>, LightmapVolume*, RenderProxyLightmapVolume>,
        ResourceTracker<ObjId<Material>, Material*, RenderProxyMaterial>,
        ResourceTracker<ObjId<Skeleton>, Skeleton*, RenderProxySkeleton>,
        ResourceTracker<ObjId<Texture>, Texture*>>;

    static_assert(TupleSize<ResourceTrackerTypes>::value == TupleSize<TrackedResourceTypes>::value, "Tuple sizes must match");

private:
public:
    /*! \param isShared if true, uses a spinlock to protect against mutual access of the data
     *  \param useRefCounting if true, will increment reference count (UpdateRefs() will need to be called) and release reference counts on destruction. */
    RenderProxyList(bool isShared, bool useRefCounting);

    RenderProxyList(const RenderProxyList& other) = delete;
    RenderProxyList& operator=(const RenderProxyList& other) = delete;

    RenderProxyList(RenderProxyList&& other) noexcept = delete;
    RenderProxyList& operator=(RenderProxyList&& other) noexcept = delete;

    ~RenderProxyList();

    HYP_API void BeginWrite();
    HYP_API void EndWrite();

    HYP_API void BeginRead();
    HYP_API void EndRead();

    template <SizeType Index>
    HYP_FORCE_INLINE auto GetResources() -> typename TupleElement_Tuple<Index, ResourceTrackerTypes>::Type*
    {
        return static_cast<typename TupleElement_Tuple<Index, ResourceTrackerTypes>::Type*>(resourceTrackers[Index]);
    }

    template <SizeType Index>
    HYP_FORCE_INLINE auto GetResources() const -> const typename TupleElement_Tuple<Index, ResourceTrackerTypes>::Type*
    {
        return static_cast<const typename TupleElement_Tuple<Index, ResourceTrackerTypes>::Type*>(resourceTrackers[Index]);
    }

    // State for tracking transitions from writing (game thread) to reading (render thread).
    enum CollectionState : uint8
    {
        CS_WRITING, //!< Currently being written to. set when the frame starts on the game thread.
        CS_WRITTEN, //!< Written to, but not yet read from. set when the frame finishes on the game thread.
        CS_READING, //!< Currently ready to be read. set when the frame starts on the render thread.
        CS_DONE     //!< Finished reading. set when the frame finishes on the render thread.
    };

    CollectionState state : 2 = CS_DONE;

    const bool isShared : 1 = false;               //!< should we use a spinlock to ensure multiple threads aren't accessing this list at the same time?
    const bool useRefCounting : 1 = true;          //!< Should we inc/dec ref counts for resources we hold?
    bool useOrdering : 1 = false;                  //!< are mesh entities sorted using an indirect array to map sort order?
    bool disableBuildRenderCollection : 1 = false; //!< Disable building out RenderCollection. Set to true in the case of custom render collection building (See UIRenderer)

#ifdef HYP_DEBUG_MODE
    bool debugIsDestroyed : 1 = false; //!< Set to true in the destructor. Used to catch use-after-free bugs.
    bool debugIsSynced : 1 = false;
#endif

    Viewport viewport;
    int priority;

    FixedArray<ResourceTrackerBase*, TupleSize<TrackedResourceTypes>::value> resourceTrackers;
    FixedArray<void (*)(ResourceTrackerBase*), TupleSize<TrackedResourceTypes>::value> releaseRefsFunctions;

#define DEF_RESOURCE_TRACKER_GETTER(getterName, T)                                                                                                                            \
    HYP_FORCE_INLINE auto Get##getterName()->typename TupleElement_Tuple<FindTypeElementIndex<class T, TrackedResourceTypes>::value, ResourceTrackerTypes>::Type&             \
    {                                                                                                                                                                         \
        return *GetResources<FindTypeElementIndex<class T, TrackedResourceTypes>::value>();                                                                                   \
    }                                                                                                                                                                         \
                                                                                                                                                                              \
    HYP_FORCE_INLINE auto Get##getterName() const->const typename TupleElement_Tuple<FindTypeElementIndex<class T, TrackedResourceTypes>::value, ResourceTrackerTypes>::Type& \
    {                                                                                                                                                                         \
        return *GetResources<FindTypeElementIndex<class T, TrackedResourceTypes>::value>();                                                                                   \
    }

    DEF_RESOURCE_TRACKER_GETTER(MeshEntities, Entity);
    DEF_RESOURCE_TRACKER_GETTER(Meshes, Mesh);
    DEF_RESOURCE_TRACKER_GETTER(Cameras, Camera);
    DEF_RESOURCE_TRACKER_GETTER(EnvProbes, EnvProbe);
    DEF_RESOURCE_TRACKER_GETTER(Lights, Light);
    DEF_RESOURCE_TRACKER_GETTER(EnvGrids, EnvGrid);
    DEF_RESOURCE_TRACKER_GETTER(LightmapVolumes, LightmapVolume);
    DEF_RESOURCE_TRACKER_GETTER(Materials, Material);
    DEF_RESOURCE_TRACKER_GETTER(Skeletons, Skeleton);
    DEF_RESOURCE_TRACKER_GETTER(Textures, Texture);

#undef DEF_RESOURCE_TRACKER_GETTER

    Array<Pair<ObjId<Entity>, int>, DynamicAllocator> meshEntityOrdering;

    // marker to set to locked when game thread is writing to this list.
    // this only really comes into play with non-buffered Views that do not double/triple buffer their RenderProxyLists
    AtomicVar<uint64> rwMarker { 0 };
    uint32 readDepth = 0;
};

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

    IDrawCallCollectionImpl* drawCallCollectionImpl;
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
