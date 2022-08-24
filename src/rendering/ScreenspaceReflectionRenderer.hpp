#ifndef HYPERION_V2_SCREENSPACE_REFLECTION_RENDERER_H
#define HYPERION_V2_SCREENSPACE_REFLECTION_RENDERER_H

#include "Compute.hpp"
#include <Constants.hpp>

#include <core/Containers.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Device;
using renderer::DescriptorSet;
using renderer::AttachmentRef;
using renderer::Extent2D;

class Engine;

class ScreenspaceReflectionRenderer {
public:
    ScreenspaceReflectionRenderer(const Extent2D &extent);
    ~ScreenspaceReflectionRenderer();

    bool IsRendered() const { return m_is_rendered; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);

    void Render(
        Engine *engine,
        Frame *frame
    );

private:
    void CreateDescriptors(Engine *engine);
    void CreateComputePipelines(Engine *engine);
    
    struct SSRImageOutput {
        std::unique_ptr<Image> image;
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

    Extent2D m_extent;

    FixedArray<std::array<SSRImageOutput, 4>, max_frames_in_flight> m_ssr_image_outputs;
    FixedArray<SSRImageOutput, max_frames_in_flight> m_ssr_radius_output;

    Handle<ComputePipeline> m_ssr_write_uvs;
    Handle<ComputePipeline> m_ssr_sample;
    Handle<ComputePipeline> m_ssr_blur_hor;
    Handle<ComputePipeline> m_ssr_blur_vert;

    bool m_is_rendered;
};

} // namespace hyperion::v2

#endif