/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SSGI_HPP
#define HYPERION_SSGI_HPP

#include <rendering/TemporalBlending.hpp>
#include <rendering/FullScreenPass.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/config/Config.hpp>

#include <core/object/HypObject.hpp>

#include <core/utilities/EnumFlags.hpp>

namespace hyperion {

class Engine;
class GBuffer;
struct SSGIUniforms;
class RenderView;

HYP_STRUCT(ConfigName = "app", JSONPath = "rendering.ssgi")
struct SSGIConfig : public ConfigBase<SSGIConfig>
{
    HYP_FIELD(Description = "The quality level of the SSGI effect. (0 = quarter res, 1 = half res)")
    int quality = 0;

    HYP_FIELD(JSONIgnore)
    Vec2u extent;

    virtual ~SSGIConfig() override = default;

    void PostLoadCallback()
    {
        extent = Vec2u { 1024, 1024 };

        switch (quality)
        {
        case 0:
            extent /= 2;
            break;
        default:
            break;
        }
    }
};

class SSGI
{
public:
    SSGI(SSGIConfig&& config, GBuffer* gbuffer);
    ~SSGI();

    HYP_FORCE_INLINE const Handle<Texture>& GetResultTexture() const
    {
        return m_result_texture;
    }

    const Handle<Texture>& GetFinalResultTexture() const;

    HYP_FORCE_INLINE bool IsRendered() const
    {
        return m_is_rendered;
    }

    void Create();

    void Render(FrameBase* frame, const RenderSetup& render_setup);

private:
    ShaderProperties GetShaderProperties() const;

    void CreateUniformBuffers();
    void CreateBlueNoiseBuffer();
    void CreateComputePipelines();

    void FillUniformBufferData(RenderView* view, SSGIUniforms& out_uniforms) const;

    SSGIConfig m_config;

    GBuffer* m_gbuffer;

    Handle<Texture> m_result_texture;

    FixedArray<GpuBufferRef, max_frames_in_flight> m_uniform_buffers;

    ComputePipelineRef m_compute_pipeline;

    UniquePtr<TemporalBlending> m_temporal_blending;

    bool m_is_rendered;
};

} // namespace hyperion

#endif