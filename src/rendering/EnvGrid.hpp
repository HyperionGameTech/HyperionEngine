#ifndef HYPERION_V2_ENV_GRID_HPP
#define HYPERION_V2_ENV_GRID_HPP

#include <core/Base.hpp>
#include <core/lib/AtomicVar.hpp>
#include <rendering/EntityDrawCollection.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/EnvProbe.hpp>
#include <core/Containers.hpp>

namespace hyperion::v2 {

class Entity;

using EnvGridFlags = UInt32;

enum EnvGridFlagBits : EnvGridFlags
{
    ENV_GRID_FLAGS_NONE            = 0x0,
    ENV_GRID_FLAGS_RESET_REQUESTED = 0x1
};

struct RenderCommand_UpdateEnvProbeAABBsInGrid;

class EnvGrid : public RenderComponent<EnvGrid>
{
public:
    friend struct RenderCommand_UpdateEnvProbeAABBsInGrid;

    static constexpr RenderComponentName component_name = RENDER_COMPONENT_ENV_GRID;

    EnvGrid(const BoundingBox &aabb, const Extent3D &density);
    EnvGrid(const EnvGrid &other) = delete;
    EnvGrid &operator=(const EnvGrid &other) = delete;
    virtual ~EnvGrid();

    void SetCameraData(const Vector3 &camera_position);

    void Init();
    void InitGame(); // init on game thread
    void OnRemoved();

    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    void CreateShader();
    void CreateFramebuffer();

    void CreateSHData();
    void CreateSHClipmapData();
    void ComputeClipmaps(Frame *frame);

    void RenderEnvProbe(
        Frame *frame,
        UInt32 probe_index
    );

    void ComputeSH(
        Frame *frame,
        const Image *image,
        const ImageView *image_view,
        UInt32 probe_index
    );

    BoundingBox m_aabb;
    Extent3D m_density;
    
    Handle<Camera> m_camera;
    RenderList m_render_list;

    Handle<Shader> m_ambient_shader;
    Handle<Framebuffer> m_framebuffer;
    std::vector<std::unique_ptr<Attachment>> m_attachments;
    
    Array<Handle<EnvProbe>> m_ambient_probes;
    Array<const EnvProbeDrawProxy *> m_env_probe_draw_proxies;

    Handle<ComputePipeline> m_compute_clipmaps;
    FixedArray<DescriptorSetRef, max_frames_in_flight> m_compute_clipmaps_descriptor_sets;

    EnvGridShaderData m_shader_data;
    UInt m_current_probe_index;

    Vector3 m_grid_offset;

    AtomicVar<EnvGridFlags> m_flags;

    Handle<ComputePipeline> m_compute_sh;
    Handle<ComputePipeline> m_clear_sh;
    Handle<ComputePipeline> m_finalize_sh;
    FixedArray<DescriptorSetRef, max_frames_in_flight> m_compute_sh_descriptor_sets;
    GPUBufferRef m_sh_tiles_buffer;

    Queue<UInt> m_next_render_indices;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_ENV_PROBE_HPP
