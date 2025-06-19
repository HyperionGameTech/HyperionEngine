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
class RenderCamera;
class View;
class RenderView;
struct RenderSetup;

class UIRenderCollector : RenderCollector
{
public:
    UIRenderCollector();
    UIRenderCollector(const UIRenderCollector& other) = delete;
    UIRenderCollector& operator=(const UIRenderCollector& other) = delete;
    UIRenderCollector(UIRenderCollector&& other) noexcept = default;
    UIRenderCollector& operator=(UIRenderCollector&& other) noexcept = default;
    ~UIRenderCollector();

    void ResetOrdering();

    void PushRenderProxy(RenderProxyTracker& render_proxy_tracker, const RenderProxy& render_proxy, int computed_depth);

    typename RenderProxyTracker::Diff PushUpdatesToRenderThread(
        RenderProxyTracker& render_proxy_tracker,
        const FramebufferRef& framebuffer,
        const Optional<RenderableAttributeSet>& override_attributes = {});

    void CollectDrawCalls(FrameBase* frame, const RenderSetup& render_setup);
    void ExecuteDrawCalls(FrameBase* frame, const RenderSetup& render_setup, const FramebufferRef& framebuffer) const;

private:
    Array<Pair<ID<Entity>, int>> m_proxy_depths;
};

HYP_CLASS(NoScriptBindings)
class HYP_API UIRenderSubsystem : public RenderSubsystem
{
    HYP_OBJECT_BODY(UIRenderSubsystem);

public:
    UIRenderSubsystem(Name name, const Handle<UIStage>& ui_stage);
    UIRenderSubsystem(const UIRenderSubsystem& other) = delete;
    UIRenderSubsystem& operator=(const UIRenderSubsystem& other) = delete;
    virtual ~UIRenderSubsystem();

    HYP_FORCE_INLINE const Handle<UIStage>& GetUIStage() const
    {
        return m_ui_stage;
    }

    HYP_FORCE_INLINE const FramebufferRef& GetFramebuffer() const
    {
        return m_framebuffer;
    }

    HYP_FORCE_INLINE UIRenderCollector& GetRenderCollector()
    {
        return m_render_collector;
    }

    HYP_FORCE_INLINE const UIRenderCollector& GetRenderCollector() const
    {
        return m_render_collector;
    }

    HYP_FORCE_INLINE RenderProxyTracker& GetRenderProxyTracker()
    {
        return m_render_proxy_tracker;
    }

    HYP_FORCE_INLINE const RenderProxyTracker& GetRenderProxyTracker() const
    {
        return m_render_proxy_tracker;
    }

private:
    virtual void Init() override;
    virtual void InitGame() override; // init on game thread
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnRender(FrameBase* frame, const RenderSetup& render_setup) override;

    void CreateFramebuffer();

    Handle<UIStage> m_ui_stage;

    FramebufferRef m_framebuffer;
    ShaderRef m_shader;
    UIRenderCollector m_render_collector;

    // Game thread side list, used for collecting UI objects
    RenderProxyTracker m_render_proxy_tracker;

    TResourceHandle<RenderCamera> m_camera_resource_handle;

    Handle<View> m_view;
    TResourceHandle<RenderView> m_render_view;

    DelegateHandler m_on_gbuffer_resolution_changed_handle;
};

} // namespace hyperion

#endif
