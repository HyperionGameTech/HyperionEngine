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

HYP_STRUCT(ConfigName = "app", JsonPath = "rendering.ssr")
struct SSRRendererConfig : public ConfigBase<SSRRendererConfig>
{
    HYP_FIELD()
    bool enabled = true;

    HYP_FIELD(Description = "The quality level of the SSR effect. (0 = low, 1 = medium, 2 = high)")
    int quality = 2;

    HYP_FIELD(Description = "Enables scattering of rays based on the roughness of the surface. May cause artifacts due to temporal instability.")
    bool roughnessScattering = true;

    HYP_FIELD(Description = "Enables cone tracing for the SSR effect. Causes the result to become blurrier based on distance of the reflection.")
    bool coneTracing = false;

    HYP_FIELD(Description = "The distance between rays when tracing the SSR effect.")
    float rayStep = 3.2f;

    HYP_FIELD(Description = "The maximum number of iterations to perform for the SSR effect before stopping.")
    uint32 numIterations = 64;

    HYP_FIELD(Description = "Where to start and end fading the SSR effect based on the eye vector.")
    Vec2f eyeFade = { 0.98f, 0.99f };

    HYP_FIELD(Description = "Where to start and end fading the SSR effect based on the screen edges.")
    Vec2f screenEdgeFade = { 0.96f, 0.99f };

    HYP_FIELD(JsonIgnore)
    Vec2u extent;

    virtual ~SSRRendererConfig() override = default;

    bool Validate() const
    {
        return extent.x * extent.y != 0
            && rayStep > 0.0f
            && numIterations > 0;
    }

    void PostLoadCallback()
    {
        extent = Vec2u { 1280, 720 };

        switch (quality)
        {
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
    SSRRenderer(
        SSRRendererConfig&& config,
        GBuffer* gbuffer,
        const ImageViewRef& mipChainImageView,
        const ImageViewRef& deferredResultImageView);
    ~SSRRenderer();

    HYP_FORCE_INLINE const Handle<Texture>& GetUVsTexture() const
    {
        return m_uvsTexture;
    }

    HYP_FORCE_INLINE const Handle<Texture>& GetSampledResultTexture() const
    {
        return m_sampledResultTexture;
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

    SSRRendererConfig m_config;

    GBuffer* m_gbuffer;

    ImageViewRef m_mipChainImageView;
    ImageViewRef m_deferredResultImageView;

    Handle<Texture> m_uvsTexture;
    Handle<Texture> m_sampledResultTexture;

    GpuBufferRef m_uniformBuffer;

    ComputePipelineRef m_writeUvs;
    ComputePipelineRef m_sampleGbuffer;

    UniquePtr<TemporalBlending> m_temporalBlending;

    DelegateHandler m_onGbufferResolutionChanged;

    bool m_isRendered;
};

} // namespace hyperion
