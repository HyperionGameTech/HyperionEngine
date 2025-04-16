/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCREENSPACE_REFLECTION_RENDERER_HPP
#define HYPERION_SCREENSPACE_REFLECTION_RENDERER_HPP

#include <rendering/TemporalBlending.hpp>
#include <rendering/FullScreenPass.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/config/Config.hpp>

#include <core/object/HypObject.hpp>

#include <core/utilities/EnumFlags.hpp>

namespace hyperion {

class Engine;

HYP_STRUCT(ConfigName="app", ConfigPath="rendering.ssr")
struct SSRRendererConfig : public ConfigBase<SSRRendererConfig>
{
    HYP_FIELD(Description="The quality level of the SSR effect. (0 = low, 1 = medium, 2 = high)")
    int     quality = 1;

    HYP_FIELD(Description="Enables scattering of rays based on the roughness of the surface. May cause artifacts due to temporal instability.")
    bool    roughness_scattering = true;

    HYP_FIELD(Description="Enables cone tracing for the SSR effect. Causes the result to become blurrier based on distance of the reflection.")
    bool    cone_tracing = false;

    HYP_FIELD(Description="The distance between rays when tracing the SSR effect.")
    float   ray_step = 3.2f;

    HYP_FIELD(Description="The maximum number of iterations to perform for the SSR effect before stopping.")
    uint32  num_iterations = 64;

    HYP_FIELD(Description="Where to start and end fading the SSR effect based on the eye vector.")
    Vec2f   eye_fade = { 0.98f, 0.99f };

    HYP_FIELD(Description="Where to start and end fading the SSR effect based on the screen edges.")
    Vec2f   screen_edge_fade = { 0.96f, 0.99f };

    HYP_FIELD(ConfigIgnore)
    Vec2u   extent;

    virtual ~SSRRendererConfig() override = default;

    static HYP_API Vec2u GetGBufferResolution();

    bool Validate() const
    {
        return extent.x * extent.y != 0
            && ray_step > 0.0f
            && num_iterations > 0;
    }

    void PostLoadCallback()
    {
        extent = GetGBufferResolution();

        switch (quality) {
        case 0:
            extent /= 4;
            break;
        case 1:
            extent /= 2;
            break;
        default:
            break;
        }
    }
};

class SSRRenderer
{
public:
    SSRRenderer(SSRRendererConfig &&config);
    ~SSRRenderer();

    HYP_FORCE_INLINE const Handle<Texture> &GetUVsTexture() const
        { return m_uvs_texture; }

    HYP_FORCE_INLINE const Handle<Texture> &GetSampledResultTexture() const
        { return m_sampled_result_texture; }

    const Handle<Texture> &GetFinalResultTexture() const;

    HYP_FORCE_INLINE bool IsRendered() const
        { return m_is_rendered; }

    void Create();
    void Destroy();

    void Render(FrameBase *frame);

private:
    ShaderProperties GetShaderProperties() const;

    void CreateUniformBuffers();
    void CreateBlueNoiseBuffer();
    void CreateComputePipelines();

    Handle<Texture>                 m_uvs_texture;
    Handle<Texture>                 m_sampled_result_texture;
    
    GPUBufferRef                    m_uniform_buffer;
    
    ComputePipelineRef              m_write_uvs;
    ComputePipelineRef              m_sample_gbuffer;

    UniquePtr<TemporalBlending>     m_temporal_blending;

    SSRRendererConfig               m_config;

    bool                            m_is_rendered;
};

} // namespace hyperion

#endif