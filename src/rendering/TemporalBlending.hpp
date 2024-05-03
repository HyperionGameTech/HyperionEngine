/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TEMPORAL_BLENDING_HPP
#define HYPERION_TEMPORAL_BLENDING_HPP

#include <Constants.hpp>

#include <core/Containers.hpp>

#include <rendering/Shader.hpp>
#include <rendering/Framebuffer.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>

namespace hyperion {

using renderer::Frame;
using renderer::Image;
using renderer::StorageImage;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Device;
using renderer::AttachmentUsage;
;
using renderer::Result;

class Engine;

struct RenderCommand_CreateTemporalImageOutputs;

enum class TemporalBlendTechnique
{
    TECHNIQUE_0,
    TECHNIQUE_1,
    TECHNIQUE_2,
    TECHNIQUE_3,

    TECHNIQUE_4 // Progressive blending for path tracing
};

enum class TemporalBlendFeedback
{
    LOW,
    MEDIUM,
    HIGH
};

class TemporalBlending
{
public:
    friend struct RenderCommand_CreateTemporalImageOutputs;

    struct ImageOutput
    {
        ImageRef image;
        ImageViewRef image_view;

        Result Create(Device *device)
        {
            AssertThrow(image.IsValid());
            AssertThrow(image_view.IsValid());

            HYPERION_BUBBLE_ERRORS(image->Create(device));
            HYPERION_BUBBLE_ERRORS(image_view->Create(device, image.Get()));

            HYPERION_RETURN_OK;
        }
    };

    TemporalBlending(
        const Extent2D &extent,
        TemporalBlendTechnique technique,
        TemporalBlendFeedback feedback,
        const FixedArray<ImageViewRef, max_frames_in_flight> &input_image_views
    );

    TemporalBlending(
        const Extent2D &extent,
        InternalFormat image_format,
        TemporalBlendTechnique technique,
        TemporalBlendFeedback feedback,
        const Handle<Framebuffer> &input_framebuffer
    );

    TemporalBlending(
        const Extent2D &extent,
        InternalFormat image_format,
        TemporalBlendTechnique technique,
        TemporalBlendFeedback feedback,
        const FixedArray<ImageViewRef, max_frames_in_flight> &input_image_views
    );

    TemporalBlending(const TemporalBlending &other) = delete;
    TemporalBlending &operator=(const TemporalBlending &other) = delete;
    ~TemporalBlending();

    ImageOutput &GetImageOutput(uint frame_index)
        { return m_image_outputs[frame_index]; }

    const ImageOutput &GetImageOutput(uint frame_index) const
        { return m_image_outputs[frame_index]; }

    TemporalBlendTechnique GetTechnique() const
        { return m_technique; }

    TemporalBlendFeedback GetFeedback() const
        { return m_feedback; }

    void ResetProgressiveBlending();

    void Create();

    void Render(Frame *frame);

private:
    ShaderProperties GetShaderProperties() const;

    void CreateImageOutputs();
    void CreateDescriptorSets();
    void CreateComputePipelines();

    Extent2D                                        m_extent;
    InternalFormat                                  m_image_format;
    TemporalBlendTechnique                          m_technique;
    TemporalBlendFeedback                           m_feedback;

    uint16                                          m_blending_frame_counter;

    ComputePipelineRef                              m_perform_blending;
    DescriptorTableRef                              m_descriptor_table;

    FixedArray<ImageViewRef, max_frames_in_flight>  m_input_image_views;
    FixedArray<ImageOutput, max_frames_in_flight>   m_image_outputs;

    Handle<Framebuffer>                             m_input_framebuffer;
};

} // namespace hyperion

#endif