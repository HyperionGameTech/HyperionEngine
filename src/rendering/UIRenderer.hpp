/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_UI_RENDERER_HPP
#define HYPERION_UI_RENDERER_HPP

#include <core/Base.hpp>

#include <rendering/RenderComponent.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <scene/Scene.hpp>

namespace hyperion {

class UIStage;
class UIObject;

class UIRenderList : public RenderList
{
public:
    UIRenderList();
    UIRenderList(const Handle<Camera> &camera);
    UIRenderList(const UIRenderList &other)                 = delete;
    UIRenderList &operator=(const UIRenderList &other)      = delete;
    UIRenderList(UIRenderList &&other) noexcept             = default;
    UIRenderList &operator=(UIRenderList &&other) noexcept  = default;
    ~UIRenderList();

    HYP_FORCE_INLINE
    SizeType GetProxyCount() const
        { return m_proxy_ordering.Size(); }

    void ResetOrdering();

    void PushEntityToRender(
        ID<Entity> entity,
        const RenderProxy &proxy
    );

    RenderListCollectionResult PushUpdatesToRenderThread(
        const FramebufferRef &framebuffer = nullptr,
        const Optional<RenderableAttributeSet> &override_attributes = { }
    );

    void CollectDrawCalls(Frame *frame);
    void ExecuteDrawCalls(Frame *frame) const;

private:
    Array<ID<Entity>>   m_proxy_ordering;
};

class HYP_API UIRenderer
    : public RenderComponent<UIRenderer>
{
public:
    UIRenderer(Name name, RC<UIStage> ui_stage);
    UIRenderer(const UIRenderer &other)             = delete;
    UIRenderer &operator=(const UIRenderer &other)  = delete;
    virtual ~UIRenderer();

    void Init();
    void InitGame(); // init on game thread
    void OnRemoved();

    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    void CreateFramebuffer();

    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override
        { AssertThrowMsg(false, "Not permitted!"); }

    RC<UIStage>     m_ui_stage;
    FramebufferRef  m_framebuffer;
    ShaderRef       m_shader;
    UIRenderList    m_render_list;
};

} // namespace hyperion

#endif
