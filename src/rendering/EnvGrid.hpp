#ifndef HYPERION_V2_ENV_GRID_HPP
#define HYPERION_V2_ENV_GRID_HPP

#include <core/Base.hpp>
#include <rendering/EntityDrawCollection.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/EnvProbe.hpp>
#include <core/Containers.hpp>

namespace hyperion::v2 {

class Entity;

class EnvGrid : public RenderComponent<EnvGrid>
{
public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_ENV_GRID;

    EnvGrid(const BoundingBox &aabb, const Extent3D &density);
    EnvGrid(const EnvGrid &other) = delete;
    EnvGrid &operator=(const EnvGrid &other) = delete;
    virtual ~EnvGrid();

    void SetCameraData(const BoundingBox &aabb);

    void Init();
    void InitGame(); // init on game thread
    void OnRemoved();

    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    void CreateShader();
    void CreateFramebuffer();

    void RenderEnvProbe(
        Frame *frame,
        Handle<EnvProbe> &probe
    );

    BoundingBox m_aabb;
    Extent3D m_density;
    
    Handle<Camera> m_camera;
    RenderList m_render_list;

    Handle<Shader> m_ambient_shader;
    Handle<Framebuffer> m_framebuffer;
    std::vector<std::unique_ptr<Attachment>> m_attachments;
    
    Array<Handle<EnvProbe>> m_ambient_probes;

    EnvGridShaderData m_shader_data;
    UInt m_current_probe_index;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_ENV_PROBE_HPP
