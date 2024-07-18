/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCREENSPACE_REFLECTION_RENDERER_HPP
#define HYPERION_SCREENSPACE_REFLECTION_RENDERER_HPP

#include <rendering/TemporalBlending.hpp>
#include <rendering/FullScreenPass.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

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

    Extent2D                        m_extent;

    FixedArray<Handle<Texture>, 4>  m_image_outputs;
    
    GPUBufferRef                    m_uniform_buffer;
    
    ComputePipelineRef              m_write_uvs;
    ComputePipelineRef              m_sample;

    UniquePtr<TemporalBlending>     m_temporal_blending;

    SSRRendererOptions              m_options;

    bool                            m_is_rendered;
};

} // namespace hyperion

#endif