/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/SSRRenderer.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/View.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/threading/Threads.hpp>

#include <engine/EngineGlobals.hpp>

#include <SSRRenderer.generated.inl>

namespace hyperion {

static constexpr bool UseTemporalBlending = false;
static constexpr TextureFormat SsrFormat = TF_R10G10B10A2;
static constexpr TextureFormat SsrUVsFormat = TF_R11G11B10F;

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

struct CreateSSRUniformBuffer : RenderCommand
{
    SSRUniforms uniforms;
    GpuBufferRef uniformBuffer;

    CreateSSRUniformBuffer(
        const SSRUniforms& uniforms,
        const GpuBufferRef& uniformBuffer)
        : uniforms(uniforms),
          uniformBuffer(uniformBuffer)
    {
        Assert(uniforms.dimensions.x * uniforms.dimensions.y != 0);

        Assert(this->uniformBuffer != nullptr);
    }

    virtual ~CreateSSRUniformBuffer() override = default;

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
    SafeDelete(std::move(m_writeUvs));
    SafeDelete(std::move(m_sampleGbuffer));
    SafeDelete(std::move(m_uvsTexture));
    SafeDelete(std::move(m_sampledResultTexture));

    if (m_temporalBlending)
    {
        m_temporalBlending.Reset();
    }

    SafeDelete(std::move(m_uniformBuffer));
}

void SSRRenderer::Create()
{
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

    return shaderProperties;
}

void SSRRenderer::CreatePasses()
{
    const ShaderProperties shaderProperties = GetShaderProperties();

    // Write UVs pass - renders to m_uvsTexture
    {
        ShaderRef writeUvsShader = g_shaderManager->GetOrCreate(NAME("SSRWriteUVs"), shaderProperties);
        Assert(writeUvsShader.IsValid());

        DescriptorTableRef writeUvsShaderDescriptorTable = g_renderBackend->MakeDescriptorTable(
            writeUvsShader->GetCompiledShader()->GetDescriptorTableDeclaration());

        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            const DescriptorSetRef& descriptorSet = writeUvsShaderDescriptorTable->GetDescriptorSet("SSRDescriptorSet", frameIndex);
            Assert(descriptorSet != nullptr);

            descriptorSet->SetElement("UniformBuffer", m_uniformBuffer);

            descriptorSet->SetElement("GBufferNormalsTexture", m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_NORMALS)->GetImageView());
            descriptorSet->SetElement("GBufferMaterialTexture", m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_MATERIAL)->GetImageView());
            descriptorSet->SetElement("GBufferVelocityTexture", m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());
            descriptorSet->SetElement("GBufferDepthTexture", m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_DEPTH)->GetImageView());
            descriptorSet->SetElement("GBufferMipChain", m_mipChainImageView ? m_mipChainImageView : g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
            descriptorSet->SetElement("DeferredResult", m_deferredResultImageView ? m_deferredResultImageView : g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }

        DeferCreate(writeUvsShaderDescriptorTable);

        // Create framebuffer for UVs texture
        FramebufferRef writeUvsFramebuffer = g_renderBackend->MakeFramebuffer(m_uvsTexture->GetExtent().GetXY());
        writeUvsFramebuffer->AddAttachment(
            0,
            m_uvsTexture->GetGpuImage(),
            LoadOperation::CLEAR,
            StoreOperation::STORE);

        DeferCreate(writeUvsFramebuffer);

        if (m_writeUvs)
        {
            SafeDelete(std::move(m_writeUvs));
        }

        m_writeUvs = CreateObject<FullScreenPass>(
            writeUvsShader,
            writeUvsShaderDescriptorTable,
            writeUvsFramebuffer,
            m_uvsTexture->GetFormat(),
            m_uvsTexture->GetExtent().GetXY(),
            nullptr);

        InitObject(m_writeUvs);
        m_writeUvs->Create();
    }

    // Sample pass - renders to m_sampledResultTexture
    {
        ShaderRef sampleGbufferShader = g_shaderManager->GetOrCreate(NAME("SSRSampleGBuffer"), shaderProperties);
        Assert(sampleGbufferShader.IsValid());

        DescriptorTableRef sampleGbufferShaderDescriptorTable = g_renderBackend->MakeDescriptorTable(
            sampleGbufferShader->GetCompiledShader()->GetDescriptorTableDeclaration());

        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            const DescriptorSetRef& descriptorSet = sampleGbufferShaderDescriptorTable->GetDescriptorSet("SSRDescriptorSet", frameIndex);
            Assert(descriptorSet != nullptr);

            descriptorSet->SetElement("UVImage", g_renderBackend->GetTextureImageView(m_uvsTexture));
            descriptorSet->SetElement("UniformBuffer", m_uniformBuffer);

            descriptorSet->SetElement("GBufferNormalsTexture", m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_NORMALS)->GetImageView());
            descriptorSet->SetElement("GBufferMaterialTexture", m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_MATERIAL)->GetImageView());
            descriptorSet->SetElement("GBufferVelocityTexture", m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());
            descriptorSet->SetElement("GBufferDepthTexture", m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_DEPTH)->GetImageView());
            descriptorSet->SetElement("GBufferMipChain", m_mipChainImageView ? m_mipChainImageView : g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
            descriptorSet->SetElement("DeferredResult", m_deferredResultImageView ? m_deferredResultImageView : g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }

        DeferCreate(sampleGbufferShaderDescriptorTable);

        // Create framebuffer for sampled result texture
        FramebufferRef sampleGbufferFramebuffer = g_renderBackend->MakeFramebuffer(m_sampledResultTexture->GetExtent().GetXY());
        sampleGbufferFramebuffer->AddAttachment(
            0,
            m_sampledResultTexture->GetGpuImage(),
            LoadOperation::CLEAR,
            StoreOperation::STORE);

        DeferCreate(sampleGbufferFramebuffer);

        if (m_writeUvs)
        {
            SafeDelete(std::move(m_sampleGbuffer));
        }

        m_sampleGbuffer = CreateObject<FullScreenPass>(
            sampleGbufferShader,
            sampleGbufferShaderDescriptorTable,
            sampleGbufferFramebuffer,
            SsrFormat,
            m_sampledResultTexture->GetExtent().GetXY(),
            nullptr);

        InitObject(m_sampleGbuffer);
        m_sampleGbuffer->Create();
    }
}

