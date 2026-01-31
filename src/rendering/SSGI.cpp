/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/SSGI.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/Frame.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Shader.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/shadows/ShadowMapAllocator.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/threading/Threads.hpp>

#include <rendering/Texture.hpp>

#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <scene/View.hpp>

#include <SSGI.generated.inl>

namespace Hyperion {

static constexpr bool UseTemporalBlending = true;
static constexpr TextureFormat SsgiFormat = TF_RGBA8;

struct SSGIUniforms
{
    Vec4u dimensions;
    float rayStep;
    float numIterations;
    float maxRayDistance;
    float distanceBias;
    float offset;
    float eyeFadeStart;
    float eyeFadeEnd;
    float screenEdgeFadeStart;
    float screenEdgeFadeEnd;

    uint32 numBoundLights;
    alignas(16) uint32 lightIndices[16];
};

#pragma region SSGI

SSGI::SSGI(SSGIConfig&& config, GBuffer* gbuffer)
    : m_config(std::move(config)),
      m_gbuffer(gbuffer),
      m_isRendered(false)
{
}

SSGI::~SSGI()
{
    if (m_temporalBlending)
    {
        m_temporalBlending.Reset();
    }

    SafeDelete(std::move(m_uniformBuffers));
}

void SSGI::Create()
{
    m_resultTexture = MakeHandle<Texture>(TextureDesc {
        TT_TEX2D,
        SsgiFormat,
        Vec3u(m_config.extent, 1),
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    m_resultTexture->SetName(NAME("SSGITexture"));

    InitObject(m_resultTexture);

    CreateUniformBuffers();

    if (UseTemporalBlending)
    {
        m_temporalBlending = MakeUnique<TemporalBlending>(
            m_config.extent,
            SsgiFormat,
            TemporalBlendTechnique::TECHNIQUE_1,
            0.96,
            g_renderInterface->textureViewCache->GetOrCreate(m_resultTexture),
            m_gbuffer);

        m_temporalBlending->Create();
    }
}

const Handle<Texture>& SSGI::GetFinalResultTexture() const
{
    return m_temporalBlending
        ? m_temporalBlending->GetResultTexture()
        : m_resultTexture;
}

ShaderPropertySet SSGI::GetShaderProperties() const
{
    ShaderPropertySet shaderProperties;

    switch (SsgiFormat)
    {
    case TF_RGBA8:
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("OUTPUT"), NAME("RGBA8"))));
        break;
    case TF_RGBA16F:
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("OUTPUT"), NAME("RGBA16F"))));
        break;
    case TF_RGBA32F:
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("OUTPUT"), NAME("RGBA32F"))));
        break;
    default:
        HYP_FAIL("Invalid SSGI format type");
    }

    return shaderProperties;
}

void SSGI::CreateUniformBuffers()
{
    SSGIUniforms uniforms;
    FillUniformBufferData(nullptr, uniforms);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        m_uniformBuffers[frameIndex] = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(uniforms));
        m_uniformBuffers[frameIndex]->SetDebugName(NAME_FMT("SSGI_UniformBuffer_Frame{}", frameIndex));

        CheckResult(m_uniformBuffers[frameIndex]->Create());

        m_uniformBuffers[frameIndex]->Copy(sizeof(uniforms), &uniforms);
    }
}

