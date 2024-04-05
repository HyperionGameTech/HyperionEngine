#ifndef HYPERION_V2_UI_RENDERER_H
#define HYPERION_V2_UI_RENDERER_H

#include <core/Base.hpp>
#include <core/lib/UniquePtr.hpp>

#include <rendering/RenderGroup.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/EntityDrawCollection.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;

class DebugDrawCommandList;

class UIRenderer
    : public RenderComponent<UIRenderer>
{
public:
    UIRenderer(Name name, Handle<Scene> ui_scene);
    UIRenderer(const UIRenderer &other)             = delete;
    UIRenderer &operator=(const UIRenderer &other)  = delete;
    virtual ~UIRenderer();

    void Init();
    void InitGame(); // init on game thread
    void OnRemoved();

    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

    void SetDebugRender(bool debug_render);

private:
    void CreateFramebuffer();
    void CreateDescriptors();

    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override
        { AssertThrowMsg(false, "Not permitted!"); }

    Handle<Scene>                   m_scene;
    Handle<Framebuffer>             m_framebuffer;
    Handle<Shader>                  m_shader;
    RenderList                      m_render_list;

    AtomicVar<bool>                 m_debug_render;
    UniquePtr<DebugDrawCommandList> m_debug_commands;
};


} // namespace hyperion::v2

#endif
