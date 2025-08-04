/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypObject.hpp>

#include <core/Handle.hpp>

#include <core/math/Ray.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Variant.hpp>

#include <core/memory/resource/Resource.hpp>

#include <rendering/RenderCollection.hpp>
#include <rendering/Shared.hpp>
#include <rendering/RenderObject.hpp>

#include <GameCounter.hpp>
#include <Constants.hpp>
#include <Types.hpp>

namespace hyperion {

class Scene;
class Camera;
class Light;
class LightmapVolume;
class EnvGrid;
class EnvProbe;
class GBuffer;
class IDrawCallCollectionImpl;

namespace threading {

class TaskBatch;

} // namespace threading

using threading::TaskBatch;

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
    SKIP_CAMERAS = 0x200,          //!< If set, the view will not collect Cameras.

    NOT_MULTI_BUFFERED = 0x1000, //!< Disables double / triple buffering for the RenderProxyList this View writes to.
                                 //  --- Use ONLY for Views that are not written to every frame, and instead are written to and read once (or infrequently); e.g EnvProbes.
                                 //  --- Use of these is still threadsafe, however it uses a spinlock instead of multiple buffering so contentions will eat up cpu cycles.
    NO_DRAW_CALLS = 0x2000,      //!< If set, no draw calls will be built for any mesh entities that this View collects.

    // enable flags
    RAYTRACING = 0x100000, //!< Does this View contain raytracing data (acceleration structures)? (Raytracing must be enabled in the global config and must have RT hardware support)

    DEFAULT = ALL_WORLD_SCENES | COLLECT_ALL_ENTITIES
};

HYP_MAKE_ENUM_FLAGS(ViewFlags)

struct ViewOutputTargetAttachmentDesc
{
    TextureFormat format = TF_RGBA8;
    TextureType imageType = TT_TEX2D;
    LoadOperation loadOp = LoadOperation::LOAD;
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
    Array<Handle<Scene>> scenes;
    Handle<Camera> camera;
    int priority = 0;
    Optional<RenderableAttributeSet> overrideAttributes;
    IDrawCallCollectionImpl* drawCallCollectionImpl = nullptr;
};

class HYP_API ViewOutputTarget
{
public:
    ViewOutputTarget();
    ViewOutputTarget(const FramebufferRef& framebuffer);
    ViewOutputTarget(GBuffer* gbuffer);
    ViewOutputTarget(const ViewOutputTarget& other) = delete;
    ViewOutputTarget& operator=(const ViewOutputTarget& other) = delete;
    ViewOutputTarget(ViewOutputTarget&& other) noexcept = default;
    ViewOutputTarget& operator=(ViewOutputTarget&& other) noexcept = default;
    ~ViewOutputTarget() = default;

    HYP_FORCE_INLINE bool IsValid() const
    {
        bool isValid = false;

        m_impl.Visit([&isValid](auto&& value)
            {
                isValid = bool(value);
            });

        return isValid;
    }

    GBuffer* GetGBuffer() const;
    const FramebufferRef& GetFramebuffer() const;
    const FramebufferRef& GetFramebuffer(RenderBucket rb) const;
    Span<const FramebufferRef> GetFramebuffers() const;

private:
    Variant<FramebufferRef, GBuffer*> m_impl;
};

HYP_CLASS()
class HYP_API View final : public HypObjectBase
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
    void RemoveScene(const Handle<Scene>& scene);

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Camera>& GetCamera() const
    {
        return m_camera;
    }

    HYP_FORCE_INLINE const ViewOutputTarget& GetOutputTarget() const
    {
        return m_outputTarget;
    }

    HYP_FORCE_INLINE const Viewport& GetViewport() const
    {
        return m_viewport;
    }

    void SetViewport(const Viewport& viewport);

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

    HYP_FORCE_INLINE const ResourceTrackerDiff& GetLastMeshCollectionResult() const
    {
        return m_lastMeshCollectionResult;
    }

    HYP_FORCE_INLINE RenderProxyList* GetRenderProxyList(uint32 index) const
    {
        return m_renderProxyLists[index];
    }

    HYP_FORCE_INLINE const WeakHandle<View>& GetRaytracingView() const
    {
        return m_raytracingView;
    }

    bool TestRay(const Ray& ray, RayTestResults& outResults, bool useBvh = true) const;

    void UpdateVisibility();

    void BeginAsyncCollection(TaskBatch& batch);
    void EndAsyncCollection();

    void CollectSync();

protected:
    void Init() override;

    void CollectCameras(RenderProxyList& rpl);
    void CollectLights(RenderProxyList& rpl);
    void CollectLightmapVolumes(RenderProxyList& rpl);
    void CollectEnvGrids(RenderProxyList& rpl);
    void CollectEnvProbes(RenderProxyList& rpl);

    ResourceTrackerDiff CollectMeshEntities(RenderProxyList& rpl);

    ViewDesc m_viewDesc;

    EnumFlags<ViewFlags> m_flags;

    Viewport m_viewport;

    Array<Handle<Scene>> m_scenes;
    Handle<Camera> m_camera;
    ViewOutputTarget m_outputTarget;

    // optional raytracing View set by the world
    WeakHandle<View> m_raytracingView;

    RenderProxyList* m_renderProxyLists[g_tripleBuffer ? 3 : 2];

    // ViewID m_viewId; // unique Id for this view in the current frame

    int m_priority;

    Optional<RenderableAttributeSet> m_overrideAttributes;

    ResourceTrackerDiff m_lastMeshCollectionResult;

    TaskBatch* m_collectionTaskBatch;
};

} // namespace hyperion

