/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/SSRRenderer.hpp>
#include <rendering/RendererBase.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/Frame.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderHelpers.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>

#include <scene/camera/Camera.hpp>

#include <Core/threading/Threads.hpp>

#include <SSRRenderer.generated.inl>

namespace Hyperion {

static constexpr bool UseTemporalBlending = true;
static constexpr TextureFormat SSRColorFormat = TextureFormat::R10G10B10A2;
static constexpr TextureFormat SSRTraceFormat = TextureFormat::RGBA16F; // store hit UVs in RG, and mask / alpha in B
static constexpr double TraceResolutionScale = 0.4;

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
        CheckResultOrReturn(uniformBuffer->Create());

        uniformBuffer->Copy(sizeof(uniforms), &uniforms);

        return {};
    }
};

#pragma endregion Render commands

#pragma region SSRRenderer

SSRRenderer::SSRRenderer(
    SSRRendererConfig&& config,
    GBuffer* gbuffer,
    const GpuImageViewRef& mipChainImageView)
    : m_config(std::move(config)),
      m_gbuffer(gbuffer),
      m_mipChainImageView(mipChainImageView),
      m_writeUvs(nullptr),
      m_sampleGbuffer(nullptr),
      m_isRendered(false)
{
}

SSRRenderer::~SSRRenderer()
{
    delete m_writeUvs;
    delete m_sampleGbuffer;

    EnqueueDeletion(std::move(m_uvsTexture));
    EnqueueDeletion(std::move(m_sampledResultTexture));

    if (m_temporalBlending)
    {
        m_temporalBlending.Reset();
    }

    EnqueueDeletion(std::move(m_uniformBuffer));
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

ShaderPropertySet SSRRenderer::GetShaderProperties() const
{
    ShaderPropertySet shaderProperties;
    shaderProperties.Set(InternShaderProperty(ShaderProperty(NAME("CONE_TRACING"))), m_config.coneTracing);
    shaderProperties.Set(InternShaderProperty(ShaderProperty(NAME("ROUGHNESS_SCATTERING"))), m_config.roughnessScattering);

    return shaderProperties;
}

void SSRRenderer::CreatePasses()
{
    const ShaderPropertySet shaderProperties = GetShaderProperties();

    // Write UVs pass - renders to m_uvsTexture
    {
        // Create framebuffer for UVs texture
        RenderTargetDesc renderTargetDesc {};
        renderTargetDesc.extent = m_uvsTexture->GetExtent().GetXY();
        renderTargetDesc.numLayers = 1;

        FramebufferRef writeUvsFramebuffer = g_renderInterface->MakeFramebuffer(renderTargetDesc);
        Attachment* attachment = writeUvsFramebuffer->AddAttachment(
            0,
            m_uvsTexture->GetGpuImage(),
            LoadOperation::CLEAR,
            StoreOperation::STORE);

        CheckResult(writeUvsFramebuffer->Create());

        delete m_writeUvs;

        m_writeUvs = new FullScreenPass(
            ShaderDesc(NAME("SSRWriteUVs"), shaderProperties),
            writeUvsFramebuffer,
            m_uvsTexture->GetFormat(),
            m_uvsTexture->GetExtent().GetXY(),
            nullptr);

        InitObject(m_writeUvs);
        m_writeUvs->Create();
    }

    // Sample pass - renders to m_sampledResultTexture
    {
        // Create framebuffer for sampled result texture
        RenderTargetDesc renderTargetDesc {};
        renderTargetDesc.extent = m_sampledResultTexture->GetExtent().GetXY();
        renderTargetDesc.numLayers = 1;

        FramebufferRef sampleGbufferFramebuffer = g_renderInterface->MakeFramebuffer(renderTargetDesc);
        sampleGbufferFramebuffer->AddAttachment(
            0,
            m_sampledResultTexture->GetGpuImage(),
            LoadOperation::CLEAR,
            StoreOperation::STORE);

        CheckResult(sampleGbufferFramebuffer->Create());

        delete m_sampleGbuffer;

        m_sampleGbuffer = new FullScreenPass(
            ShaderDesc(NAME("SSRSampleGBuffer"), shaderProperties),
            sampleGbufferFramebuffer,
            SSRColorFormat,
            m_sampledResultTexture->GetExtent().GetXY(),
            nullptr);

        m_sampleGbuffer->Create();
    }
}

void SSRRenderer::UpdatePipelineState(Frame* frame, const RenderSetup& renderSetup)
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
        delete m_writeUvs;
        m_writeUvs = nullptr;

        delete m_sampleGbuffer;
        m_sampleGbuffer = nullptr;

        EnqueueDeletion(std::move(m_uniformBuffer));

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

        m_uniformBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(uniforms));
#ifdef HYP_DEBUG_MODE
        m_uniformBuffer->SetDebugName(NAME("SSR_UniformBuffer"));
