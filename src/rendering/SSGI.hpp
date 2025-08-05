/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/TemporalBlending.hpp>
#include <rendering/FullScreenPass.hpp>

#include <rendering/RenderObject.hpp>

#include <core/config/Config.hpp>

#include <core/object/HypObject.hpp>

#include <core/utilities/EnumFlags.hpp>

namespace hyperion {

class Engine;
class GBuffer;
class View;
struct SSGIUniforms;

HYP_STRUCT(ConfigName = "app", JsonPath = "rendering.ssgi")
struct SSGIConfig : public ConfigBase<SSGIConfig>
{
    HYP_FIELD(Description = "The quality level of the SSGI effect. (0 = quarter res, 1 = half res)")
    int quality = 0;

    HYP_FIELD(JsonIgnore)
    Vec2u extent;

    virtual ~SSGIConfig() override = default;

    void PostLoadCallback()
    {
        extent = Vec2u { 1280, 720 };

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
        return m_resultTexture;
    }

    const Handle<Texture>& GetFinalResultTexture() const;

    HYP_FORCE_INLINE bool IsRendered() const
    {
        return m_isRendered;
    }

    void Create();

    void Render(FrameBase* frame, const RenderSetup& renderSetup);

private:
    ShaderProperties GetShaderProperties() const;

    void CreateUniformBuffers();
    void CreateBlueNoiseBuffer();
    void CreateComputePipelines();

    void FillUniformBufferData(View* view, SSGIUniforms& outUniforms) const;

    SSGIConfig m_config;

    GBuffer* m_gbuffer;

    Handle<Texture> m_resultTexture;

    FixedArray<GpuBufferRef, g_framesInFlight> m_uniformBuffers;

    ComputePipelineRef m_computePipeline;

    UniquePtr<TemporalBlending> m_temporalBlending;

    bool m_isRendered;
};

} // namespace hyperion
