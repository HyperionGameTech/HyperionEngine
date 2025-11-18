/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Constants.hpp>
#include <core/Types.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/math/Ray.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/functional/Delegate.hpp>

#include <core/memory/resource/Resource.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderObject.hpp>

namespace hyperion {

class Scene;
class Camera;
class Light;
class LightmapVolume;
class EnvGrid;
class EnvProbe;
class Texture;
class ParticleVolume;
class GBuffer;
class EntityBatchAllocatorBase;
class RenderProxyList;

namespace threading {

class TaskBatch;

} // namespace threading

using threading::TaskBatch;

HYP_ENUM()
enum class ViewFlags : uint32
{
    NONE = 0x0,
    GBUFFER = 0x1,

    ALL_WORLD_SCENES = 0x2, //!< If set, all scenes added to the world will be added view, and removed when removed from the world. Otherwise, the View itself manages the scenes it contains.

    COLLECT_STATIC_ENTITIES = 0x4,  //!< If set, the view will collect static entities (those that are not dynamic). Dynamic entities are those that move or are animated.
    COLLECT_DYNAMIC_ENTITIES = 0x8, //!< If set, the view will collect dynamic entities (those that are not static). Static entities are those that do not move and are not animated.
    COLLECT_ALL_ENTITIES = COLLECT_STATIC_ENTITIES | COLLECT_DYNAMIC_ENTITIES,

    NO_FRUSTUM_CULLING = 0x10, //!< If set, the view will not perform frustum culling. This is useful for debugging or when you want to render everything regardless of visibility.

    SKIP_ENV_PROBES = 0x20,        //!< If set, the view will not collect EnvProbes
    SKIP_ENV_GRIDS = 0x40,         //!< If set, the view will not collect EnvGrids.
    SKIP_LIGHTS = 0x80,            //!< If set, the view will not collect Lights.
    SKIP_LIGHTMAP_VOLUMES = 0x100, //!< If set, the view will not collect LightmapVolumes.
    SKIP_PARTICLE_VOLUMES = 0x200, //!< If set, the view will not collect ParticleVolumes.
    SKIP_CAMERAS = 0x400,          //!< If set, the view will not collect Cameras.

    NOT_MULTI_BUFFERED = 0x1000, //!< Disables double / triple buffering for the RenderProxyList this View writes to.
                                 //  --- Use ONLY for Views that are not written to every frame, and instead are written to and read once (or infrequently); e.g EnvProbes.
                                 //  --- Use of these is still threadsafe, however it uses a spinlock instead of multiple buffering so contentions will eat up cpu cycles.
    NO_DRAW_CALLS = 0x2000,      //!< If set, no draw calls will be built for any mesh entities that this View collects.

    ENABLE_READBACK = 0x4000, //!< Enable render target texture readback (final texture)

    // enable flags
    RAYTRACING = 0x100000, //!< Does this View contain raytracing data (acceleration structures)? (Raytracing must be enabled in the global config and must have RT hardware support)

    MATCH_CAMERA_DIMENSIONS = 0x200000, //!< If set, the Viewport dimensions will always match the associated Camera's dimensions.

    DEFAULT = ALL_WORLD_SCENES | COLLECT_ALL_ENTITIES
};

HYP_MAKE_ENUM_FLAGS(ViewFlags)

struct ViewOutputTargetAttachmentDesc
{
    TextureFormat format = TF_RGBA8;
    TextureType imageType = TT_TEX2D;
    LoadOperation loadOp = LoadOperation::CLEAR;
    StoreOperation storeOp = StoreOperation::STORE;
    Vec4f clearColor = Vec4f::Zero();
};

struct ViewOutputTargetDesc
{
    Vec2u extent = Vec2u::One();
    Array<ViewOutputTargetAttachmentDesc> attachments;
    uint32 numViews = 1;
};

struct ViewDesc
{
    EnumFlags<ViewFlags> flags = ViewFlags::DEFAULT;
    Viewport viewport;
    ViewOutputTargetDesc outputTargetDesc;
    Array<Scene*> scenes;
    Camera* camera = nullptr;
    int priority = 0;
    float resolutionScale = 1.0f;
    Optional<RenderableAttributeSet> overrideAttributes;
    EntityBatchAllocatorBase* batchAllocator = nullptr;
    TextureFormat readbackTextureFormat = TF_R10G10B10A2; //!< If ENABLE_READBACK is set, the format of the texture we copy the output to.
};

class HYP_API ViewOutputTarget
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
    AnyHandle m_impl;
};

