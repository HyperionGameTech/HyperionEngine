#ifndef HYPERION_V2_ENV_GRID_HPP
#define HYPERION_V2_ENV_GRID_HPP

#include <core/Base.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/EnvProbe.hpp>
#include <core/Containers.hpp>

namespace hyperion::v2 {

class Scene;
class Entity;

class EnvGrid : public RenderComponent<EnvGrid>
{
    static const Extent3D density;
    static const Extent2D cubemap_dimensions;

public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_ENV_GRID;

    EnvGrid(const BoundingBox &aabb);
    EnvGrid(const EnvGrid &other) = delete;
    EnvGrid &operator=(const EnvGrid &other) = delete;
    ~EnvGrid();

    const BoundingBox &GetAABB() const
        { return m_aabb; }

    void SetAABB(const BoundingBox &aabb)
        { m_aabb = aabb; }

    void Init();
    void InitGame(); // init on game thread
    void OnRemoved();

    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    void CreateShader();
    void CreateFramebuffer();

    BoundingBox m_aabb;
    Handle<Scene> m_scene;
    Handle<Shader> m_shader;
    Handle<Framebuffer> m_framebuffer;
    std::vector<std::unique_ptr<Attachment>> m_attachments;
    Array<Handle<EnvProbe>> m_env_probes;
    UInt m_current_probe_index;

    Handle<Entity> m_entity;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_ENV_PROBE_HPP
