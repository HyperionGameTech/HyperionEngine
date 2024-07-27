/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TEMPORAL_BLENDING_HPP
#define HYPERION_TEMPORAL_BLENDING_HPP

#include <Constants.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/functional/Delegate.hpp>

#include <rendering/Shader.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

using renderer::Result;

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
    friend struct RenderCommand_RecreateTemporalBlendingFramebuffer;

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
        const FramebufferRef &input_framebuffer
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

    TemporalBlendTechnique GetTechnique() const
        { return m_technique; }

    TemporalBlendFeedback GetFeedback() const
        { return m_feedback; }

    const Handle<Texture> &GetResultTexture() const
        { return m_result_texture; }

    const Handle<Texture> &GetHistoryTexture() const
        { return m_history_texture; }

    void ResetProgressiveBlending();

    void Create();
    void Render(Frame *frame);

    void Resize(Extent2D new_size);

private:
    void Resize_Internal(Extent2D new_size);

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
    FramebufferRef                                  m_input_framebuffer;

    Handle<Texture>                                 m_result_texture;
    Handle<Texture>                                 m_history_texture;

    DelegateHandler                                 m_after_swapchain_recreated_delegate;

    bool                                            m_is_initialized;
};

} // namespace hyperion

#endif