/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Core/Math/BoundingBox.hpp>

#include <Core/Utilities/Tuple.hpp>

#include <Core/Threading/SharedMutex.hpp>

#include <Core/Resource/Resource.hpp>

#include <Core/Reflection/ObjId.hpp>

#include <Rendering/RenderableAttributes.hpp>
#include <Rendering/Shared.hpp>

#include <Framework/Resources/ResourceTracker.hpp>

namespace Hyperion {

class Scene;
class Camera;
class Entity;
class Mesh;
class EnvProbe;
class Light;
class ProbeVolume;
class LightmapVolume;
class ParticleVolume;
class FogVolume;
class MaterialInstance;
class Texture;
class Skeleton;
class Sprite;

struct RenderProxyCamera;
struct RenderProxyMesh;
struct RenderProxyEnvProbe;
struct RenderProxyLight;
struct RenderProxyProbeVolume;
struct RenderProxyLightmapVolume;
struct RenderProxyParticleVolume;
struct RenderProxyFogVolume;
struct RenderProxyMaterial;
struct RenderProxySkeleton;
struct RenderProxySprite;

HYP_MAKE_HAS_METHOD(UpdateRenderProxy);

/*! \brief A collection of rendering-related objects for a View, populated via View::Collect() and usable for rendering a frame.
 *  Keeps track of which objects are newly added, removed or changed (via render proxy version changing), allowing updates to be applied to only objects that need it. */
class RenderProxyList
{
    static constexpr uint64 WriteFlag = 0x1;
    static constexpr uint64 ReadMask = uint64(-1) & ~WriteFlag;

public:
    using AllocatorType = DynamicAllocator;

    template <class... Ts>
    using ResourceTrackerBase = Resources::ResourceTrackerBase<Ts...>;

    template <class... Ts>
    using ResourceTracker = Resources::ResourceTracker<Ts...>;

    using TrackedResourceTypes = Tuple<
        Entity, // mesh entities
        Mesh,
        Camera,
        EnvProbe,
        Light,
        ProbeVolume,
        LightmapVolume,
        ParticleVolume,
        FogVolume,
        MaterialInstance,
        Skeleton,
        Texture,
        Sprite>;

    using ResourceTrackerTypes = Tuple<
        ResourceTracker<AllocatorType, ObjId<Entity>, Entity*, RenderProxyMesh>,
        ResourceTracker<AllocatorType, ObjId<Mesh>, Mesh*>,
        ResourceTracker<AllocatorType, ObjId<Camera>, Camera*, RenderProxyCamera>,
        ResourceTracker<AllocatorType, ObjId<EnvProbe>, EnvProbe*, RenderProxyEnvProbe>,
        ResourceTracker<AllocatorType, ObjId<Light>, Light*, RenderProxyLight>,
        ResourceTracker<AllocatorType, ObjId<ProbeVolume>, ProbeVolume*, RenderProxyProbeVolume>,
        ResourceTracker<AllocatorType, ObjId<LightmapVolume>, LightmapVolume*, RenderProxyLightmapVolume>,
        ResourceTracker<AllocatorType, ObjId<ParticleVolume>, ParticleVolume*, RenderProxyParticleVolume>,
        ResourceTracker<AllocatorType, ObjId<FogVolume>, FogVolume*, RenderProxyFogVolume>,
        ResourceTracker<AllocatorType, ObjId<MaterialInstance>, MaterialInstance*, RenderProxyMaterial>,
        ResourceTracker<AllocatorType, ObjId<Skeleton>, Skeleton*, RenderProxySkeleton>,
        ResourceTracker<AllocatorType, ObjId<Texture>, Texture*>,
        ResourceTracker<AllocatorType, ObjId<Sprite>, Sprite*, RenderProxySprite>>;

    static_assert(TupleSize<ResourceTrackerTypes>::value == TupleSize<TrackedResourceTypes>::value, "Tuple sizes must match");

public:
    /*! \param isShared if true, uses a spinlock to protect against mutual access of the data
     *  \param useRefCounting if true, will increment reference count (UpdateRefs() will need to be called) and release reference counts on destruction. */
    RenderProxyList(bool isShared, bool useRefCounting);

    RenderProxyList(const RenderProxyList& other) = delete;
    RenderProxyList& operator=(const RenderProxyList& other) = delete;

    RenderProxyList(RenderProxyList&& other) noexcept = delete;
    RenderProxyList& operator=(RenderProxyList&& other) noexcept = delete;

    virtual ~RenderProxyList();

    ENGINE_API void BeginWrite();
    ENGINE_API void EndWrite();

