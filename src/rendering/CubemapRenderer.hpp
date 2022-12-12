#ifndef HYPERION_V2_CUBEMAP_RENDERER_H
#define HYPERION_V2_CUBEMAP_RENDERER_H

#include <core/Base.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/Renderer.hpp>
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
using renderer::Extent2D;

struct RenderCommand_CreateCubemapImages;
struct RenderCommand_DestroyCubemapRenderPass;

class CubemapRenderer
    : public EngineComponentBase<STUB_CLASS(CubemapRenderer)>,
      public RenderComponent<CubemapRenderer>
{
    friend struct RenderCommand_CreateCubemapImages;
    friend struct RenderCommand_DestroyCubemapRenderPass;

public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_CUBEMAP;

    CubemapRenderer(
        const Extent2D &cubemap_dimensions = Extent2D { 512, 512 },
        const Vector3 &origin = Vector3::zero,
        FilterMode filter_mode = FilterMode::TEXTURE_FILTER_LINEAR
    );

    CubemapRenderer(
        const Extent2D &cubemap_dimensions = Extent2D { 512, 512 },
        const BoundingBox &aabb = BoundingBox(-100.0f, 100.0f),
        FilterMode filter_mode = FilterMode::TEXTURE_FILTER_LINEAR
    );

    CubemapRenderer(const CubemapRenderer &other) = delete;
    CubemapRenderer &operator=(const CubemapRenderer &other) = delete;
    ~CubemapRenderer();

    Handle<Texture> &GetCubemap(UInt frame_index) { return m_cubemaps[frame_index]; }
    const Handle<Texture> &GetCubemap(UInt frame_index) const { return m_cubemaps[frame_index]; }

    Handle<EnvProbe> &GetEnvProbe() { return m_env_probe; }
    const Handle<EnvProbe> &GetEnvProbe() const { return m_env_probe; }

    void Init();
    void InitGame(); // init on game thread
    void OnRemoved();

    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    static const FixedArray<std::pair<Vector3, Vector3>, 6> cubemap_directions;

    Image *GetCubemapImage(UInt frame_index) const
    {
        return m_framebuffer->GetAttachmentRefs()[0]->GetAttachment()->GetImage();
    }

    ImageView *GetCubemapImageView(UInt frame_index) const
    {
        return m_framebuffer->GetAttachmentRefs()[0]->GetImageView();
    }

    void CreateImagesAndBuffers();
    void CreateShader();
    void CreateFramebuffer();

    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    Extent2D m_cubemap_dimensions;
    BoundingBox m_aabb;
    FilterMode m_filter_mode;
    Handle<Scene> m_scene;
    Handle<Framebuffer> m_framebuffer;
    Handle<Shader> m_shader;
    Handle<RenderGroup> m_render_group;
    std::vector<std::unique_ptr<Attachment>> m_attachments;
    FixedArray<Handle<Texture>, max_frames_in_flight> m_cubemaps;
    Handle<EnvProbe> m_env_probe;

    CubemapUniforms m_cubemap_uniforms;
    FixedArray<UniquePtr<UniformBuffer>, max_frames_in_flight> m_cubemap_render_uniform_buffers;
};


} // namespace hyperion::v2

#endif
