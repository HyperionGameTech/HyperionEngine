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
class View;
class ViewRenderResource;

class UIRenderCollector : RenderCollector
{
public:
    using RenderCollector::GetDrawCollection;

    UIRenderCollector();
    UIRenderCollector(const UIRenderCollector &other)                 = delete;
    UIRenderCollector &operator=(const UIRenderCollector &other)      = delete;
    UIRenderCollector(UIRenderCollector &&other) noexcept             = default;
    UIRenderCollector &operator=(UIRenderCollector &&other) noexcept  = default;
    ~UIRenderCollector();

    void ResetOrdering();

    void PushRenderProxy(RenderProxyList &proxy_list, const RenderProxy &render_proxy, int computed_depth);

    CollectionResult PushUpdatesToRenderThread(
        const FramebufferRef &framebuffer,
        const Optional<RenderableAttributeSet> &override_attributes = { }
    );

    void CollectDrawCalls(FrameBase *frame);
    void ExecuteDrawCalls(
        FrameBase *frame,
        ViewRenderResource *view,
        const FramebufferRef &framebuffer
    ) const;

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

    HYP_FORCE_INLINE const FramebufferRef &GetFramebuffer() const
        { return m_framebuffer; }

    HYP_FORCE_INLINE UIRenderCollector &GetRenderCollector()
        { return m_render_collector; }

    HYP_FORCE_INLINE const UIRenderCollector &GetRenderCollector() const
        { return m_render_collector; }

private:
    virtual void Init() override;
    virtual void InitGame() override; // init on game thread
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnRender(FrameBase *frame) override;

    virtual void OnComponentIndexChanged(RenderSubsystem::Index new_index, RenderSubsystem::Index prev_index) override
        { AssertThrowMsg(false, "Not permitted!"); }

    void CreateFramebuffer();

    RC<UIStage>                             m_ui_stage;

    FramebufferRef                          m_framebuffer;
    ShaderRef                               m_shader;
    UIRenderCollector                       m_render_collector;

    TResourceHandle<CameraRenderResource>   m_camera_resource_handle;

    Handle<View>                            m_view;
    TResourceHandle<ViewRenderResource>     m_view_render_resource_handle;

    DelegateHandler                         m_on_gbuffer_resolution_changed_handle;
};

} // namespace hyperion

#endif
