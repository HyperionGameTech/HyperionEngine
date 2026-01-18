/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/SSGI.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/Frame.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Shader.hpp>
#include <rendering/TextureViewCache.hpp>

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

#pragma region Render commands

struct CreateSSGIUniformBuffers : RenderCommand
{
    SSGIUniforms uniforms;
    FixedArray<GpuBufferRef, NumFramesInFlight> uniformBuffers;

    CreateSSGIUniformBuffers(
        const SSGIUniforms& uniforms,
        const FixedArray<GpuBufferRef, NumFramesInFlight>& uniformBuffers)
        : uniforms(uniforms),
          uniformBuffers(uniformBuffers)
    {
        Assert(uniforms.dimensions.x * uniforms.dimensions.y != 0);
    }

    virtual ~CreateSSGIUniformBuffers() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            Assert(uniformBuffers[frameIndex] != nullptr);

            CheckResultOrReturn(uniformBuffers[frameIndex]->Create());

            uniformBuffers[frameIndex]->Copy(sizeof(uniforms), &uniforms);
        }

        return {};
    }
};

#pragma endregion Render commands

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
    SafeDelete(std::move(m_computePipeline));
}

void SSGI::Create()
{
    m_resultTexture = CreateObject<Texture>(TextureDesc {
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

    CreateComputePipelines();
}

const Handle<Texture>& SSGI::GetFinalResultTexture() const
{
    return m_temporalBlending
        ? m_temporalBlending->GetResultTexture()
        : m_resultTexture;
}

ShaderProperties SSGI::GetShaderProperties() const
{
    ShaderProperties shaderProperties;

    switch (SsgiFormat)
    {
    case TF_RGBA8:
        shaderProperties.Set(ShaderProperty(NAME("OUTPUT"), NAME("RGBA8")));
        break;
    case TF_RGBA16F:
        shaderProperties.Set(ShaderProperty(NAME("OUTPUT"), NAME("RGBA16F")));
        break;
    case TF_RGBA32F:
        shaderProperties.Set(ShaderProperty(NAME("OUTPUT"), NAME("RGBA32F")));
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
        m_uniformBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(uniforms));
        m_uniformBuffers[frameIndex]->SetDebugName(NAME_FMT("SSGI_UniformBuffer_Frame{}", frameIndex));
    }

    PUSH_RENDER_COMMAND(CreateSSGIUniformBuffers, uniforms, m_uniformBuffers);
}

void SSGI::CreateComputePipelines()
{
    const ShaderProperties shaderProperties = GetShaderProperties();

    ShaderRef shader = g_shaderManager->GetOrCreate(NAME("SSGI"), shaderProperties);
    Assert(shader.IsValid());

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(
        shader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("SSGIDescriptorSet"_sh, frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("OutImage"_sh, g_renderInterface->textureViewCache->GetOrCreate(m_resultTexture));
        descriptorSet->SetElement("UniformBuffer"_sh, m_uniformBuffers[frameIndex]);
    }

    DeferCreate(descriptorTable);

    m_computePipeline = g_renderBackend->MakeComputePipeline(
        shader,
        descriptorTable);

    DeferCreate(m_computePipeline);
}

void SSGI::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_NAMED_SCOPE("Screen Space Global Illumination");

    AssertDebug(renderSetup.world && renderSetup.view);

    const uint32 frameIndex = frame->GetFrameIndex();

    // Update uniform buffer data
    SSGIUniforms uniforms;
    FillUniformBufferData(renderSetup.view, uniforms);
    m_uniformBuffers[frame->GetFrameIndex()]->Copy(sizeof(uniforms), &uniforms);

    const uint32 totalPixelsInImage = m_config.extent.Volume();
    const uint32 numDispatchCalls = (totalPixelsInImage + 255) / 256;

    // put sample image in writeable state
    frame->renderQueue << InsertBarrier(m_resultTexture->GetGpuImage(), RS_UNORDERED_ACCESS);

    frame->renderQueue << BindComputePipeline(m_computePipeline);

    frame->renderQueue << BindDescriptorTable(
        m_computePipeline->GetDescriptorTable(),
        m_computePipeline,
        { { "Global"_sh,
            { { "CamerasBuffer"_sh, ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) },
                { "CurrentEnvProbe"_sh, ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frameIndex);

    const uint32 viewDescriptorSetIndex = m_computePipeline->GetDescriptorTable()->GetDescriptorSetIndex("View"_sh);

    if (viewDescriptorSetIndex != ~0u)
    {
        Assert(renderSetup.passData != nullptr);

        frame->renderQueue << BindDescriptorSet(
            renderSetup.passData->descriptorSets[frame->GetFrameIndex()],
            m_computePipeline,
            {},
            viewDescriptorSetIndex);
    }

    frame->renderQueue << DispatchCompute(m_computePipeline, Vec3u { numDispatchCalls, 1, 1 });

    // transition sample image back into read state
    frame->renderQueue << InsertBarrier(m_resultTexture->GetGpuImage(), RS_SHADER_RESOURCE);

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
        RenderProxyList& rpl = RenderApi::GetConsumerProxyList(view);
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

            outUniforms.lightIndices[numBoundLights++] = RenderApi::RetrieveResourceBinding(light);
        }
    }

    outUniforms.numBoundLights = numBoundLights;
}

#pragma endregion SSGI

} // namespace Hyperion
