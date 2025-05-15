/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RT_RADIANCE_RENDERER_HPP
#define HYPERION_RT_RADIANCE_RENDERER_HPP

#include <Constants.hpp>

#include <rendering/Shader.hpp>
#include <rendering/TemporalBlending.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>

#include <core/config/Config.hpp>

namespace hyperion {

using renderer::RTUpdateStateFlags;

class Engine;
class GBuffer;
class ViewRenderResource;

struct RenderCommand_DestroyRTRadianceRenderer;
struct RenderCommand_CreateRTRadianceImageOutputs;

HYP_STRUCT(ConfigName="app", ConfigPath="rendering.rt.reflections")
struct RTRadianceConfig : public ConfigBase<RTRadianceConfig>
{
    HYP_FIELD()
    Vec2u   extent = { 1024, 1024 };
    
    HYP_FIELD()
    bool    path_tracing = false;

    virtual ~RTRadianceConfig() override = default;

    bool Validate() const
    {
        return extent.x * extent.y != 0;
    }
};

class RTRadianceRenderer
{
public:
    friend struct RenderCommand_DestroyRTRadianceRenderer;
    friend struct RenderCommand_CreateRTRadianceImageOutputs;
 
    HYP_API RTRadianceRenderer(
        RTRadianceConfig &&config,
        GBuffer *gbuffer
    );

    HYP_API ~RTRadianceRenderer();

    HYP_FORCE_INLINE bool IsPathTracer() const
        { return m_config.path_tracing; }
    
    HYP_FORCE_INLINE void SetTopLevelAccelerationStructures(const FixedArray<TLASRef, max_frames_in_flight> &tlas)
        { m_top_level_acceleration_structures = tlas; }

    HYP_API void ApplyTLASUpdates(RTUpdateStateFlags flags);

    HYP_API void Create();
    HYP_API void Destroy();

    HYP_API void Render(FrameBase *frame, ViewRenderResource *view);

private:
    void CreateImages();
    void CreateUniformBuffer();
    void CreateRaytracingPipeline();
    void CreateTemporalBlending();
    void UpdateUniforms(FrameBase *frame, ViewRenderResource *view);

    RTRadianceConfig                                    m_config;

    GBuffer                                             *m_gbuffer;

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