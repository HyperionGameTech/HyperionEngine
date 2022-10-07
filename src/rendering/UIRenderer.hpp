#ifndef HYPERION_V2_UI_RENDERER_H
#define HYPERION_V2_UI_RENDERER_H

#include <core/Base.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/RenderComponent.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <scene/Scene.hpp>
#include <camera/Camera.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::Extent2D;

class UIRenderer
    : public EngineComponentBase<STUB_CLASS(UIRenderer)>,
      public RenderComponent<UIRenderer>
{
public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_UI;

    UIRenderer(Handle<Scene> &&ui_scene);
    UIRenderer(const UIRenderer &other) = delete;
    UIRenderer &operator=(const UIRenderer &other) = delete;
    ~UIRenderer();

    void Init(Engine *engine);
    void InitGame(Engine *engine); // init on game thread
    void OnRemoved();

    void OnUpdate(Engine *engine, GameCounter::TickUnit delta);
    void OnRender(Engine *engine, Frame *frame);

private:
    void CreateRenderPass(Engine *engine);
    void CreateFramebuffers(Engine *engine);
    void CreateDescriptors(Engine *engine);

    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override
        { AssertThrowMsg(false, "Not permitted!"); }

    Handle<Scene> m_scene;
    FixedArray<Handle<Framebuffer>, max_frames_in_flight> m_framebuffers;
    Handle<Shader> m_shader;
    Handle<RenderPass> m_render_pass;
};


} // namespace hyperion::v2

#endif