#endif

        PUSH_RENDER_COMMAND(CreateSSRUniformBuffer, uniforms, m_uniformBuffer);

        // Create textures
        m_uvsTexture = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            SSRTraceFormat,
            Vec3u {
                uint32(MathUtil::Ceil(m_currentExtent.x * TraceResolutionScale)),
                uint32(MathUtil::Ceil(m_currentExtent.y * TraceResolutionScale)),
                1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_ATTACHMENT | IU_SAMPLED });

        m_uvsTexture->SetName(NAME("SSRTexture_UVs"));
        InitObject(m_uvsTexture);

        m_sampledResultTexture = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            SSRColorFormat,
            Vec3u(m_currentExtent, 1),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_ATTACHMENT | IU_SAMPLED
        });

        m_sampledResultTexture->SetName(NAME("SSRTexture_SampledResult"));
        InitObject(m_sampledResultTexture);

        // Create temporal blending
        if (UseTemporalBlending)
        {
            m_temporalBlending = MakeUnique<TemporalBlending>(
                m_currentExtent,
                TextureFormat::RGBA8,
                TemporalBlendTechnique::TECHNIQUE_1,
                0.95,
                g_renderInterface->textureViewCache->GetOrCreate(m_sampledResultTexture),
                m_gbuffer);

            m_temporalBlending->Create();
        }

        // Create passes
        CreatePasses();
    }
}

void SSRRenderer::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_NAMED_SCOPE("Screen Space Reflections");

    AssertDebug(renderSetup.world && renderSetup.view);

    UpdatePipelineState(frame, renderSetup);

    const uint32 frameIndex = frame->GetFrameIndex();
    RenderQueue& rq = frame->renderQueue;

    { // PASS 1 -- write UVs
        m_writeUvs->Begin(frame, renderSetup);

        uint32 uniformIndex = 0;

        rq << SetShaderUniform(uniformIndex++, "UniformBuffer"_sh, m_uniformBuffer);
        rq << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_NORMALS)->GetImageView());
        rq << SetShaderUniform(uniformIndex++, "GBufferMaterialTexture"_sh, m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_MATERIAL)->GetImageView());
        rq << SetShaderUniform(uniformIndex++, "GBufferVelocityTexture"_sh, m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());
        rq << SetShaderUniform(uniformIndex++, "GBufferMipChain"_sh, m_mipChainImageView ? m_mipChainImageView : g_renderInterface->placeholderData->GetImageView2D1x1R8());
        rq << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_DEPTH)->GetImageView());
        rq << SetShaderUniform(uniformIndex++, "DeferredResult"_sh, m_mipChainImageView ? m_mipChainImageView : g_renderInterface->placeholderData->GetImageView2D1x1R8());
        rq << SetShaderUniform(uniformIndex++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
        rq << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
        rq << SetShaderUniform(uniformIndex++, "BlueNoiseBuffer"_sh, g_renderInterface->blueNoiseBuffer);
        rq << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));
        rq << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frameIndex), TShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()));

        m_writeUvs->RenderFullScreenQuad(frame, renderSetup);
        m_writeUvs->End(frame, renderSetup);

        rq << InsertBarrier(m_uvsTexture->GetGpuImage(), RS_SHADER_RESOURCE);
    }

    { // PASS 2 -- fill color buffer using mip chain to sample based on roughness
        m_sampleGbuffer->Begin(frame, renderSetup);

        uint32 uniformIndex = 0;

        rq << SetShaderUniform(uniformIndex++, "UVImage"_sh, g_renderInterface->textureViewCache->GetOrCreate(m_uvsTexture));
        rq << SetShaderUniform(uniformIndex++, "UniformBuffer"_sh, m_uniformBuffer);
        rq << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_NORMALS)->GetImageView());
        rq << SetShaderUniform(uniformIndex++, "GBufferMaterialTexture"_sh, m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_MATERIAL)->GetImageView());
        rq << SetShaderUniform(uniformIndex++, "GBufferVelocityTexture"_sh, m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());
        rq << SetShaderUniform(uniformIndex++, "GBufferMipChain"_sh, m_mipChainImageView ? m_mipChainImageView : g_renderInterface->placeholderData->GetImageView2D1x1R8());
        rq << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_DEPTH)->GetImageView());
        rq << SetShaderUniform(uniformIndex++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
        rq << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
        rq << SetShaderUniform(uniformIndex++, "BlueNoiseBuffer"_sh, g_renderInterface->blueNoiseBuffer);
        rq << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));
        rq << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frameIndex), TShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()));

        m_sampleGbuffer->RenderFullScreenQuad(frame, renderSetup);
        m_sampleGbuffer->End(frame, renderSetup);
    }

    if (UseTemporalBlending && m_temporalBlending != nullptr)
    {
        m_temporalBlending->Render(frame, renderSetup);
    }

    m_isRendered = true;
}

#pragma endregion SSRRenderer

} // namespace Hyperion
