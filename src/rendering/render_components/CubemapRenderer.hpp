#ifndef HYPERION_V2_CUBEMAP_RENDERER_H
#define HYPERION_V2_CUBEMAP_RENDERER_H

#include "../Base.hpp"
#include "../PostFX.hpp"
#include "../Renderer.hpp"
#include "../Light.hpp"
#include "../RenderComponent.hpp"
#include "../Buffers.hpp"
#include "../EnvProbe.hpp"

#include <core/lib/FixedArray.hpp>

#include <rendering/backend/RendererFrame.hpp>

#include <math/BoundingBox.hpp>
#include <scene/Scene.hpp>
#include <camera/Camera.hpp>
#include <Types.hpp>

#include <array>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::Extent2D;

class CubemapRenderer
    : public EngineComponentBase<STUB_CLASS(CubemapRenderer)>,
      public RenderComponent<CubemapRenderer>
{
public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_CUBEMAP;

    CubemapRenderer(
        const Extent2D &cubemap_dimensions = Extent2D { 512, 512 },
        const Vector3 &origin = Vector3::zero,
        Image::FilterMode filter_mode = Image::FilterMode::TEXTURE_FILTER_LINEAR
    );

    CubemapRenderer(
        const Extent2D &cubemap_dimensions = Extent2D { 512, 512 },
        const BoundingBox &aabb = BoundingBox(-100.0f, 100.0f),
        Image::FilterMode filter_mode = Image::FilterMode::TEXTURE_FILTER_LINEAR
    );

    CubemapRenderer(const CubemapRenderer &other) = delete;
    CubemapRenderer &operator=(const CubemapRenderer &other) = delete;
    ~CubemapRenderer();

    Handle<Texture> &GetCubemap(UInt frame_index) { return m_cubemaps[frame_index]; }
    const Handle<Texture> &GetCubemap(UInt frame_index) const { return m_cubemaps[frame_index]; }

    Ref<EnvProbe> &GetEnvProbe() { return m_env_probe; }
    const Ref<EnvProbe> &GetEnvProbe() const { return m_env_probe; }

    void Init(Engine *engine);
    void InitGame(Engine *engine); // init on game thread
    void OnRemoved();

    void OnUpdate(Engine *engine, GameCounter::TickUnit delta);
    void OnRender(Engine *engine, Frame *frame);

private:
    static const FixedArray<std::pair<Vector3, Vector3>, 6> cubemap_directions;

    Image *GetCubemapImage(UInt frame_index) const
    {
        return m_framebuffers[frame_index]->GetFramebuffer().GetAttachmentRefs()[0]->GetAttachment()->GetImage();
    }

    ImageView *GetCubemapImageView(UInt frame_index) const
    {
        return m_framebuffers[frame_index]->GetFramebuffer().GetAttachmentRefs()[0]->GetImageView();
    }

    void CreateImagesAndBuffers(Engine *engine);
    void CreateRendererInstance(Engine *engine);
    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateFramebuffers(Engine *engine);

    virtual void OnEntityAdded(Ref<Entity> &entity) override;
    virtual void OnEntityRemoved(Ref<Entity> &entity) override;
    virtual void OnEntityRenderableAttributesChanged(Ref<Entity> &entity) override;
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    Extent2D m_cubemap_dimensions;
    BoundingBox m_aabb;
    Image::FilterMode m_filter_mode;
    Handle<Scene> m_scene;
    FixedArray<Ref<Framebuffer>, max_frames_in_flight> m_framebuffers;
    Handle<Shader> m_shader;
    Handle<RenderPass> m_render_pass;
    Handle<RendererInstance> m_renderer_instance;
    std::vector<std::unique_ptr<Attachment>> m_attachments;
    FixedArray<Handle<Texture>, max_frames_in_flight> m_cubemaps;
    Ref<EnvProbe> m_env_probe;
    FixedArray<std::unique_ptr<UniformBuffer>, max_frames_in_flight> m_cubemap_render_uniform_buffers;
};


} // namespace hyperion::v2

#endif