void SSRRenderer::UpdatePipelineState(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;

    const Vec2u renderTargetExtent = Vec2u(Vec2f(renderSetup.view->GetOutputTarget().GetFramebuffer()->GetExtent()) * m_config.resolutionScale);

    // Check if we need to recreate textures and passes
    const bool needsRecreate = !m_writeUvs
        || !m_sampleGbuffer
        || !m_uvsTexture
        || !m_sampledResultTexture
        || m_currentExtent != renderTargetExtent;

    if (needsRecreate)
    {
        m_currentExtent = renderTargetExtent;

        // Clean up old resources
        SafeDelete(std::move(m_writeUvs));
        SafeDelete(std::move(m_sampleGbuffer));
        SafeDelete(std::move(m_uniformBuffer));

        if (m_temporalBlending)
        {
            m_temporalBlending.Reset();
        }

        // Create uniform buffer with current settings
        SSRUniforms uniforms {};
        uniforms.dimensions = Vec4u(m_currentExtent, 0, 0);
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
        m_uniformBuffer->SetDebugName(NAME("SSR_UniformBuffer"));

        PUSH_RENDER_COMMAND(CreateSSRUniformBuffer, uniforms, m_uniformBuffer);

        // Create textures
        m_uvsTexture = CreateObject<Texture>(TextureDesc {
            TT_TEX2D,
            SsrUVsFormat, // store hit UVs in RG, and mask / alpha in B
            Vec3u(m_currentExtent / 2, 1),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_ATTACHMENT | IU_SAMPLED });

        m_uvsTexture->SetName(NAME("SSRTexture_UVs"));
        InitObject(m_uvsTexture);

        m_sampledResultTexture = CreateObject<Texture>(TextureDesc {
            TT_TEX2D,
            SsrFormat,
            Vec3u(m_currentExtent, 1),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_ATTACHMENT | IU_SAMPLED });

        m_sampledResultTexture->SetName(NAME("SSRTexture_SampledResult"));
        InitObject(m_sampledResultTexture);

        // Create temporal blending
        if (UseTemporalBlending)
        {
            m_temporalBlending = MakeUnique<TemporalBlending>(
                m_currentExtent,
                SsrFormat,
                TemporalBlendTechnique::TECHNIQUE_3,
                0.98,
                g_renderBackend->GetTextureImageView(m_sampledResultTexture),
                m_gbuffer);

            m_temporalBlending->Create();
        }

        // Create passes
        CreatePasses();
    }
}

void SSRRenderer::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_NAMED_SCOPE("Screen Space Reflections");

    AssertDebug(renderSetup.world && renderSetup.view);

    UpdatePipelineState(frame, renderSetup);

    // PASS 1 -- write UVs
    m_writeUvs->Render(frame, renderSetup);

    // // shouldn't need this? renderpass should handle transitions?
    // frame->renderQueue << InsertBarrier(m_uvsTexture->GetGpuImage(), RS_SHADER_RESOURCE);

    // PASS 2 - sample textures
    m_sampleGbuffer->Render(frame, renderSetup);

    if (UseTemporalBlending && m_temporalBlending != nullptr)
    {
        m_temporalBlending->Render(frame, renderSetup);
    }

    m_isRendered = true;
}

#pragma endregion SSRRenderer

} // namespace hyperion