void SSGI::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_NAMED_SCOPE("Screen Space Global Illumination");

    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    DeferredRendererPassData* dpd = ObjCast<DeferredRendererPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    const FramebufferRef& inputsFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RB_OPAQUE);

    // Update uniform buffer data
    SSGIUniforms uniforms;
    FillUniformBufferData(renderSetup.view, uniforms);
    m_uniformBuffers[frameIndex]->Copy(sizeof(uniforms), &uniforms);

    const uint32 totalPixelsInImage = m_config.extent.Volume();
    const uint32 numDispatchCalls = (totalPixelsInImage + 255) / 256;

    RenderQueue& rq = frame->renderQueue;

    // put sample image in writeable state
    rq << InsertBarrier(m_resultTexture->GetGpuImage(), RS_UNORDERED_ACCESS);

    rq << SetCurrentShader(ShaderDesc(NAME("SSGI"), GetShaderProperties()));

    uint32 numShaderUniforms = 0;

    rq << SetShaderUniform(numShaderUniforms++, "OutImage"_sh, g_renderInterface->textureViewCache->GetOrCreate(m_resultTexture));
    rq << SetShaderUniform(numShaderUniforms++, "UniformBuffer"_sh, m_uniformBuffers[frameIndex]);

    // GBuffer textures
    rq << SetShaderUniform(numShaderUniforms++, "GBufferAlbedoTexture"_sh, inputsFramebuffer->GetAttachment(GTN_ALBEDO)->GetImageView());
    rq << SetShaderUniform(numShaderUniforms++, "GBufferNormalsTexture"_sh, inputsFramebuffer->GetAttachment(GTN_NORMALS)->GetImageView());
    rq << SetShaderUniform(numShaderUniforms++, "GBufferMaterialTexture"_sh, inputsFramebuffer->GetAttachment(GTN_MATERIAL)->GetImageView());
    rq << SetShaderUniform(numShaderUniforms++, "GBufferVelocityTexture"_sh, inputsFramebuffer->GetAttachment(GTN_VELOCITY)->GetImageView());
    rq << SetShaderUniform(numShaderUniforms++, "GBufferDepthTexture"_sh, inputsFramebuffer->GetAttachment(GTN_DEPTH)->GetImageView());
    rq << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->mipChain));
    rq << SetShaderUniform(numShaderUniforms++, "DeferredResult"_sh, dpd->combinePass->GetFinalImageView());

    // Samplers
    rq << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    rq << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());

    // Blue noise
    rq << SetShaderUniform(numShaderUniforms++, "BlueNoiseBuffer"_sh, g_renderInterface->blueNoiseBuffer);

    // World and camera buffers
    rq << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));
    rq << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frameIndex), TShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()));

    // Lights
    rq << SetShaderUniform(numShaderUniforms++, "LightsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_LIGHTS]->GetBuffer(frameIndex));

    // Shadow maps
    rq << SetShaderUniform(numShaderUniforms++, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetAtlasImageView());
    rq << SetShaderUniform(numShaderUniforms++, "PointLightShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetPointLightShadowMapImageView());

    // Env probes
    rq << SetShaderUniform(numShaderUniforms++, "EnvProbesTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->envProbesTexture));

    if (renderSetup.envProbe)
        rq << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe));
    else
        rq << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(0));

    rq << SetShaderUniform(numShaderUniforms++, "EnvProbesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex));

    rq << DispatchCompute(Vec3u { numDispatchCalls, 1, 1 });

    // transition sample image back into read state
    rq << InsertBarrier(m_resultTexture->GetGpuImage(), RS_SHADER_RESOURCE);

    if (UseTemporalBlending && m_temporalBlending != nullptr)
    {
        m_temporalBlending->Render(frame, renderSetup);
    }

    m_isRendered = true;
}

void SSGI::FillUniformBufferData(View* view, SSGIUniforms& outUniforms) const
{
    outUniforms = SSGIUniforms();
    outUniforms.dimensions = Vec4u(m_config.extent, 0, 0);
    outUniforms.rayStep = 1.0f;
    outUniforms.numIterations = 16;
    outUniforms.maxRayDistance = 1000.0f;
    outUniforms.distanceBias = 0.1f;
    outUniforms.offset = 0.001f;
    outUniforms.eyeFadeStart = 0.98f;
    outUniforms.eyeFadeEnd = 0.99f;
    outUniforms.screenEdgeFadeStart = 0.98f;
    outUniforms.screenEdgeFadeEnd = 0.99f;

    uint32 numBoundLights = 0;

    // Can only fill the lights if we have a view ready
    if (view)
    {
        RenderProxyList& rpl = GetConsumerProxyList(view);
        rpl.BeginRead();

        HYP_DEFER({ rpl.EndRead(); });

        const uint32 maxBoundLights = ArraySize(outUniforms.lightIndices);

        for (Light* light : rpl.GetLights())
        {
            const LightType lightType = light->GetLightType();

            if (lightType != LT_DIRECTIONAL && lightType != LT_POINT)
            {
                continue;
            }

            if (numBoundLights >= maxBoundLights)
            {
                break;
            }

            outUniforms.lightIndices[numBoundLights++] = RetrieveResourceBinding(light);
        }
    }

    outUniforms.numBoundLights = numBoundLights;
}

#pragma endregion SSGI

} // namespace Hyperion
