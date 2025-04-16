/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_RENDERER_HPP
#define HYPERION_UI_RENDERER_HPP

#include <core/Base.hpp>

#include <core/functional/Delegate.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/RenderSubsystem.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <scene/Scene.hpp>

namespace hyperion {

class UIStage;
class UIObject;
class CameraRenderResource;

class UIRenderCollector : public RenderCollector
{
public:
    UIRenderCollector();
    UIRenderCollector(const UIRenderCollector &other)                 = delete;
    UIRenderCollector &operator=(const UIRenderCollector &other)      = delete;
    UIRenderCollector(UIRenderCollector &&other) noexcept             = default;
    UIRenderCollector &operator=(UIRenderCollector &&other) noexcept  = default;
    ~UIRenderCollector();

    void ResetOrdering();

    void PushEntityToRender(ID<Entity> entity, const RenderProxy &proxy, int computed_depth);

    CollectionResult PushUpdatesToRenderThread(
        const Handle<Camera> &camera = Handle<Camera>::empty,
        const Optional<RenderableAttributeSet> &override_attributes = { }
    );

    void CollectDrawCalls(IFrame *frame);
    void ExecuteDrawCalls(IFrame *frame, const TResourceHandle<CameraRenderResource> &camera_resource_handle, const FramebufferRef &framebuffer) const;

private:
    Array<Pair<ID<Entity>, int>>    m_proxy_depths;
};

HYP_CLASS()
class HYP_API UIRenderSubsystem : public RenderSubsystem
{
    HYP_OBJECT_BODY(UIRenderSubsystem);

public:
    UIRenderSubsystem(Name name, const RC<UIStage> &ui_stage);
    UIRenderSubsystem(const UIRenderSubsystem &other)             = delete;
    UIRenderSubsystem &operator=(const UIRenderSubsystem &other)  = delete;
    virtual ~UIRenderSubsystem();

    HYP_FORCE_INLINE const RC<UIStage> &GetUIStage() const
        { return m_ui_stage; }

    HYP_FORCE_INLINE UIRenderCollector &GetRenderCollector()
        { return m_render_collector; }

    HYP_FORCE_INLINE const UIRenderCollector &GetRenderCollector() const
        { return m_render_collector; }

private:
    virtual void Init() override;
    virtual void InitGame() override; // init on game thread
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnRender(IFrame *frame) override;

    virtual void OnComponentIndexChanged(RenderSubsystem::Index new_index, RenderSubsystem::Index prev_index) override
        { AssertThrowMsg(false, "Not permitted!"); }

    void CreateFramebuffer();

    RC<UIStage>                             m_ui_stage;

    FramebufferRef                          m_framebuffer;
    ShaderRef                               m_shader;
    UIRenderCollector                       m_render_collector;

    TResourceHandle<CameraRenderResource>   m_camera_resource_handle;

    DelegateHandler                         m_on_gbuffer_resolution_changed_handle;
};

} // namespace hyperion

#endif
