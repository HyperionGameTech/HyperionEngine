#ifndef HYPERION_V2_SKYDOME_RENDERER_HPP
#define HYPERION_V2_SKYDOME_RENDERER_HPP

#include <core/Base.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/EnvProbe.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;

class HYP_API SkydomeRenderer : public RenderComponent<SkydomeRenderer>
{
public:
    SkydomeRenderer(Name name, Extent2D dimensions = { 1024, 1024 });
    virtual ~SkydomeRenderer() = default;

    const Handle<Texture> &GetCubemap() const
        { return m_cubemap; }

    void Init();
    void InitGame();

    void OnRemoved();
    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    Extent2D            m_dimensions;
    Handle<Texture>     m_cubemap;
    Handle<Camera>      m_camera;

    Handle<Scene>       m_virtual_scene;
    Handle<EnvProbe>    m_env_probe;

    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override
        { }
};

} // namespace hyperion::v2

#endif