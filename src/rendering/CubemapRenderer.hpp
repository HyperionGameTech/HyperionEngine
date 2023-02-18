#ifndef HYPERION_V2_CUBEMAP_RENDERER_H
#define HYPERION_V2_CUBEMAP_RENDERER_H

#include <core/Base.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Light.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/EnvProbe.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <core/lib/FixedArray.hpp>

#include <math/BoundingBox.hpp>
#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>
#include <Types.hpp>

#include <array>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;

class CubemapRenderer
    : public EngineComponentBase<STUB_CLASS(CubemapRenderer)>,
      public RenderComponent<CubemapRenderer>
{
public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_CUBEMAP;

    CubemapRenderer(
        const Vector3 &origin
    );

    CubemapRenderer(
        const BoundingBox &aabb
    );

    CubemapRenderer(const CubemapRenderer &other) = delete;
    CubemapRenderer &operator=(const CubemapRenderer &other) = delete;
    ~CubemapRenderer();

    Handle<EnvProbe> &GetEnvProbe() { return m_env_probe; }
    const Handle<EnvProbe> &GetEnvProbe() const { return m_env_probe; }

    void Init();
    void InitGame(); // init on game thread
    void OnRemoved();

    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    BoundingBox m_aabb;
    Handle<EnvProbe> m_env_probe;
};


} // namespace hyperion::v2

#endif
