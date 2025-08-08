/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/FlatMap.hpp>
#include <core/containers/ArrayMap.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/object/ObjId.hpp>

#include <core/math/Transform.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderStats.hpp>
#include <rendering/IndirectDraw.hpp>

#include <rendering/RenderQueue.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderObject.hpp>

#include <rendering/RenderProxyable.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Scene;
class Camera;
class Entity;
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

struct ParallelRenderingState
{
    static constexpr uint32 maxBatches = g_numAsyncRenderingCommandBuffers;

    TaskBatch* taskBatch = nullptr;

    uint32 numBatches = 0;

    // Non-async rendering command list - used for binding state at the start of the pass before async stuff
    RenderQueue rootQueue;

    // per-thread RenderQueue
    FixedArray<RenderQueue, maxBatches> localQueues {};

    FixedArray<RenderStatsCounts, maxBatches> renderStatsCounts {};

    // Temporary storage for data that will be executed in parallel during the frame
    Array<Span<const DrawCall>, FixedAllocator<maxBatches>> drawCalls;
    Array<Span<const InstancedDrawCall>, FixedAllocator<maxBatches>> instancedDrawCalls;
    Array<Proc<void(Span<const DrawCall>, uint32, uint32)>, FixedAllocator<1>> drawCallProcs;
    Array<Proc<void(Span<const InstancedDrawCall>, uint32, uint32)>, FixedAllocator<1>> instancedDrawCallProcs;

    ParallelRenderingState* next = nullptr;
};

// Utility struct that maps attribute sets -> draw call collections that have been written to already and had render groups created.
struct DrawCallCollectionMapping
{
    Handle<RenderGroup> renderGroup;
    // map entity id to mesh proxy
    SparsePagedArray<RenderProxyMesh*, 128> meshProxies;
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
    template <class T>
    static constexpr SizeType GetTrackedResourceTypeIndex()
    {
        return FindTypeElementIndex<T, TrackedResourceTypes>::value;
    }

    template <class Functor, SizeType... Indices>
    static inline void ForEachResourceTracker_Impl(Span<ResourceTrackerBase*> resourceTrackers, const Functor& functor, std::index_sequence<Indices...>)
    {
        (functor(static_cast<typename TupleElement_Tuple<Indices, ResourceTrackerTypes>::Type&>(*resourceTrackers[Indices])), ...);
    }

    template <class Functor>
    static inline void ForEachResourceTracker(Span<ResourceTrackerBase*> resourceTrackers, const Functor& functor)
    {
        ForEachResourceTracker_Impl(resourceTrackers, functor, std::make_index_sequence<TupleSize<ResourceTrackerTypes>::value>());
    }

public:
    /*! \param isShared if true, uses a spinlock to protect against mutual access of the data */
    RenderProxyList(bool isShared);

    RenderProxyList(const RenderProxyList& other) = delete;
    RenderProxyList& operator=(const RenderProxyList& other) = delete;

    RenderProxyList(RenderProxyList&& other) noexcept = delete;
    RenderProxyList& operator=(RenderProxyList&& other) noexcept = delete;

    ~RenderProxyList();

    void BeginWrite()
    {
        if (isShared)
        {
            uint64 rwMarkerState = rwMarker.BitOr(writeFlag, MemoryOrder::ACQUIRE);
            while (HYP_UNLIKELY(rwMarkerState & readMask))
            {
                HYP_LOG_TEMP("Busy waiting for read marker to be released! "
                             "If this is occuring frequently, the View that owns this RenderProxyList should have double / triple buffering enabled");

                rwMarkerState = rwMarker.Get(MemoryOrder::ACQUIRE);
                HYP_WAIT_IDLE();
            }
        }

        AssertDebug(state != CS_READING);
        state = CS_WRITING;

        // advance all trackers to the next state before we write into them.
        // this clears their 'next' bits and sets their 'previous' bits so we can tell what changed.
        ForEachResourceTracker(resourceTrackers.ToSpan(), [](auto&& resourceTracker)
            {
                resourceTracker.Advance();
            });
    }

    void EndWrite()
    {
        AssertDebug(state == CS_WRITING);

        state = CS_WRITTEN;

        if (isShared)
        {
            rwMarker.BitAnd(~writeFlag, MemoryOrder::RELEASE);
        }
    }

    void BeginRead()
    {
        if (isShared)
        {
            uint64 rwMarkerState;

            do
            {
                rwMarkerState = rwMarker.Increment(2, MemoryOrder::ACQUIRE);

                if (HYP_UNLIKELY(rwMarkerState & writeFlag))
                {
                    HYP_LOG_TEMP("Waiting for write marker to be released. "
                                 "If this is occurring frequently, the View that owns this RenderProxyList should have double / triple buffering enabled");

                    rwMarker.Decrement(2, MemoryOrder::RELAXED);

                    // spin to wait for write flag to be released
                    HYP_WAIT_IDLE();
                }
            }
            while (HYP_UNLIKELY(rwMarkerState & writeFlag));
        }
        else
        {
            ++readDepth;
        }

        AssertDebug(state != CS_WRITING);
        state = CS_READING;
    }

