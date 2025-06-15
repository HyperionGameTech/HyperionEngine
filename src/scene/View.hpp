/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_VIEW_HPP
#define HYPERION_VIEW_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>

#include <core/math/Ray.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/memory/resource/Resource.hpp>

#include <rendering/RenderCollection.hpp>
#include <rendering/backend/RendererStructs.hpp>

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

enum class ViewFlags : uint32
{
    NONE = 0x0,
    GBUFFER = 0x1,

    ALL_WORLD_SCENES = 0x2, //!< If set, all scenes added to the world will be added view, and removed when removed from the world. Otherwise, the View itself manages the scenes it contains.

    DEFAULT = ALL_WORLD_SCENES
};

HYP_MAKE_ENUM_FLAGS(ViewFlags)

enum class ViewEntityCollectionFlags : uint32
{
    NONE = 0x0,
    COLLECT_STATIC = 0x1,
    COLLECT_DYNAMIC = 0x2,
    COLLECT_ALL = COLLECT_STATIC | COLLECT_DYNAMIC,

    SKIP_FRUSTUM_CULLING = 0x4,

    DEFAULT = COLLECT_ALL
};

HYP_MAKE_ENUM_FLAGS(ViewEntityCollectionFlags)

struct ViewDesc
{
    EnumFlags<ViewFlags> flags = ViewFlags::DEFAULT;
    Viewport viewport;
    Array<Handle<Scene>> scenes;
    Handle<Camera> camera;
    int priority = 0;
    EnumFlags<ViewEntityCollectionFlags> entity_collection_flags = ViewEntityCollectionFlags::DEFAULT;
    Optional<RenderableAttributeSet> override_attributes;
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

    HYP_FORCE_INLINE const typename RenderProxyTracker::Diff& GetLastCollectionResult() const
    {
        return m_last_collection_result;
    }

    bool TestRay(const Ray& ray, RayTestResults& out_results, bool use_bvh = true) const;

    void Update(GameCounter::TickUnit delta);

protected:
    void Init() override;
    
    void CollectLights();
    void CollectLightmapVolumes();

    typename RenderProxyTracker::Diff CollectEntities();

    typename RenderProxyTracker::Diff CollectAllEntities();
    typename RenderProxyTracker::Diff CollectDynamicEntities();
    typename RenderProxyTracker::Diff CollectStaticEntities();

    RenderView* m_render_resource;

    EnumFlags<ViewFlags> m_flags;

    Viewport m_viewport;

    Array<Handle<Scene>> m_scenes;
    Handle<Camera> m_camera;

    int m_priority;

    EnumFlags<ViewEntityCollectionFlags> m_entity_collection_flags;

    Optional<RenderableAttributeSet> m_override_attributes;

    // Game thread side collection
    RenderProxyTracker m_render_proxy_tracker;

    ResourceTracker<ID<Light>, RenderLight*> m_tracked_lights;
    ResourceTracker<ID<LightmapVolume>, RenderLightmapVolume*> m_tracked_lightmap_volumes;

    typename RenderProxyTracker::Diff m_last_collection_result;
};

} // namespace hyperion

#endif