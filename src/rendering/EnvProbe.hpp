#ifndef HYPERION_V2_ENV_PROBE_HPP
#define HYPERION_V2_ENV_PROBE_HPP

#include <core/Base.hpp>
#include <core/lib/Optional.hpp>
#include <math/BoundingBox.hpp>
#include <rendering/Texture.hpp>
#include <rendering/DrawProxy.hpp>
#include <rendering/Buffers.hpp>

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererBuffer.hpp>

namespace hyperion::v2 {

struct RenderCommand_UpdateEnvProbeRenderData;
struct RenderCommand_CreateCubemapBuffers;
struct RenderCommand_DestroyCubemapRenderPass;

class Framebuffer;

using renderer::Attachment;
using renderer::UniformBuffer;

class EnvProbe
    : public EngineComponentBase<STUB_CLASS(EnvProbe)>,
      public HasDrawProxy<STUB_CLASS(EnvProbe)>,
      public RenderResource
{

    static const Extent2D cubemap_dimensions;
    static const FixedArray<std::pair<Vector3, Vector3>, 6> cubemap_directions;

public:
    friend struct RenderCommand_UpdateEnvProbeRenderData;
    friend struct RenderCommand_CreateCubemapBuffers;
    friend struct RenderCommand_DestroyCubemapRenderPass;

    EnvProbe(const Handle<Scene> &parent_scene);
    EnvProbe(const Handle<Scene> &parent_scene, const BoundingBox &aabb);
    EnvProbe(const EnvProbe &other) = delete;
    EnvProbe &operator=(const EnvProbe &other) = delete;
    ~EnvProbe();

    const Matrix4 &GetProjectionMatrix() const
        { return m_projection_matrix; }

    const FixedArray<Matrix4, 6> &GetViewMatrices() const
        { return m_view_matrices; }

    const BoundingBox &GetAABB() const
        { return m_aabb; }

    void SetAABB(const BoundingBox &aabb)
        { m_aabb = aabb; SetNeedsUpdate(); }

    void Init();
    void EnqueueBind() const;
    void EnqueueUnbind() const;
    void Update();
    void Render(Frame *frame);

private:
    void SetNeedsUpdate() { m_needs_update = true; }

    void CreateImagesAndBuffers();
    void CreateShader();
    void CreateFramebuffer();

    Handle<Scene> m_parent_scene;
    BoundingBox m_aabb;
    Handle<Texture> m_texture;
    Handle<Framebuffer> m_framebuffer;
    std::vector<std::unique_ptr<Attachment>> m_attachments;
    Handle<Shader> m_shader;
    Handle<Scene> m_scene;

    CubemapUniforms m_cubemap_uniforms;
    FixedArray<UniquePtr<UniformBuffer>, max_frames_in_flight> m_cubemap_render_uniform_buffers;

    Matrix4 m_projection_matrix;
    FixedArray<Matrix4, 6> m_view_matrices;

    bool m_needs_update;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_ENV_PROBE_HPP
