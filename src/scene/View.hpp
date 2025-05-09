/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_VIEW_HPP
#define HYPERION_VIEW_HPP

#include <core/Base.hpp>
#include <core/Handle.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/memory/resource/Resource.hpp>

#include <rendering/RenderCollection.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <GameCounter.hpp>

#include <Types.hpp>

namespace hyperion {

class ViewRenderResource;
class Scene;
class Camera;
class Light;

enum class ViewFlags : uint32
{
    NONE    = 0x0,
    GBUFFER = 0x1,
};

HYP_MAKE_ENUM_FLAGS(ViewFlags)

struct ViewDesc
{
    EnumFlags<ViewFlags>    flags = ViewFlags::NONE;
    Viewport                viewport;
    Handle<Scene>           scene;
    Handle<Camera>          camera;
    int                     priority = 0;
};

HYP_CLASS()
class HYP_API View : public HypObject<View>
{
    HYP_OBJECT_BODY(View);

public:
    View();

    View(const ViewDesc &view_desc);

    View(const View &other)                 = delete;
    View &operator=(const View &other)      = delete;

    View(View &&other) noexcept             = delete;
    View &operator=(View &&other) noexcept  = delete;

    ~View();

    HYP_FORCE_INLINE ViewRenderResource &GetRenderResource() const
        { return *m_render_resource; }

    HYP_FORCE_INLINE EnumFlags<ViewFlags> GetFlags() const
        { return m_flags; }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Scene> &GetScene() const
        { return m_scene; }

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<Camera> &GetCamera() const
        { return m_camera; }

    HYP_METHOD()
    HYP_FORCE_INLINE const Array<Handle<Light>> &GetLights() const
        { return m_lights; }

    HYP_METHOD()
    void AddLight(const Handle<Light> &light);

    HYP_METHOD()
    void RemoveLight(const Handle<Light> &light);

    HYP_FORCE_INLINE const Viewport &GetViewport() const
        { return m_viewport; }

    void SetViewport(const Viewport &viewport);

    HYP_METHOD()
    HYP_FORCE_INLINE int GetPriority() const
        { return m_priority; }

    HYP_METHOD()
    void SetPriority(int priority);

    void Init();
    void Update(GameCounter::TickUnit delta);

    
    RenderCollector::CollectionResult CollectEntities(bool skip_frustum_culling = false);
    RenderCollector::CollectionResult CollectDynamicEntities(bool skip_frustum_culling = false);
    RenderCollector::CollectionResult CollectStaticEntities(bool skip_frustum_culling = false);
    
protected:
    void CollectLights();
    
    RenderCollector::CollectionResult CollectEntities(RenderCollector &render_collector, bool skip_frustum_culling = false);
    RenderCollector::CollectionResult CollectDynamicEntities(RenderCollector &render_collector, bool skip_frustum_culling = false);
    RenderCollector::CollectionResult CollectStaticEntities(RenderCollector &render_collector, bool skip_frustum_culling = false);

    ViewRenderResource      *m_render_resource;

    EnumFlags<ViewFlags>    m_flags;

    Viewport                m_viewport;
    
    Handle<Scene>           m_scene;
    Handle<Camera>          m_camera;
    Array<Handle<Light>>    m_lights;

    int                     m_priority;
};

} // namespace hyperion

#endif