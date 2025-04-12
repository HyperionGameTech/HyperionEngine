/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RT_RADIANCE_RENDERER_HPP
#define HYPERION_RT_RADIANCE_RENDERER_HPP

#include <Constants.hpp>

#include <rendering/Shader.hpp>
#include <rendering/TemporalBlending.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>

namespace hyperion {

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
        const Vec2u &extent,
        RTRadianceRendererOptions options = RT_RADIANCE_RENDERER_OPTION_NONE
    );

    HYP_API ~RTRadianceRenderer();

    HYP_FORCE_INLINE bool IsPathTracer() const
        { return m_options & RT_RADIANCE_RENDERER_OPTION_PATHTRACER; }
    
    HYP_FORCE_INLINE void SetTopLevelAccelerationStructures(const FixedArray<TLASRef, max_frames_in_flight> &tlas)
        { m_top_level_acceleration_structures = tlas; }

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

    Vec2u                                               m_extent;

    FixedArray<TLASRef, max_frames_in_flight>           m_top_level_acceleration_structures;
    
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