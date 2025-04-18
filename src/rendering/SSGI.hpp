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

HYP_STRUCT(ConfigName="app", ConfigPath="rendering.ssgi")
struct SSGIConfig : public ConfigBase<SSGIConfig>
{
    HYP_FIELD(Description="The quality level of the SSGI effect. (0 = quarter res, 1 = half res)")
    int     quality = 0;

    HYP_FIELD(ConfigIgnore)
    Vec2u   extent;

    virtual ~SSGIConfig() override = default;

    static HYP_API Vec2u GetGBufferResolution();

    void PostLoadCallback()
    {
        extent = GetGBufferResolution() / 2;

        switch (quality) {
        case 0:
            extent /= 2;
            break;
        default:
            break;
        }
    }
};

struct SSGIUniforms;

class SSGI
{
public:
    SSGI(SSGIConfig &&config);
    ~SSGI();

    HYP_FORCE_INLINE const Handle<Texture> &GetResultTexture() const
        { return m_result_texture; }

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

    void GetUniformBufferData(SSGIUniforms &out_uniforms) const;

    Handle<Texture>                                 m_result_texture;
    
    FixedArray<GPUBufferRef, max_frames_in_flight>  m_uniform_buffers;
    
    ComputePipelineRef                              m_compute_pipeline;

    UniquePtr<TemporalBlending>                     m_temporal_blending;

    SSGIConfig                                      m_config;

    bool                                            m_is_rendered;
};

} // namespace hyperion

#endif