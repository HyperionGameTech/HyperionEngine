/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_VIEW_HPP
#define HYPERION_VIEW_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>

#include <core/math/Ray.hpp>

#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Variant.hpp>

#include <core/memory/resource/Resource.hpp>

#include <rendering/RenderCollection.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <GameCounter.hpp>

#include <Types.hpp>

namespace hyperion {

class RenderView;
class Scene;
class Camera;
class Light;
class RenderLight;
class LightmapVolume;
class RenderLightmapVolume;
class EnvGrid;
class RenderEnvGrid;
class EnvProbe;
class RenderEnvProbe;
class GBuffer;

// /// ViewID is used to identify a View in a single frame. When a View is used in a frame, the global render state assigns an ID to it.
// /// It is not persistent across frames, and should not be used to identify a View across multiple frames.
// using ViewID = uint32;
// constexpr ViewID invalid_view_id = ViewID(-1);
// constexpr ViewID max_view_id = 15;

enum class ViewFlags : uint32
{
    NONE = 0x0,
    GBUFFER = 0x1,

    ALL_WORLD_SCENES = 0x2, //!< If set, all scenes added to the world will be added view, and removed when removed from the world. Otherwise, the View itself manages the scenes it contains.

    COLLECT_STATIC_ENTITIES = 0x4,  //!< If set, the view will collect static entities (those that are not dynamic). Dynamic entities are those that move or are animated.
    COLLECT_DYNAMIC_ENTITIES = 0x8, //!< If set, the view will collect dynamic entities (those that are not static). Static entities are those that do not move and are not animated.
    COLLECT_ALL_ENTITIES = COLLECT_STATIC_ENTITIES | COLLECT_DYNAMIC_ENTITIES,

    SKIP_FRUSTUM_CULLING = 0x10, //!< If set, the view will not perform frustum culling. This is useful for debugging or when you want to render everything regardless of visibility.

    SKIP_ENV_PROBES = 0x20,        //!< If set, the view will not collect EnvProbes. Use for RenderEnvProbe, so that it does not collect itself!
    SKIP_ENV_GRIDS = 0x40,         //!< If set, the view will not collect EnvGrids.
    SKIP_LIGHTS = 0x80,            //!< If set, the view will not collect Lights.
    SKIP_LIGHTMAP_VOLUMES = 0x100, //!< If set, the view will not collect LightmapVolumes.

    DEFAULT = ALL_WORLD_SCENES | COLLECT_ALL_ENTITIES
};

HYP_MAKE_ENUM_FLAGS(ViewFlags)

struct ViewOutputTargetAttachmentDesc
{
    InternalFormat format = InternalFormat::RGBA8;
    ImageType image_type = ImageType::TEXTURE_TYPE_2D;
    renderer::LoadOperation load_op = renderer::LoadOperation::LOAD;
    renderer::StoreOperation store_op = renderer::StoreOperation::STORE;
    Vec4f clear_color = Vec4f::Zero();
};

struct ViewOutputTargetDesc
{
    Vec2u extent = Vec2u::One();
    Array<ViewOutputTargetAttachmentDesc> attachments;
    uint32 num_views = 1;
};

struct ViewDesc
{
    EnumFlags<ViewFlags> flags = ViewFlags::DEFAULT;
    Viewport viewport;
    ViewOutputTargetDesc output_target_desc;
    Array<Handle<Scene>> scenes;
    Handle<Camera> camera;
    int priority = 0;
    Optional<RenderableAttributeSet> override_attributes;
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

    GBuffer* GetGBuffer() const;
    const FramebufferRef& GetFramebuffer() const;
    const FramebufferRef& GetFramebuffer(Bucket bucket) const;

private:
    Variant<FramebufferRef, GBuffer*> m_impl;
};

HYP_CLASS()
class HYP_API View final : public HypObject<View>
{
    HYP_OBJECT_BODY(View);

public:
    View();

    View(const ViewDesc& view_desc);

    View(const View& other) = delete;
    View& operator=(const View& other) = delete;

    View(View&& other) noexcept = delete;
    View& operator=(View&& other) noexcept = delete;

    ~View();

    HYP_FORCE_INLINE RenderView& GetRenderResource() const
    {
        return *m_render_resource;
    }

    HYP_FORCE_INLINE const ViewDesc& GetViewDesc() const
    {
        return m_view_desc;
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
        return m_output_target;
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
        return m_override_attributes;
    }

    HYP_FORCE_INLINE const typename RenderProxyTracker::Diff& GetLastCollectionResult() const
    {
        return m_last_collection_result;
    }

    bool TestRay(const Ray& ray, RayTestResults& out_results, bool use_bvh = true) const;

    void UpdateVisibility();
    void Update(float delta);

protected:
    void Init() override;
    
    void CollectLights();
    void CollectLightmapVolumes();
    void CollectEnvGrids();
    void CollectEnvProbes();

    typename RenderProxyTracker::Diff CollectEntities(RenderProxyList& rpl);

    typename RenderProxyTracker::Diff CollectAllEntities(RenderProxyList& rpl);
    typename RenderProxyTracker::Diff CollectDynamicEntities(RenderProxyList& rpl);
    typename RenderProxyTracker::Diff CollectStaticEntities(RenderProxyList& rpl);

    ViewDesc m_view_desc;

    RenderView* m_render_resource;

    EnumFlags<ViewFlags> m_flags;

    Viewport m_viewport;

    Array<Handle<Scene>> m_scenes;
    Handle<Camera> m_camera;
    ViewOutputTarget m_output_target;

    // ViewID m_view_id; // unique ID for this view in the current frame

    int m_priority;

    Optional<RenderableAttributeSet> m_override_attributes;

    // Game thread side collection
    RenderProxyTracker m_render_proxy_tracker;

    ResourceTracker<ID<Light>, RenderLight*> m_tracked_lights;
    ResourceTracker<ID<LightmapVolume>, RenderLightmapVolume*> m_tracked_lightmap_volumes;
    ResourceTracker<ID<EnvGrid>, RenderEnvGrid*> m_tracked_env_grids;
    ResourceTracker<ID<EnvProbe>, RenderEnvProbe*> m_tracked_env_probes;

    typename RenderProxyTracker::Diff m_last_collection_result;
};

} // namespace hyperion

#endif