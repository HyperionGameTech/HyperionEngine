/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_UI_RENDERER_HPP
#define HYPERION_UI_RENDERER_HPP

#include <core/Base.hpp>
#include <core/memory/UniquePtr.hpp>

#include <rendering/RenderGroup.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/EntityDrawCollection.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

#include <Types.hpp>

namespace hyperion {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;

class UIStage;
class UIObject;

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

    void CollectObjects(const NodeProxy &node, Array<RC<UIObject>> &out_objects);

    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override
        { AssertThrowMsg(false, "Not permitted!"); }

    RC<UIStage>                     m_ui_stage;
    Handle<Framebuffer>             m_framebuffer;
    Handle<Shader>                  m_shader;
    RenderList                      m_render_list;
};

} // namespace hyperion

#endif
