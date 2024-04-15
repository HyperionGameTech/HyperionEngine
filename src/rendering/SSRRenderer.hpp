/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_SCREENSPACE_REFLECTION_RENDERER_H
#define HYPERION_V2_SCREENSPACE_REFLECTION_RENDERER_H

#include <Constants.hpp>

#include <core/Containers.hpp>

#include <rendering/TemporalBlending.hpp>
#include <rendering/FullScreenPass.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Device;
using renderer::AttachmentUsage;

class Engine;

struct RenderCommand_CreateSSRImageOutputs;
struct RenderCommand_DestroySSRInstance;

using SSRRendererOptions = uint32;

enum SSRRendererOptionBits : SSRRendererOptions
{
    SSR_RENDERER_OPTIONS_NONE                   = 0x0,
    SSR_RENDERER_OPTIONS_CONE_TRACING           = 0x1,
    SSR_RENDERER_OPTIONS_ROUGHNESS_SCATTERING   = 0x2
};

class SSRRenderer
{
public:
    friend struct RenderCommand_CreateSSRImageOutputs;
    friend struct RenderCommand_DestroySSRInstance;

    SSRRenderer(const Extent2D &extent, SSRRendererOptions options);
    ~SSRRenderer();

    bool IsRendered() const
        { return m_is_rendered; }

    void Create();
    void Destroy();

    void Render(Frame *frame);

private:
    ShaderProperties GetShaderProperties() const;

    void CreateUniformBuffers();
    void CreateBlueNoiseBuffer();
    void CreateComputePipelines();

    Extent2D m_extent;

    FixedArray<Handle<Texture>, 4> m_image_outputs;
    
    GPUBufferRef                    m_uniform_buffer;
    
    ComputePipelineRef              m_write_uvs;
    ComputePipelineRef              m_sample;

    UniquePtr<TemporalBlending>     m_temporal_blending;

    SSRRendererOptions              m_options;

    bool                            m_is_rendered;
};

} // namespace hyperion::v2

#endif