HYP_CLASS()
class HYP_API View final : public ObjectBase
{
    HYP_OBJECT_BODY(View);

    friend class World;

public:
    View();

    View(const ViewDesc& viewDesc);

    View(const View& other) = delete;
    View& operator=(const View& other) = delete;

    View(View&& other) noexcept = delete;
    View& operator=(View&& other) noexcept = delete;

    ~View();

    HYP_FORCE_INLINE const ViewDesc& GetViewDesc() const
    {
        return m_viewDesc;
    }

    HYP_FORCE_INLINE EnumFlags<ViewFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE const Array<Handle<Scene>>& GetScenes() const
    {
        return m_scenes;
    }

    HYP_METHOD()
    void AddScene(const Handle<Scene>& scene);

    HYP_METHOD()
    void RemoveScene(Scene* scene);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Camera>& GetCamera() const
    {
        return m_camera;
    }

    HYP_FORCE_INLINE const ViewOutputTarget& GetOutputTarget() const
    {
        return m_outputTarget;
    }

    /*! \brief Get the Viewport of this View.
     *  Thread-safe but only callable from the Game thread or Render thread as it is buffered over multiple frames. */
    const Viewport& GetViewport() const;

    /*! \brief Set the Viewport of this View.
     *  Only callable on the Game thread. */
    void SetViewport(const Viewport& viewport);

    HYP_FORCE_INLINE const Handle<Texture>& GetReadbackTexture() const
    {
        AssertOnThread(g_gameThread);

        return m_readbackTexture;
    }

    GpuImageBase* GetReadbackTextureGpuImage() const;

    HYP_METHOD()
    HYP_FORCE_INLINE int GetPriority() const
    {
        return m_priority;
    }

    HYP_METHOD()
    void SetPriority(int priority);

    HYP_FORCE_INLINE const Optional<RenderableAttributeSet>& GetOverrideAttributes() const
    {
        return m_overrideAttributes;
    }

    HYP_FORCE_INLINE RenderProxyList* GetRenderProxyList(uint32 index) const
    {
        return m_renderProxyLists[index];
    }

    HYP_FORCE_INLINE const WeakHandle<View>& GetRaytracingView() const
    {
        return m_raytracingView;
    }

    bool TestRay(const Ray& ray, RayTestResults& outResults, EnumFlags<RayTestFlags> flags = RTF_USE_BVH) const;

    /*! \brief Sync changes to the Viewport so the render thread can see updates to it for the next frame */
    void UpdateViewport();

    /*! \brief Computes visibility states for all Scenes this View has using the Camera */
    void UpdateVisibility();

    /*! \brief Enqueue tasks to `batch` to asynchronously collect entities and other scene resources for the current View. */
    void BeginAsyncCollection(TaskBatch& batch);
    /*! \brief End asynchronous scene collection tasks */
    void EndAsyncCollection();

    /*! \brief Synchronously collect scene resources for the View, blocks the current thread until complete. */
    void CollectSync();

    Delegate<void, const Handle<Texture>&> OnReadbackTextureChanged;

protected:
    void Init() override;

    void CreateReadbackTexture();

    void CollectCameras(RenderProxyList& rpl);
    void CollectLights(RenderProxyList& rpl);
    void CollectLightmapVolumes(RenderProxyList& rpl);
    void CollectParticleVolumes(RenderProxyList& rpl);
    void CollectEnvGrids(RenderProxyList& rpl);
    void CollectEnvProbes(RenderProxyList& rpl);
    void CollectMeshEntities(RenderProxyList& rpl);

    ViewDesc m_viewDesc;

    EnumFlags<ViewFlags> m_flags;

    Array<Handle<Scene>> m_scenes;
    Handle<Camera> m_camera;
    ViewOutputTarget m_outputTarget;

    // optional raytracing View set by the world
    WeakHandle<View> m_raytracingView;

    RenderProxyList* m_renderProxyLists[RingBufferDepth];

    Viewport m_viewport;
    Viewport m_viewportBuffered[RingBufferDepth];

    // ViewID m_viewId; // unique Id for this view in the current frame

    int m_priority;

    Handle<Texture> m_readbackTexture;
    GpuImageBase* m_readbackTextureGpuImages[RingBufferDepth];

    Optional<RenderableAttributeSet> m_overrideAttributes;

    TaskBatch* m_collectionTaskBatch;
};

} // namespace hyperion