    /*! \brief Waits for write lock to unlock and sets the list in read mode. If pOutSuccess is provided, it will be set to true on lock acquired.
        However if the lock cannot be acquired after a number of loops, it will be set to false and will not perform any other action. If pOutSuccess
        is not provided, the busy wait will spin indefinitely. (for backwards compatibility with previous behaviour) */
    ENGINE_API void BeginRead(bool* pOutSuccess = nullptr);
    ENGINE_API void EndRead();

    // Must be in write mode to call
    ENGINE_API void ClearAll();

    template <size_t Index>
    HYP_FORCE_INLINE auto GetResources() -> typename TupleElement_Tuple<Index, ResourceTrackerTypes>::Type*
    {
        return static_cast<typename TupleElement_Tuple<Index, ResourceTrackerTypes>::Type*>(resourceTrackers[Index]);
    }

    template <size_t Index>
    HYP_FORCE_INLINE auto GetResources() const -> const typename TupleElement_Tuple<Index, ResourceTrackerTypes>::Type*
    {
        return static_cast<const typename TupleElement_Tuple<Index, ResourceTrackerTypes>::Type*>(resourceTrackers[Index]);
    }

    // State for tracking transitions from writing (sim thread) to reading (render thread).
    enum CollectionState : uint8
    {
        CS_WRITING, //!< Currently being written to. set when the frame starts on the sim thread.
        CS_WRITTEN, //!< Written to, but not yet read from. set when the frame finishes on the sim thread.
        CS_READING, //!< Currently ready to be read. set when the frame starts on the render thread.
        CS_DONE     //!< Finished reading. set when the frame finishes on the render thread.
    };

    CollectionState state : 2 = CS_DONE;

    const bool isShared : 1 = false;               //!< should we use a spinlock to ensure multiple threads aren't accessing this list at the same time?
    const bool useRefCounting : 1 = true;          //!< Should we inc/dec ref counts for resources we hold?
    bool useOrdering : 1 = false;                  //!< are mesh entities sorted using an indirect array to map sort order?
    bool disableBuildRenderCollection : 1 = false; //!< Disable building out RenderCollection. Set to true in the case of custom render collection building (See UIPass)

    uint32 writeGeneration = 0;

    int priority;

    FixedArray<ResourceTrackerBase<AllocatorType>*, TupleSize<TrackedResourceTypes>::value> resourceTrackers;

#define DEF_RESOURCE_TRACKER_GETTER(getterName, T)                                                                                                                              \
    HYP_FORCE_INLINE auto Get##getterName() -> typename TupleElement_Tuple<FindTypeElementIndex<class T, TrackedResourceTypes>::value, ResourceTrackerTypes>::Type&             \
    {                                                                                                                                                                           \
        return *GetResources<FindTypeElementIndex<class T, TrackedResourceTypes>::value>();                                                                                     \
    }                                                                                                                                                                           \
                                                                                                                                                                                \
    HYP_FORCE_INLINE auto Get##getterName() const -> const typename TupleElement_Tuple<FindTypeElementIndex<class T, TrackedResourceTypes>::value, ResourceTrackerTypes>::Type& \
    {                                                                                                                                                                           \
        return *GetResources<FindTypeElementIndex<class T, TrackedResourceTypes>::value>();                                                                                     \
    }

    DEF_RESOURCE_TRACKER_GETTER(MeshEntities, Entity);
    DEF_RESOURCE_TRACKER_GETTER(Meshes, Mesh);
    DEF_RESOURCE_TRACKER_GETTER(Cameras, Camera);
    DEF_RESOURCE_TRACKER_GETTER(EnvProbes, EnvProbe);
    DEF_RESOURCE_TRACKER_GETTER(Lights, Light);
    DEF_RESOURCE_TRACKER_GETTER(ProbeVolumes, ProbeVolume);
    DEF_RESOURCE_TRACKER_GETTER(LightmapVolumes, LightmapVolume);
    DEF_RESOURCE_TRACKER_GETTER(ParticleVolumes, ParticleVolume);
    DEF_RESOURCE_TRACKER_GETTER(FogVolumes, FogVolume);
    DEF_RESOURCE_TRACKER_GETTER(Materials, MaterialInstance);
    DEF_RESOURCE_TRACKER_GETTER(Skeletons, Skeleton);
    DEF_RESOURCE_TRACKER_GETTER(Textures, Texture);
    DEF_RESOURCE_TRACKER_GETTER(Sprites, Sprite);

#undef DEF_RESOURCE_TRACKER_GETTER

    Array<Pair<ObjId<Entity>, int>, DynamicAllocator> meshEntityOrdering;

    CameraMatrices cachedMatrices;
    BoundingBox cachedBounds;

    SharedMutex m_lock;
};

} // namespace Hyperion
