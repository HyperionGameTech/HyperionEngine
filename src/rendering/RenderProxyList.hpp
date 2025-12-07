/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/FlatMap.hpp>
#include <core/containers/ArrayMap.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/utilities/Pair.hpp>
#include <core/utilities/Tuple.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/memory/allocator/ArenaAllocator.hpp>

#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/TaskSystem.hpp>

#include <core/reflection/ObjId.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/Shared.hpp>
#include <rendering/util/ResourceTracker.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Scene;
class Camera;
class Entity;
class Mesh;
class EnvProbe;
class Light;
class EnvGrid;
class LightmapVolume;
class ParticleVolume;
class FogVolume;
class Material;
class Texture;
class Skeleton;

HYP_MAKE_HAS_METHOD(UpdateRenderProxy);

/*! \brief A collection of rendering-related objects for a View, populated via View::Collect() and usable for rendering a frame.
 *  Keeps track of which objects are newly added, removed or changed (via render proxy version changing), allowing updates to be applied to only objects that need it. */
class RenderProxyList
{
    static constexpr uint64 WriteFlag = 0x1;
    static constexpr uint64 ReadMask = uint64(-1) & ~WriteFlag;

public:
    using AllocatorType = Pool; // per-frame pools

    using TrackedResourceTypes = Tuple<
        Entity, // mesh entities
        Mesh,
        Camera,
        EnvProbe,
        Light,
        EnvGrid,
        LightmapVolume,
        ParticleVolume,
        FogVolume,
        Material,
        Skeleton,
        Texture>;

    using ResourceTrackerTypes = Tuple<
        ResourceTracker<AllocatorType, ObjId<Entity>, Entity*, RenderProxyMesh>,
        ResourceTracker<AllocatorType, ObjId<Mesh>, Mesh*>,
        ResourceTracker<AllocatorType, ObjId<Camera>, Camera*, RenderProxyCamera>,
        ResourceTracker<AllocatorType, ObjId<EnvProbe>, EnvProbe*, RenderProxyEnvProbe>,
        ResourceTracker<AllocatorType, ObjId<Light>, Light*, RenderProxyLight>,
        ResourceTracker<AllocatorType, ObjId<EnvGrid>, EnvGrid*, RenderProxyEnvGrid>,
        ResourceTracker<AllocatorType, ObjId<LightmapVolume>, LightmapVolume*, RenderProxyLightmapVolume>,
        ResourceTracker<AllocatorType, ObjId<ParticleVolume>, ParticleVolume*, RenderProxyParticleVolume>,
        ResourceTracker<AllocatorType, ObjId<FogVolume>, FogVolume*, RenderProxyFogVolume>,
        ResourceTracker<AllocatorType, ObjId<Material>, Material*, RenderProxyMaterial>,
        ResourceTracker<AllocatorType, ObjId<Skeleton>, Skeleton*, RenderProxySkeleton>,
        ResourceTracker<AllocatorType, ObjId<Texture>, Texture*>>;

    static_assert(TupleSize<ResourceTrackerTypes>::value == TupleSize<TrackedResourceTypes>::value, "Tuple sizes must match");

private:
public:
    /*! \param pAllocator The allocator to use for this render proxy list
     *  \param isShared if true, uses a spinlock to protect against mutual access of the data
     *  \param useRefCounting if true, will increment reference count (UpdateRefs() will need to be called) and release reference counts on destruction. */
    RenderProxyList(AllocatorType* pAllocator, bool isShared, bool useRefCounting);

    RenderProxyList(const RenderProxyList& other) = delete;
    RenderProxyList& operator=(const RenderProxyList& other) = delete;

    RenderProxyList(RenderProxyList&& other) noexcept = delete;
    RenderProxyList& operator=(RenderProxyList&& other) noexcept = delete;

    ~RenderProxyList();

    HYP_API void BeginWrite();
    HYP_API void EndWrite();

    /*! \brief Waits for write lock to unlock and sets the list in read mode. If pOutSuccess is provided, it will be set to true on lock acquired.
        However if the lock cannot be acquired after a number of loops, it will be set to false and will not perform any other action. If pOutSuccess
        is not provided, the busy wait will spin indefinitely. (for backwards compatibility with previous behaviour) */
    HYP_API void BeginRead(bool* pOutSuccess = nullptr);
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

    FixedArray<ResourceTrackerBase<AllocatorType>*, TupleSize<TrackedResourceTypes>::value> resourceTrackers;
    FixedArray<void (*)(ResourceTrackerBase<AllocatorType>*), TupleSize<TrackedResourceTypes>::value> releaseRefsFunctions;

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
    DEF_RESOURCE_TRACKER_GETTER(ParticleVolumes, ParticleVolume);
    DEF_RESOURCE_TRACKER_GETTER(FogVolumes, FogVolume);
    DEF_RESOURCE_TRACKER_GETTER(Materials, Material);
    DEF_RESOURCE_TRACKER_GETTER(Skeletons, Skeleton);
    DEF_RESOURCE_TRACKER_GETTER(Textures, Texture);

#undef DEF_RESOURCE_TRACKER_GETTER

    Array<Pair<ObjId<Entity>, int>, DynamicAllocator> meshEntityOrdering;

    // marker to set to locked when game thread is writing to this list.
    // this only really comes into play with non-buffered Views that do not double/triple buffer their RenderProxyLists
    volatile int64 rwMarker = 0;
    uint32 readDepth = 0;
};

} // namespace hyperion
