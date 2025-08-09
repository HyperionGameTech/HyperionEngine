/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/SSRRenderer.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>

#include <rendering/RenderQueue.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/RenderComputePipeline.hpp>

#include <rendering/Texture.hpp>

#include <scene/View.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/threading/Threads.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

static constexpr bool useTemporalBlending = true;

static constexpr TextureFormat ssrFormat = TF_RGBA8;

struct SSRUniforms
{
    Vec4u dimensions;
    float rayStep,
        numIterations,
        maxRayDistance,
        distanceBias,
        offset,
        eyeFadeStart,
        eyeFadeEnd,
        screenEdgeFadeStart,
        screenEdgeFadeEnd;
};

#pragma region Render commands

struct RENDER_COMMAND(CreateSSRUniformBuffer)
    : RenderCommand
{
    SSRUniforms uniforms;
    GpuBufferRef uniformBuffer;

    RENDER_COMMAND(CreateSSRUniformBuffer)(
        const SSRUniforms& uniforms,
        const GpuBufferRef& uniformBuffer)
        : uniforms(uniforms),
          uniformBuffer(uniformBuffer)
    {
        Assert(uniforms.dimensions.x * uniforms.dimensions.y != 0);

        Assert(this->uniformBuffer != nullptr);
    }

    virtual ~RENDER_COMMAND(CreateSSRUniformBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYP_GFX_CHECK(uniformBuffer->Create());

        uniformBuffer->Copy(sizeof(uniforms), &uniforms);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region SSRRenderer

SSRRenderer::SSRRenderer(
    SSRRendererConfig&& config,
    GBuffer* gbuffer,
    const GpuImageViewRef& mipChainImageView,
    const GpuImageViewRef& deferredResultImageView)
    : m_config(std::move(config)),
      m_gbuffer(gbuffer),
      m_mipChainImageView(mipChainImageView),
      m_deferredResultImageView(deferredResultImageView),
      m_isRendered(false)
{
}

SSRRenderer::~SSRRenderer()
{
    SafeRelease(std::move(m_writeUvs));
    SafeRelease(std::move(m_sampleGbuffer));

    if (m_temporalBlending)
    {
        m_temporalBlending.Reset();
    }

    SafeRelease(std::move(m_uniformBuffer));
}

void SSRRenderer::Create()
{
    m_uvsTexture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        TF_RGBA16F,
        Vec3u(m_config.extent, 1),
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    m_uvsTexture->SetName(NAME("SsrUvs"));
    InitObject(m_uvsTexture);

    m_sampledResultTexture = CreateObject<Texture>(TextureDesc {
        TT_TEX2D,
        ssrFormat,
        Vec3u(m_config.extent, 1),
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    m_sampledResultTexture->SetName(NAME("SsrSampledResult"));
    InitObject(m_sampledResultTexture);

    CreateUniformBuffers();

    if (useTemporalBlending)
    {
        m_temporalBlending = MakeUnique<TemporalBlending>(
            m_config.extent,
            ssrFormat,
            TemporalBlendTechnique::TECHNIQUE_1,
            TemporalBlendFeedback::HIGH,
            g_renderBackend->GetTextureImageView(m_sampledResultTexture),
            m_gbuffer);

        m_temporalBlending->Create();
    }

    CreateComputePipelines();

    // m_onGbufferResolutionChanged = m_gbuffer->OnGBufferResolutionChanged.Bind([this](Vec2u newSize)
    // {
    //     SafeRelease(std::move(m_writeUvs));
    //     SafeRelease(std::move(m_sampleGbuffer));

    //     CreateComputePipelines();
    // });
}

const Handle<Texture>& SSRRenderer::GetFinalResultTexture() const
{
    return m_temporalBlending
        ? m_temporalBlending->GetResultTexture()
        : m_sampledResultTexture;
}

ShaderProperties SSRRenderer::GetShaderProperties() const
{
    ShaderProperties shaderProperties;
    shaderProperties.Set(NAME("CONE_TRACING"), m_config.coneTracing);
    shaderProperties.Set(NAME("ROUGHNESS_SCATTERING"), m_config.roughnessScattering);

    switch (ssrFormat)
    {
    case TF_RGBA8:
        shaderProperties.Set(NAME("OUTPUT_RGBA8"));
        break;
    case TF_RGBA16F:
        shaderProperties.Set(NAME("OUTPUT_RGBA16F"));
        break;
    case TF_RGBA32F:
        shaderProperties.Set(NAME("OUTPUT_RGBA32F"));
        break;
    default:
        HYP_FAIL("Invalid SSR format type");
    }

    return shaderProperties;
}

void SSRRenderer::CreateUniformBuffers()
{
    SSRUniforms uniforms {};
    uniforms.dimensions = Vec4u(m_config.extent, 0, 0);
    uniforms.rayStep = m_config.rayStep;
    uniforms.numIterations = m_config.numIterations;
    uniforms.maxRayDistance = 1000.0f;
    uniforms.distanceBias = 0.02f;
    uniforms.offset = 0.25f;
    uniforms.eyeFadeStart = m_config.eyeFade.x;
    uniforms.eyeFadeEnd = m_config.eyeFade.y;
    uniforms.screenEdgeFadeStart = m_config.screenEdgeFade.x;
    uniforms.screenEdgeFadeEnd = m_config.screenEdgeFade.y;

    m_uniformBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(uniforms));

    PUSH_RENDER_COMMAND(CreateSSRUniformBuffer, uniforms, m_uniformBuffer);
}

void SSRRenderer::CreateComputePipelines()
{
    const ShaderProperties shaderProperties = GetShaderProperties();

    // Write UVs pass

    ShaderRef writeUvsShader = g_shaderManager->GetOrCreate(NAME("SSRWriteUVs"), shaderProperties);
    Assert(writeUvsShader.IsValid());

    const DescriptorTableDeclaration& writeUvsShaderDescriptorTableDecl = writeUvsShader->GetCompiledShader()->GetDescriptorTableDeclaration();
    DescriptorTableRef writeUvsShaderDescriptorTable = g_renderBackend->MakeDescriptorTable(&writeUvsShaderDescriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = writeUvsShaderDescriptorTable->GetDescriptorSet(NAME("SSRDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("UVImage"), g_renderBackend->GetTextureImageView(m_uvsTexture));
        descriptorSet->SetElement(NAME("UniformBuffer"), m_uniformBuffer);

        descriptorSet->SetElement(NAME("GBufferNormalsTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_NORMALS)->GetImageView());
        descriptorSet->SetElement(NAME("GBufferMaterialTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_MATERIAL)->GetImageView());
        descriptorSet->SetElement(NAME("GBufferVelocityTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());
        descriptorSet->SetElement(NAME("GBufferDepthTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_DEPTH)->GetImageView());
        descriptorSet->SetElement(NAME("GBufferMipChain"), m_mipChainImageView ? m_mipChainImageView : g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        descriptorSet->SetElement(NAME("DeferredResult"), m_deferredResultImageView ? m_deferredResultImageView : g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
    }

    DeferCreate(writeUvsShaderDescriptorTable);

    m_writeUvs = g_renderBackend->MakeComputePipeline(
        g_shaderManager->GetOrCreate(NAME("SSRWriteUVs"), shaderProperties),
        writeUvsShaderDescriptorTable);

    DeferCreate(m_writeUvs);

    // Sample pass

    ShaderRef sampleGbufferShader = g_shaderManager->GetOrCreate(NAME("SSRSampleGBuffer"), shaderProperties);
    Assert(sampleGbufferShader.IsValid());

    const DescriptorTableDeclaration& sampleGbufferShaderDescriptorTableDecl = sampleGbufferShader->GetCompiledShader()->GetDescriptorTableDeclaration();
    DescriptorTableRef sampleGbufferShaderDescriptorTable = g_renderBackend->MakeDescriptorTable(&sampleGbufferShaderDescriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = sampleGbufferShaderDescriptorTable->GetDescriptorSet(NAME("SSRDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("UVImage"), g_renderBackend->GetTextureImageView(m_uvsTexture));
        descriptorSet->SetElement(NAME("SampleImage"), g_renderBackend->GetTextureImageView(m_sampledResultTexture));
        descriptorSet->SetElement(NAME("UniformBuffer"), m_uniformBuffer);

        descriptorSet->SetElement(NAME("GBufferNormalsTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_NORMALS)->GetImageView());
        descriptorSet->SetElement(NAME("GBufferMaterialTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_MATERIAL)->GetImageView());
        descriptorSet->SetElement(NAME("GBufferVelocityTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());
        descriptorSet->SetElement(NAME("GBufferDepthTexture"), m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_DEPTH)->GetImageView());
        descriptorSet->SetElement(NAME("GBufferMipChain"), m_mipChainImageView ? m_mipChainImageView : g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        descriptorSet->SetElement(NAME("DeferredResult"), m_deferredResultImageView ? m_deferredResultImageView : g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
    }

    DeferCreate(sampleGbufferShaderDescriptorTable);

    m_sampleGbuffer = g_renderBackend->MakeComputePipeline(
        sampleGbufferShader,
        sampleGbufferShaderDescriptorTable);

    DeferCreate(m_sampleGbuffer);
}

void SSRRenderer::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_NAMED_SCOPE("Screen Space Reflections");

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    const uint32 frameIndex = frame->GetFrameIndex();

    /* ========== BEGIN SSR ========== */
    const uint32 totalPixelsInImage = m_config.extent.Volume();

    const uint32 numDispatchCalls = (totalPixelsInImage + 255) / 256;

    { // PASS 1 -- write UVs

        frame->renderQueue << InsertBarrier(m_uvsTexture->GetGpuImage(), RS_UNORDERED_ACCESS);

        frame->renderQueue << BindComputePipeline(m_writeUvs);

        frame->renderQueue << BindDescriptorTable(
            m_writeUvs->GetDescriptorTable(),
            m_writeUvs,
            { { NAME("Global"),
                { { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) } } } },
            frameIndex);

        const uint32 viewDescriptorSetIndex = m_writeUvs->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

        if (viewDescriptorSetIndex != ~0u)
        {
            Assert(renderSetup.passData != nullptr);

            frame->renderQueue << BindDescriptorSet(
                renderSetup.passData->descriptorSets[frame->GetFrameIndex()],
                m_writeUvs,
                {},
                viewDescriptorSetIndex);
        }

        frame->renderQueue << DispatchCompute(m_writeUvs, Vec3u { numDispatchCalls, 1, 1 });

        // transition the UV image back into read state
        frame->renderQueue << InsertBarrier(m_uvsTexture->GetGpuImage(), RS_SHADER_RESOURCE);
    }

    { // PASS 2 - sample textures
        // put sample image in writeable state
        frame->renderQueue << InsertBarrier(m_sampledResultTexture->GetGpuImage(), RS_UNORDERED_ACCESS);

        frame->renderQueue << BindComputePipeline(m_sampleGbuffer);

        frame->renderQueue << BindDescriptorTable(
            m_sampleGbuffer->GetDescriptorTable(),
            m_sampleGbuffer,
            { { NAME("Global"),
                { { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) } } } },
            frameIndex);

        const uint32 viewDescriptorSetIndex = m_sampleGbuffer->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

        if (viewDescriptorSetIndex != ~0u)
        {
            Assert(renderSetup.passData != nullptr);

            frame->renderQueue << BindDescriptorSet(
                renderSetup.passData->descriptorSets[frame->GetFrameIndex()],
                m_sampleGbuffer,
                {},
                viewDescriptorSetIndex);
        }

        frame->renderQueue << DispatchCompute(m_sampleGbuffer, Vec3u { numDispatchCalls, 1, 1 });

        // transition sample image back into read state
        frame->renderQueue << InsertBarrier(m_sampledResultTexture->GetGpuImage(), RS_SHADER_RESOURCE);
    }

    if (useTemporalBlending && m_temporalBlending != nullptr)
    {
        m_temporalBlending->Render(frame, renderSetup);
    }

    m_isRendered = true;
    /* ==========  END SSR  ========== */
}

#pragma endregion SSRRenderer

} // namespace hyperion
