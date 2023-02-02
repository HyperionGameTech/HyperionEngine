#ifndef HYPERION_V2_ENV_PROBE_HPP
#define HYPERION_V2_ENV_PROBE_HPP

#include <core/Base.hpp>
#include <core/lib/Optional.hpp>
#include <math/BoundingBox.hpp>
#include <rendering/Texture.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/EntityDrawCollection.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/Compute.hpp>
#include <rendering/RenderCommands.hpp>

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererImage.hpp>

namespace hyperion::v2 {

struct RenderCommand_UpdateEnvProbeDrawProxy;
struct RenderCommand_CreateCubemapBuffers;
struct RenderCommand_DestroyCubemapRenderPass;

class Framebuffer;

using renderer::Attachment;
using renderer::UniformBuffer;
using renderer::Image;

enum EnvProbeType : UInt
{
    ENV_PROBE_TYPE_INVALID = UInt(-1),
    ENV_PROBE_TYPE_REFLECTION = 0,
    ENV_PROBE_TYPE_AMBIENT,
    ENV_PROBE_TYPE_SHADOW,

    ENV_PROBE_TYPE_MAX
};

class EnvProbe;

struct RENDER_COMMAND(UpdateEnvProbeDrawProxy) : RenderCommand
{
    EnvProbe &env_probe;
    EnvProbeDrawProxy draw_proxy;

    RENDER_COMMAND(UpdateEnvProbeDrawProxy)(EnvProbe &env_probe, EnvProbeDrawProxy &&draw_proxy)
        : env_probe(env_probe),
          draw_proxy(std::move(draw_proxy))
    {
    }

    virtual Result operator()() override;
};

struct EnvProbeIndex
{
    Extent3D position;
    Extent3D grid_size;

    // defaults such that unset == ~0u
    EnvProbeIndex()
        : position { ~0u, ~0u, ~0u },
          grid_size { 0, 0, 0 }
    {
    }

    EnvProbeIndex(const Extent3D &position, const Extent3D &grid_size)
        : position(position),
          grid_size(grid_size)
    {
    }

    EnvProbeIndex(const EnvProbeIndex &other) = default;
    EnvProbeIndex &operator=(const EnvProbeIndex &other) = default;
    EnvProbeIndex(EnvProbeIndex &&other) noexcept = default;
    EnvProbeIndex &operator=(EnvProbeIndex &&other) noexcept = default;

    ~EnvProbeIndex() = default;

    UInt GetProbeIndex() const
    {
        return (position[0] * grid_size.height * grid_size.depth)
            + (position[1] * grid_size.depth)
            + position[2];
    }

    bool operator<(UInt value) const
        { return GetProbeIndex() < value; }

    bool operator==(UInt value) const
        { return GetProbeIndex() == value; }

    bool operator!=(UInt value) const
        { return GetProbeIndex() != value; }

    bool operator<(const EnvProbeIndex &other) const
        { return GetProbeIndex() < other.GetProbeIndex(); }

    bool operator==(const EnvProbeIndex &other) const
        { return GetProbeIndex() == other.GetProbeIndex(); }

    bool operator!=(const EnvProbeIndex &other) const
        { return GetProbeIndex() != other.GetProbeIndex(); }
};

class EnvProbe
    : public EngineComponentBase<STUB_CLASS(EnvProbe)>,
      public HasDrawProxy<STUB_CLASS(EnvProbe)>,
      public RenderResource
{
public:
    friend struct RenderCommand_UpdateEnvProbeDrawProxy;
    friend struct RenderCommand_DestroyCubemapRenderPass;
    
    EnvProbe(
        const Handle<Scene> &parent_scene,
        const BoundingBox &aabb,
        const Extent2D &dimensions,
        EnvProbeType env_probe_type
    );

    EnvProbe(const EnvProbe &other) = delete;
    EnvProbe &operator=(const EnvProbe &other) = delete;
    ~EnvProbe();

    EnvProbeType GetEnvProbeType() const
        { return m_env_probe_type; }

    bool IsReflectionProbe() const
        { return m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_REFLECTION; }

    bool IsAmbientProbe() const
        { return m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_AMBIENT; }

    bool IsShadowProbe() const
        { return m_env_probe_type == EnvProbeType::ENV_PROBE_TYPE_SHADOW; }

    const EnvProbeIndex &GetBoundIndex() const
        { return m_bound_index; }

    void SetBoundIndex(const EnvProbeIndex &bound_index)
        { m_bound_index = bound_index; }

    const Matrix4 &GetProjectionMatrix() const
        { return m_projection_matrix; }

    const FixedArray<Matrix4, 6> &GetViewMatrices() const
        { return m_view_matrices; }

    const BoundingBox &GetAABB() const
        { return m_aabb; }

    void SetAABB(const BoundingBox &aabb)
        { m_aabb = aabb; SetNeedsUpdate(true); }

    Handle<Texture> &GetTexture()
        { return m_texture; }

    const Handle<Texture> &GetTexture() const
        { return m_texture; }

    void SetNeedsUpdate(bool needs_update)
        { m_needs_update = needs_update; }

    bool NeedsUpdate() const
        { return m_needs_update; }

    void SetNeedsRender(bool needs_render)
    {
        if (needs_render) {
            // TEMP: Works when having multiple frames between states

            m_needs_render_counter.fetch_add(2, std::memory_order_relaxed);
        } else {
            m_needs_render_counter.fetch_sub(1, std::memory_order_relaxed);
        }
    }

    bool NeedsRender() const
    {
        const UInt16 counter = m_needs_render_counter.load(std::memory_order_relaxed);

        return counter != 0;
    }

    void Init();
    void EnqueueBind() const;
    void EnqueueUnbind() const;
    void Update(GameCounter::TickUnit delta);

    void Render(Frame *frame);

    void ComputeSH(Frame *frame, const Image *image, const ImageView *image_view);

    void UpdateRenderData(const EnvProbeIndex &probe_index);
    
private:
    void CreateShader();
    void CreateFramebuffer();

    void CreateSHData();

    Handle<Scene> m_parent_scene;
    BoundingBox m_aabb;
    Extent2D m_dimensions;
    EnvProbeType m_env_probe_type;

    Float m_camera_near;
    Float m_camera_far;

    Handle<Texture> m_texture;
    Handle<Framebuffer> m_framebuffer;
    std::vector<std::unique_ptr<Attachment>> m_attachments;
    Handle<Shader> m_shader;
    Handle<Camera> m_camera;
    RenderList m_render_list;

    GPUBufferRef m_sh_tiles_buffer;

    Handle<ComputePipeline> m_compute_sh;
    Handle<ComputePipeline> m_clear_sh;
    Handle<ComputePipeline> m_finalize_sh;
    FixedArray<DescriptorSetRef, max_frames_in_flight> m_compute_sh_descriptor_sets;

    Matrix4 m_projection_matrix;
    FixedArray<Matrix4, 6> m_view_matrices;

    EnvProbeIndex m_bound_index;

    bool m_needs_update;
    std::atomic_bool m_is_rendered;
    std::atomic<UInt16> m_needs_render_counter;
    HashCode m_octant_hash_code;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_ENV_PROBE_HPP