    void EndRead()
    {
        AssertDebug(state == CS_READING);

        if (isShared)
        {
            uint64 rwMarkerState = rwMarker.Decrement(2, MemoryOrder::ACQUIRE_RELEASE);
            AssertDebug(rwMarkerState & readMask, "Invalid state, expected read mask to be set when calling EndRead()");

            /// FIXME: If BeginRead() is called on other thread between the check and setting state to CS_DONE,
            /// we could set state to done when it isn't actually.
            if (((rwMarkerState - 2) & readMask) == 0)
            {
                state = CS_DONE;
            }
        }
        else
        {
            if (!--readDepth)
            {
                state = CS_DONE;
            }
        }
    }

    template <SizeType Index>
    HYP_FORCE_INLINE auto GetResources() -> typename TupleElement_Tuple<Index, ResourceTrackerTypes>::Type*
    {
        return static_cast<typename TupleElement_Tuple<Index, ResourceTrackerTypes>::Type*>(resourceTrackers[Index]);
    }

    // template hackery to allow usage of undefined types
    template <class T>
    static inline void UpdateRefs(T& renderProxyList)
    {
        const auto impl = []<class ElementType, class ProxyType>(ResourceTracker<ObjId<ElementType>, ElementType*, ProxyType>& resourceTracker)
        {
            auto diff = resourceTracker.GetDiff();
            if (!diff.NeedsUpdate())
            {
                return;
            }

            Array<ElementType*> removed;
            resourceTracker.GetRemoved(removed, false);

            Array<ElementType*> added;
            resourceTracker.GetAdded(added, false);

            for (ElementType* resource : added)
            {
                resource->GetObjectHeader_Internal()->IncRefStrong();

                if constexpr (!std::is_same_v<ProxyType, NullProxy>)
                {
                    ProxyType* pProxy = resourceTracker.GetProxy(ObjId<ElementType>(resource->Id()));

                    if (!pProxy)
                    {
                        pProxy = resourceTracker.SetProxy(ObjId<ElementType>(resource->Id()), ProxyType());
                    }

                    AssertDebug(pProxy != nullptr);

                    if constexpr (HYP_HAS_METHOD(ElementType, UpdateRenderProxy))
                    {
                        resource->UpdateRenderProxy(pProxy);
                    }
                }
            }

            for (ElementType* resource : removed)
            {
                resource->GetObjectHeader_Internal()->DecRefStrong();

                if constexpr (!std::is_same_v<ProxyType, NullProxy>)
                {
                    resourceTracker.RemoveProxy(ObjId<ElementType>(resource->Id()));
                }
            }

            if constexpr (!std::is_same_v<ProxyType, NullProxy> && HYP_HAS_METHOD(ElementType, UpdateRenderProxy))
            {
                Array<ObjId<ElementType>> changedIds;
                resourceTracker.GetChanged(changedIds);
                
                for (const ObjId<ElementType>& id : changedIds)
                {
                    ElementType** ppResource = resourceTracker.GetElement(id);
                    AssertDebug(ppResource && *ppResource);
                    
                    ElementType& resource = **ppResource;
                    
                    ProxyType* pProxy = resourceTracker.GetProxy(id);
                    AssertDebug(pProxy != nullptr);
                    
                    resource.UpdateRenderProxy(pProxy);
                }
            }
        };

        ForEachResourceTracker(renderProxyList.resourceTrackers.ToSpan(), impl);
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
    bool useOrdering : 1 = false;                  //!< are mesh entities sorted using an indirect array to map sort order?
    bool disableBuildRenderCollection : 1 = false; //!< Disable building out RenderCollection. Set to true in the case of custom render collection building (See UIRenderer)

    Viewport viewport;
    int priority;

    FixedArray<ResourceTrackerBase*, TupleSize<TrackedResourceTypes>::value> resourceTrackers;

#define DEF_RESOURCE_TRACKER_GETTER(getterName, T)                                                                                                                \
    HYP_FORCE_INLINE auto Get##getterName()->typename TupleElement_Tuple<FindTypeElementIndex<class T, TrackedResourceTypes>::value, ResourceTrackerTypes>::Type& \
    {                                                                                                                                                             \
        return *GetResources<FindTypeElementIndex<class T, TrackedResourceTypes>::value>();                                                                       \
    }

    DEF_RESOURCE_TRACKER_GETTER(MeshEntities, Entity);
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

class RenderCollector
{
public:
    RenderCollector();
    RenderCollector(const RenderCollector& other) = delete;
    RenderCollector& operator=(const RenderCollector& other) = delete;
    RenderCollector(RenderCollector&& other) noexcept = delete;
    RenderCollector& operator=(RenderCollector&& other) noexcept = delete;
    ~RenderCollector();

#ifdef HYP_DEBUG_MODE
    HYP_FORCE_INLINE SizeType NumDrawCallsCollected() const
    {
        SizeType numDrawCalls = 0;

        for (const auto& mappings : mappingsByBucket)
        {
            for (const KeyValuePair<RenderableAttributeSet, DrawCallCollectionMapping>& it : mappings)
            {
                const DrawCallCollectionMapping& mapping = it.second;

                numDrawCalls += mapping.drawCallCollection.drawCalls.Size()
                    + mapping.drawCallCollection.instancedDrawCalls.Size();
            }
        }

        return numDrawCalls;
    }
#endif

    void Clear(bool freeMemory = true);

    ParallelRenderingState* parallelRenderingStateHead;
    ParallelRenderingState* parallelRenderingStateTail;

    // map entity id to previous attribute set (for draw call collection)
    SparsePagedArray<RenderableAttributeSet, 128> previousAttributes;

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
