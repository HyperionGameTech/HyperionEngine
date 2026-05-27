/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/math/Ray.hpp>

#include <Core/utilities/EnumFlags.hpp>

#include <Core/functional/Delegate.hpp>

#include <Core/memory/resource/Resource.hpp>

#include <Core/math/Frustum.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderTypes.hpp>

namespace Hyperion {

class Scene;
class Camera;
class Light;
class LightmapVolume;
class EnvGrid;
class EnvProbe;
class Texture;
class GBuffer;
class EntityBatchAllocatorBase;
class RenderProxyList;

HYP_API extern Pool* g_scenePool;
using SceneAllocator = AllocatorInstance<Pool, &g_scenePool>;

namespace threading {

class TaskBatch;

} // namespace threading

using threading::TaskBatch;

HYP_ENUM()
enum class ViewFlags : uint32
{
    NONE = 0x0,

    GBUFFER = 0x1,

    ALL_WORLD_SCENES = 0x2,             //!< If set, all scenes added to the world will be added view, and removed when removed from the world. Otherwise, the View itself manages the scenes it contains.

    COLLECT_STATIC_ENTITIES = 0x4,      //!< If set, the view will collect static entities (those that are not dynamic). Dynamic entities are those that move or are animated.
    COLLECT_DYNAMIC_ENTITIES = 0x8,     //!< If set, the view will collect dynamic entities (those that are not static). Static entities are those that do not move and are not animated.
    COLLECT_ALL_ENTITIES = COLLECT_STATIC_ENTITIES | COLLECT_DYNAMIC_ENTITIES,

    NO_FRUSTUM_CULLING = 0x10,          //!< If set, the view will not perform frustum culling. This is useful for debugging or when you want to render everything regardless of visibility.

    SKIP_ENV_PROBES = 0x20,             //!< If set, the view will not collect EnvProbes
    SKIP_ENV_GRIDS = 0x40,              //!< If set, the view will not collect EnvGrids.
    SKIP_LIGHTS = 0x80,                 //!< If set, the view will not collect Lights.
    SKIP_LIGHTMAP_VOLUMES = 0x100,      //!< If set, the view will not collect LightmapVolumes.
    SKIP_PARTICLE_VOLUMES = 0x200,      //!< If set, the view will not collect ParticleVolumes.
    SKIP_FOG_VOLUMES = 0x400,           //!< If set, the view will not collect FogVolumes.
    SKIP_CAMERAS = 0x800,               //!< If set, the view will not collect Cameras.
    SKIP_SPRITES = 0x1000,              //!< If set, the view will not collect Sprites.

    NOT_MULTI_BUFFERED = 0x2000,        //!< Disables double / triple buffering for the RenderProxyList this View writes to.
                                        //  --- Use ONLY for Views that are not written to every frame, and instead are written to and read once (or infrequently); e.g EnvProbes.
                                        //  --- Use of these is still threadsafe, however it uses a spinlock instead of multiple buffering so contentions will eat up cpu cycles.
    NO_DRAW_CALLS = 0x4000,             //!< If set, no draw calls will be built for any mesh entities that this View collects.

    NO_PARALLEL_DRAW_CALL_COLLECTION = 0x8000, //!< Set flag in order to forcibly disable parallel draw call collection for this View. Used for systems like EnvProbe rendering, rather than geometry pass rendering.

    // enable flags
    RAY_TRACING = 0x100000,             //!< Does this View contain rayTracing data (acceleration structures)? (RayTracing must be enabled in the global config and must have RT hardware support)

    MATCH_CAMERA_DIMENSIONS = 0x200000, //!< If set, the Viewport dimensions will always match the associated Camera's dimensions.

    SHADOW_VIEW = 0x400000,             //!< This View is for a rendering a shadow map slice or cascade
    BAKER_VIEW = 0x800000,              //!< This View is for baking lightmaps or shadow maps, not for rendering to the screen (see: Baker.cpp)
    UI_VIEW = 0x1000000,                //!< This View is for rendering UI elements. See UISubsystem.
    CUBEMAP_FACE_VIEW = 0x2000000,           //!< This View corresponds to a face in a cubemap - will not automatically update sub-frustum

    EXTERNAL_RENDERTARGET = 0x10000000,

    DEFAULT = ALL_WORLD_SCENES | COLLECT_ALL_ENTITIES
};

HYP_MAKE_ENUM_FLAGS(ViewFlags);

struct ViewDesc
{
    EnumFlags<ViewFlags> flags = ViewFlags::DEFAULT;

    FramebufferDesc framebufferDesc;
    uint8 viewIndex = 0; // for cubemap drawing

    Array<Scene*> scenes;

    Camera* camera = nullptr;

    BoundingBox bounds = BoundingBox::Empty();

    int priority = 0;
    float resolutionScale = 1.0f;

    Optional<RenderableAttributeSet> overrideAttributes;

