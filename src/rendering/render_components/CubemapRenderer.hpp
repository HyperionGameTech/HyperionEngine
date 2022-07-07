#ifndef HYPERION_V2_CUBEMAP_RENDERER_H
#define HYPERION_V2_CUBEMAP_RENDERER_H

#include "../Base.hpp"
#include "../PostFx.hpp"
#include "../Renderer.hpp"
#include "../Light.hpp"
#include "../RenderComponent.hpp"
#include "../Buffers.hpp"

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

class CubemapRenderer : public EngineComponentBase<STUB_CLASS(CubemapRenderer)>, public RenderComponent<CubemapRenderer> {
public:
    CubemapRenderer(
        const Extent2D &cubemap_dimensions = Extent2D { 512, 512 },
        const Vector3 &origin              = Vector3::Zero(),
        Image::FilterMode filter_mode      = Image::FilterMode::TEXTURE_FILTER_LINEAR
    );

    CubemapRenderer(
        const Extent2D &cubemap_dimensions = Extent2D { 512, 512 },
        const BoundingBox &aabb            = BoundingBox(-100.0f, 100.0f),
        Image::FilterMode filter_mode      = Image::FilterMode::TEXTURE_FILTER_LINEAR
    );

    CubemapRenderer(const CubemapRenderer &other) = delete;
    CubemapRenderer &operator=(const CubemapRenderer &other) = delete;
    ~CubemapRenderer();

    Ref<Texture> &GetCubemap(UInt frame_index)             { return m_cubemaps[frame_index]; }
    const Ref<Texture> &GetCubemap(UInt frame_index) const { return m_cubemaps[frame_index]; }

    void Init(Engine *engine);
    void InitGame(Engine *engine); // init on game thread

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
    void CreateGraphicsPipelines(Engine *engine);
    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateFramebuffers(Engine *engine);

    virtual void OnEntityAdded(Ref<Spatial> &spatial);
    virtual void OnEntityRemoved(Ref<Spatial> &spatial);
    virtual void OnEntityRenderableAttributesChanged(Ref<Spatial> &spatial);
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    Extent2D                                           m_cubemap_dimensions;
    BoundingBox                                        m_aabb;
    Image::FilterMode                                  m_filter_mode;
    Ref<Scene>                                         m_scene;
    std::array<Ref<Framebuffer>, max_frames_in_flight> m_framebuffers;
    Ref<Shader>                                        m_shader;
    Ref<RenderPass>                                    m_render_pass;
    Ref<GraphicsPipeline>                              m_pipeline;
    std::vector<std::unique_ptr<Attachment>>           m_attachments;
    std::array<Ref<Texture>, max_frames_in_flight>     m_cubemaps;
    UniformBuffer                                      m_cubemap_render_uniform_buffer;
    UniformBuffer                                      m_env_probe_uniform_buffer;
};


} // namespace hyperion::v2

#endif
