#ifndef HYPERION_V2_POINT_SHADOW_RENDERER_HPP
#define HYPERION_V2_POINT_SHADOW_RENDERER_HPP

#include <core/Base.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderComponent.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <core/lib/FixedArray.hpp>

#include <math/BoundingBox.hpp>
#include <scene/camera/Camera.hpp>
#include <Types.hpp>

#include <array>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
;

class Light;

class PointShadowRenderer : public RenderComponent<PointShadowRenderer>
{
public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_POINT_SHADOW;

    PointShadowRenderer(const Handle<Light> &light, const Extent2D &extent);
    PointShadowRenderer(const PointShadowRenderer &other) = delete;
    PointShadowRenderer &operator=(const PointShadowRenderer &other) = delete;
    virtual ~PointShadowRenderer() override;

    void Init();
    void InitGame(); // init on game thread
    void OnRemoved();

    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    Handle<Light> m_light;
    Extent2D m_extent;
    BoundingBox m_aabb;
    Handle<EnvProbe> m_env_probe;

    bool m_last_visibility_state = false;
};


} // namespace hyperion::v2

#endif
