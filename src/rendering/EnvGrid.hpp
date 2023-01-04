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
    static const Extent2D reflection_probe_dimensions;
    static const Extent2D ambient_probe_dimensions;
    static const Float overlap_amount;

public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_ENV_GRID;

    EnvGrid(const BoundingBox &aabb, const Extent3D &density);
    EnvGrid(const EnvGrid &other) = delete;
    EnvGrid &operator=(const EnvGrid &other) = delete;
    virtual ~EnvGrid();

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

    void CreateClipmapComputeShader();
    void ComputeClipmaps(Frame *frame);

    void RenderEnvProbe(
        Frame *frame,
        Handle<Scene> &scene,
        Handle<EnvProbe> &probe
    );

    BoundingBox m_aabb;
    Extent3D m_density;

    Handle<Scene> m_reflection_scene;
    Handle<Scene> m_ambient_scene;
    Handle<Shader> m_reflection_shader;
    Handle<Shader> m_ambient_shader;
    Handle<Framebuffer> m_framebuffer;
    std::vector<std::unique_ptr<Attachment>> m_attachments;

    Handle<EnvProbe> m_reflection_probe;
    Array<Handle<EnvProbe>> m_ambient_probes;

    Handle<ComputePipeline> m_compute_clipmaps;
    FixedArray<DescriptorSetRef, max_frames_in_flight> m_compute_clipmaps_descriptor_sets;

    EnvGridShaderData m_shader_data;
    UInt m_current_probe_index;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_ENV_PROBE_HPP
