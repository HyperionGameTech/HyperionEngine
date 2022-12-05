#ifndef HYPERION_V2_SCREENSPACE_REFLECTION_RENDERER_H
#define HYPERION_V2_SCREENSPACE_REFLECTION_RENDERER_H

#include "Compute.hpp"
#include <Constants.hpp>

#include <core/Containers.hpp>

#include <rendering/TemporalBlending.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererBuffer.hpp>
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
using renderer::UniformBuffer;

class Engine;

struct RenderCommand_CreateSSRImageOutputs;
struct RenderCommand_DestroySSRInstance;

class SSRRenderer
{
    static constexpr bool use_temporal_blending = true;

public:
    friend struct RenderCommand_CreateSSRImageOutputs;
    friend struct RenderCommand_DestroySSRInstance;

    SSRRenderer(const Extent2D &extent);
    ~SSRRenderer();

    bool IsRendered() const { return m_is_rendered; }

    void Create();
    void Destroy();

    void Render(Frame *frame);

private:
    void CreateUniformBuffers();
    void CreateDescriptorSets();
    void CreateComputePipelines();
    
    struct ImageOutput
    {
        StorageImage image;
        ImageView image_view;

        void Create(Device *device)
        {
            HYPERION_ASSERT_RESULT(image.Create(device));
            HYPERION_ASSERT_RESULT(image_view.Create(device, &image));
        }

        void Destroy(Device *device)
        {
            HYPERION_ASSERT_RESULT(image.Destroy(device));
            HYPERION_ASSERT_RESULT(image_view.Destroy(device));
        }
    };

    Extent2D m_extent;

    FixedArray<FixedArray<ImageOutput, 4>, max_frames_in_flight> m_image_outputs;
    FixedArray<ImageOutput, max_frames_in_flight> m_radius_output;
    FixedArray<UniquePtr<UniformBuffer>, max_frames_in_flight> m_uniform_buffers;
    FixedArray<UniquePtr<DescriptorSet>, max_frames_in_flight> m_descriptor_sets;
    
    Handle<ComputePipeline> m_write_uvs;
    Handle<ComputePipeline> m_sample;
    Handle<ComputePipeline> m_blur_hor;
    Handle<ComputePipeline> m_blur_vert;

    TemporalBlending m_temporal_blending;

    bool m_is_rendered;
};

} // namespace hyperion::v2

#endif