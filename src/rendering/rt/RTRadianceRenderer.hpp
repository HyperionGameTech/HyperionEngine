/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RT_RADIANCE_RENDERER_HPP
#define HYPERION_RT_RADIANCE_RENDERER_HPP

#include <Constants.hpp>

#include <core/Containers.hpp>

#include <rendering/Shader.hpp>
#include <rendering/TemporalBlending.hpp>
#include <rendering/rt/TLAS.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>

namespace hyperion {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::StorageImage;
using renderer::Sampler;
using renderer::Device;
using renderer::Result;
using renderer::RaytracingPipeline;
using renderer::RTUpdateStateFlags;

class Engine;

struct RenderCommand_DestroyRTRadianceRenderer;
struct RenderCommand_CreateRTRadianceImageOutputs;

using RTRadianceRendererOptions = uint32;

enum RTRadianceRendererOptionBits : RTRadianceRendererOptions
{
    RT_RADIANCE_RENDERER_OPTION_NONE = 0x0,
    RT_RADIANCE_RENDERER_OPTION_PATHTRACER = 0x1
};

class RTRadianceRenderer
{
public:
    friend struct RenderCommand_DestroyRTRadianceRenderer;
    friend struct RenderCommand_CreateRTRadianceImageOutputs;
 
    HYP_API RTRadianceRenderer(
        const Extent2D &extent,
        RTRadianceRendererOptions options = RT_RADIANCE_RENDERER_OPTION_NONE
    );

    HYP_API ~RTRadianceRenderer();

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool IsPathTracer() const
        { return m_options & RT_RADIANCE_RENDERER_OPTION_PATHTRACER; }
    
    void SetTLAS(Handle<TLAS> tlas)
        { m_tlas = std::move(tlas); }

    HYP_API void ApplyTLASUpdates(RTUpdateStateFlags flags);

    HYP_API void Create();
    HYP_API void Destroy();

    HYP_API void Render(Frame *frame);

private:
    void CreateImages();
    void CreateUniformBuffer();
    void CreateRaytracingPipeline();
    void CreateTemporalBlending();
    void UpdateUniforms(Frame *frame);

    RTRadianceRendererOptions                           m_options;

    Extent2D                                            m_extent;
    Handle<TLAS>                                        m_tlas;
    
    FixedArray<uint32, max_frames_in_flight>            m_updates;

    ShaderRef                                           m_shader;

    Handle<Texture>                                     m_texture;
    UniquePtr<TemporalBlending>                         m_temporal_blending;

    RaytracingPipelineRef                               m_raytracing_pipeline;
    FixedArray<GPUBufferRef, max_frames_in_flight>      m_uniform_buffers;

    Matrix4                                             m_previous_view_matrix;
};

} // namespace hyperion

#endif