    const Class* entityBatchClass = nullptr;
};

class ViewOutputTarget
{
public:
    ViewOutputTarget();
    ViewOutputTarget(const FramebufferRef& framebuffer);
    ViewOutputTarget(const Handle<GBuffer>& gbuffer);
    ViewOutputTarget(const ViewOutputTarget& other) = delete;
    ViewOutputTarget& operator=(const ViewOutputTarget& other) = delete;
    ViewOutputTarget(ViewOutputTarget&& other) noexcept = default;
    ViewOutputTarget& operator=(ViewOutputTarget&& other) noexcept = default;
    ~ViewOutputTarget();

    HYP_FORCE_INLINE bool IsValid() const
    {
        return bool(m_impl);
    }

    const Handle<GBuffer>& GetGBuffer() const;
    const FramebufferRef& GetFramebuffer() const;
    const FramebufferRef& GetFramebuffer(RenderBucket rb) const;
    Span<const FramebufferRef> GetFramebuffers() const;

private:
    Handle<ObjectBase> m_impl;
};

HYP_CLASS()
class HYP_API View final : public ObjectBase
{
    HYP_OBJECT_BODY(View);

    friend class World;

public:
    View();

    explicit View(const ViewDesc& viewDesc, Name name = Name::Invalid());

    View(const View& other) = delete;
    View& operator=(const View& other) = delete;

    View(View&& other) noexcept = delete;
    View& operator=(View&& other) noexcept = delete;

    ~View();

    HYP_FORCE_INLINE const ViewDesc& GetViewDesc() const
    {
        return desc;
    }

    HYP_FORCE_INLINE Name GetName() const
    {
        return name;
    }

    HYP_FORCE_INLINE void SetName(Name newName)
    {
        name = newName;
    }

    HYP_FORCE_INLINE EnumFlags<ViewFlags> GetFlags() const
    {
        return flags;
    }

    HYP_METHOD()
    const Array<Scene*>& GetScenes() const
    {
        return m_scenes;
    }

    HYP_METHOD()
    void AddScene(Scene* scene);

    HYP_METHOD()
    void RemoveScene(Scene* scene);

    HYP_METHOD()
    const Handle<Camera>& GetCamera() const
    {
        return m_camera;
    }

    HYP_FORCE_INLINE const ViewOutputTarget& GetOutputTarget() const
    {
        return m_outputTarget;
    }

    HYP_METHOD()
    int GetPriority() const
    {
        return priority;
    }

    HYP_METHOD()
    void SetPriority(int pri);

    HYP_FORCE_INLINE const Optional<RenderableAttributeSet>& GetOverrideAttributes() const
    {
        return m_overrideAttributes;
    }

    HYP_FORCE_INLINE RenderProxyList* GetRenderProxyList(uint32 index) const
    {
        return m_renderProxyLists[index];
    }

    HYP_FORCE_INLINE const WeakHandle<View>& GetRayTracingView() const
    {
        return m_rayTracingView;
    }

    HYP_FORCE_INLINE const ProcRef<void(RenderProxyList&)>& GetOverrideCollectFunctor() const
    {
        return m_overrideCollectFunctor;
    }

    HYP_FORCE_INLINE void SetOverrideCollectFunctor(const ProcRef<void(RenderProxyList&)>& ref)
    {
        m_overrideCollectFunctor = ref;
    }

    bool TestRay(const Ray& ray, RayTestResults& outResults, EnumFlags<RayTestFlags> flags = RayTestFlags::TestBVH) const;

    /*! \brief Sync changes to the Viewport so the render thread can see updates to it for the next frame */
    void UpdateViewport();

    /*! \brief Computes visibility states for all Scenes this View has using the Camera */
    void UpdateVisibility();

    void PrepareShadowViews(Array<View*, SceneAllocator>& outShadowViews);

    /*! \brief Enqueue tasks to `batch` to asynchronously collect entities and other scene resources for the current View. */
    void BeginAsyncCollection(TaskBatch& batch);
    /*! \brief End asynchronous scene collection tasks */
    void EndAsyncCollection();

    /*! \brief Synchronously collect scene resources for the View, blocks the current thread until complete. */
    void CollectSync();

    ViewDesc desc;

    Name name;

    EnumFlags<ViewFlags> flags;

    Mat4f cachedViewProjMatrix;
    Frustum cachedFrustum;

    int priority;

protected:
    void Init() override;

    void CollectCameras(RenderProxyList& rpl);
    void CollectLights(RenderProxyList& rpl);
    void CollectLightmapVolumes(RenderProxyList& rpl);
    void CollectParticleVolumes(RenderProxyList& rpl);
    void CollectFogVolumes(RenderProxyList& rpl);
    void CollectEnvGrids(RenderProxyList& rpl);
    void CollectEnvProbes(RenderProxyList& rpl);
    void CollectSprites(RenderProxyList& rpl);
    void CollectMeshEntities(RenderProxyList& rpl);

    Array<Scene*> m_scenes;
    Handle<Camera> m_camera;
    ViewOutputTarget m_outputTarget;

    // optional rayTracing View set by the world
    WeakHandle<View> m_rayTracingView;

    RenderProxyList* m_renderProxyLists[RingBufferDepth];

    Optional<RenderableAttributeSet> m_overrideAttributes;

    ProcRef<void(RenderProxyList&)> m_overrideCollectFunctor;

    TaskBatch* m_collectionTaskBatch;
};

} // namespace Hyperion
