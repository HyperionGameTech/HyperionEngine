#ifndef HYPERION_V2_DEFERRED_H
#define HYPERION_V2_DEFERRED_H

#include "FullScreenPass.hpp"
#include "PostFX.hpp"
#include "Renderer.hpp"
#include "Texture.hpp"
#include "Compute.hpp"

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Device;

class DeferredPass : public FullScreenPass {
    friend class DeferredRenderer;
public:
    DeferredPass(bool is_indirect_pass);
    DeferredPass(const DeferredPass &other) = delete;
    DeferredPass &operator=(const DeferredPass &other) = delete;
    ~DeferredPass();

    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void Create(Engine *engine);

    void Destroy(Engine *engine);
    void Record(Engine *engine, UInt frame_index);
    void Render(Engine *engine, Frame *frame);

private:
    const bool m_is_indirect_pass;
};

class DeferredRenderer : public Renderer {
public:
    DeferredRenderer();
    DeferredRenderer(const DeferredRenderer &other) = delete;
    DeferredRenderer &operator=(const DeferredRenderer &other) = delete;
    ~DeferredRenderer();

    PostProcessing &GetPostProcessing()              { return m_post_processing; }
    const PostProcessing &GetPostProcessing() const  { return m_post_processing; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame);

private:
    void CreateComputePipelines(Engine *engine);
    
    struct SSRImageOutput {
        std::unique_ptr<Image>     image;
        std::unique_ptr<ImageView> image_view;

        void Create(Device *device)
        {
            AssertThrow(image != nullptr);
            AssertThrow(image_view != nullptr);

            HYPERION_ASSERT_RESULT(image->Create(device));
            HYPERION_ASSERT_RESULT(image_view->Create(device, image.get()));
        }

        void Destroy(Device *device)
        {
            AssertThrow(image != nullptr);
            AssertThrow(image_view != nullptr);

            HYPERION_ASSERT_RESULT(image->Destroy(device));
            HYPERION_ASSERT_RESULT(image_view->Destroy(device));
        }
    };

    void RenderOpaqueObjects(Engine *engine, Frame *frame);
    void RenderTranslucentObjects(Engine *engine, Frame *frame);

    DeferredPass   m_indirect_pass;
    DeferredPass   m_direct_pass;
    PostProcessing m_post_processing;

    std::array<Ref<Texture>, max_frames_in_flight>                  m_mipmapped_results;
    std::unique_ptr<Sampler>                                        m_sampler;
    std::array<std::array<SSRImageOutput, 4>, max_frames_in_flight> m_ssr_image_outputs;
    std::array<SSRImageOutput, max_frames_in_flight>                m_ssr_radius_output;

    Ref<ComputePipeline>                                            m_ssr_write_uvs;
    Ref<ComputePipeline>                                            m_ssr_sample;
    Ref<ComputePipeline>                                            m_ssr_blur_hor;
    Ref<ComputePipeline>                                            m_ssr_blur_vert;
};

} // namespace hyperion::v2

#endif