/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/SSRPass.hpp>
#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/Pass.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/FullScreenPass.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/ShaderInstance.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/DepthPyramidRenderer.hpp>
#include <Rendering/StencilMasks.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Framework/CVarManager.hpp>
#include <Framework/EngineStats.hpp>

#include <Scene/View.hpp>
#include <Scene/EnvProbe.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion {

static constexpr bool UseTemporalBlending = false;
static constexpr TextureFormat SSRColorFormat = TextureFormat::R10G10B10A2;
static constexpr TextureFormat SSRTraceFormat = TextureFormat::R32; // packed

static EngineStatGpuTimer s_statSSRTracePass("Rendering/GPU/SSRTrace");
static EngineStatGpuTimer s_statSSRColorPass("Rendering/GPU/SSRColor");

CVar<bool> cvSSRConeTracing { "Rendering.SSR.ConeTracing", true };
CVar<bool> cvSSRRoughnessScattering { "Rendering.SSR.RoughnessScattering", false };

CVar<float> cvSSRRayStep { "Rendering.SSR.RayStep", 0.08f };
CVar<float> cvSSRDistanceBias { "Rendering.SSR.DistanceBias", 0.025f };
CVar<float> cvSSRThickness { "Rendering.SSR.Thickness", 0.2f };
CVar<float> cvSSRMaxDistance { "Rendering.SSR.MaxDistance", 1000.0f };
CVar<float> cvSSRTraceResolutionScale { "Rendering.SSR.TraceResolutionScale", 0.65 };
CVar<float> cvSSRScreenEdgeFadeStart { "Rendering.SSR.ScreenEdgeFadeStart", 0.92f };
CVar<float> cvSSRScreenEdgeFadeEnd { "Rendering.SSR.ScreenEdgeFadeEnd", 0.98f };
CVar<uint32> cvSSRMaxIterations { "Rendering.SSR.MaxIterations", 256 };

struct SSRConstants
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
    float thickness;
};

#pragma region SSRPass

SSRPass::SSRPass(Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TextureFormat::R8, extent, gbuffer, FSP_EXTERNAL_RENDERTARGET),
      m_tracePass(nullptr),
      m_samplePass(nullptr),
      m_isRendered(false)
{
    SetPassName(NAME("SSR"));
}

SSRPass::~SSRPass()
{
    delete m_tracePass;
    delete m_samplePass;

    EnqueueDeletion(std::move(m_uvsTexture));
    EnqueueDeletion(std::move(m_sampledResultTexture));

    if (m_temporalBlending)
    {
        m_temporalBlending.Reset();
    }
}

void SSRPass::Create()
{
}

Texture* SSRPass::GetFinalResultTexture() const
{
    return m_temporalBlending
        ? m_temporalBlending->GetResultTexture()
        : m_sampledResultTexture;
}

ShaderPropertySet SSRPass::GetShaderProperties() const
{
    ShaderPropertySet shaderProperties;
    shaderProperties.Set(InternShaderProperty(ShaderProperty(NAME("CONE_TRACING"))), cvSSRConeTracing.Get());
    shaderProperties.Set(InternShaderProperty(ShaderProperty(NAME("ROUGHNESS_SCATTERING"))), cvSSRRoughnessScattering.Get());

    return shaderProperties;
}

void SSRPass::CreatePasses()
{
    const ShaderPropertySet shaderProperties = GetShaderProperties();

    // Write UVs pass - renders to m_uvsTexture
    {
        // Create framebuffer for UVs texture
        FramebufferDesc framebufferDesc {};
        framebufferDesc.extent = m_uvsTexture->GetExtent().GetXY();
        framebufferDesc.numLayers = 1;

        FramebufferRef writeUVsFramebuffer = RI.MakeFramebuffer(framebufferDesc);

#ifdef HYP_RHI_DEBUG_NAMES
        writeUVsFramebuffer->SetDebugName(NAME("SSRWriteUVsFramebuffer"));
#endif

        AttachmentDesc colorAttachmentDesc {};
        colorAttachmentDesc.imageType = TextureType::Texture2D;
        colorAttachmentDesc.format = m_uvsTexture->GetFormat();
        colorAttachmentDesc.loadOp = LoadOperation::CLEAR;
        colorAttachmentDesc.storeOp = StoreOperation::STORE;

        writeUVsFramebuffer->AddAttachment(
            0,
            colorAttachmentDesc,
            RI.MakeImageView(m_uvsTexture->GetGpuImage()));

        // depth for stencil testing - so we don't render against sky
        const GpuImageViewRef& depthImageView = m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Depth)->GetImageView();
        Assert(depthImageView.IsValid());

        AttachmentDesc depthAttachmentDesc {};
        depthAttachmentDesc.imageType = TextureType::Texture2D;
        depthAttachmentDesc.format = depthImageView->GetImage()->GetTextureFormat();
        depthAttachmentDesc.loadOp = LoadOperation::LOAD;
        depthAttachmentDesc.storeOp = StoreOperation::NONE;
        depthAttachmentDesc.onlyStencil = true;

        writeUVsFramebuffer->AddAttachment(
            1,
            depthAttachmentDesc,
            depthImageView);

        Check(writeUVsFramebuffer->Create());

        delete m_tracePass;

        m_tracePass = new FullScreenPass(
            ShaderDesc(NAME("SSRWriteUVs"), shaderProperties),
            writeUVsFramebuffer,
            m_uvsTexture->GetFormat(),
            m_uvsTexture->GetExtent().GetXY(),
            nullptr);

        m_tracePass->Create();
    }

    // Sample pass - renders to m_sampledResultTexture
    {
        // Create framebuffer for sampled result texture
        FramebufferDesc framebufferDesc {};
        framebufferDesc.extent = m_sampledResultTexture->GetExtent().GetXY();
        framebufferDesc.numLayers = 1;

        FramebufferRef sampleGBufferFramebuffer = RI.MakeFramebuffer(framebufferDesc);

#ifdef HYP_RHI_DEBUG_NAMES
        sampleGBufferFramebuffer->SetDebugName(NAME("SSRSampleGBufferFramebuffer"));
#endif

        AttachmentDesc attachmentDesc {};
        attachmentDesc.imageType = TextureType::Texture2D;
        attachmentDesc.format = m_sampledResultTexture->GetFormat();
        attachmentDesc.loadOp = LoadOperation::CLEAR;
        attachmentDesc.storeOp = StoreOperation::STORE;

        sampleGBufferFramebuffer->AddAttachment(
            0,
            attachmentDesc,
            RI.MakeImageView(m_sampledResultTexture->GetGpuImage()));

        // depth for stencil testing - so we don't render against sky
        const GpuImageViewRef& depthImageView = m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Depth)->GetImageView();
        Assert(depthImageView.IsValid());

        AttachmentDesc depthAttachmentDesc {};
        depthAttachmentDesc.imageType = TextureType::Texture2D;
        depthAttachmentDesc.format = depthImageView->GetImage()->GetTextureFormat();
        depthAttachmentDesc.loadOp = LoadOperation::LOAD;
        depthAttachmentDesc.storeOp = StoreOperation::NONE;
        depthAttachmentDesc.onlyStencil = true;

        sampleGBufferFramebuffer->AddAttachment(
            1,
            depthAttachmentDesc,
            depthImageView);

        Check(sampleGBufferFramebuffer->Create());

        delete m_samplePass;

        m_samplePass = new FullScreenPass(
            ShaderDesc(NAME("SSRSampleGBuffer"), shaderProperties),
            sampleGBufferFramebuffer,
            SSRColorFormat,
            m_sampledResultTexture->GetExtent().GetXY(),
            nullptr);

        m_samplePass->Create();
    }
}

void SSRPass::UpdatePipelineState(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;

    // Check if we need to recreate textures and passes
    const bool needsRecreate = !m_tracePass
        || !m_samplePass
        || !m_uvsTexture
        || !m_sampledResultTexture;

    if (needsRecreate)
    {
        // Clean up old resources
        delete m_tracePass;
        m_tracePass = nullptr;

        delete m_samplePass;
        m_samplePass = nullptr;

        if (m_temporalBlending)
        {
            m_temporalBlending.Reset();
        }

        // Create textures
        m_uvsTexture = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            SSRTraceFormat,
            Vec3u(MathUtil::Max(Vec2u::One(), m_extent), 1),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_ATTACHMENT | IU_SAMPLED
        });

        m_uvsTexture->SetName(NAME("SSRTexture_UVs"));
        Check(m_uvsTexture->Create());

        m_sampledResultTexture = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            SSRColorFormat,
            Vec3u(m_extent, 1),
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_ATTACHMENT | IU_SAMPLED
        });

        m_sampledResultTexture->SetName(NAME("SSRTexture_SampledResult"));
        Check(m_sampledResultTexture->Create());

        // Create temporal blending
        if (UseTemporalBlending)
        {
            m_temporalBlending = MakeUnique<TemporalBlending>(
                m_extent,
                TextureFormat::RGBA8,
                TemporalBlendTechnique::TECHNIQUE_1,
                0.95,
                RI.textureViewCache->GetOrCreate(m_sampledResultTexture),
                m_gbuffer);

            m_temporalBlending->Create();
        }

        // Create passes
        CreatePasses();
    }
}

void SSRPass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    AssertDebug(renderSetup.world && renderSetup.view);

    UpdatePipelineState(frame, renderSetup);

    CommandRecorder& cr = frame->cr;
    
    static constexpr uint8 StencilFilterMask = SkyStencilMask;

    cr << SetStencilTest(true);
    cr << SetStencilFunction(StencilFunction { SO_KEEP, SO_KEEP, SO_KEEP, SCO_EQUAL });
    cr << SetStencilState(0, StencilFilterMask, 0x0);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    { // Update cbuffer data
        SSRConstants* constants = RI.cbufferAllocator->Allocate<SSRConstants>();
        constants->dimensions = Vec4u(m_extent, 0, 0);
        constants->rayStep = cvSSRRayStep.Get();
        constants->numIterations = cvSSRMaxIterations.Get();
        constants->maxRayDistance = cvSSRMaxDistance.Get();
        constants->distanceBias = cvSSRDistanceBias.Get();
        constants->thickness = cvSSRThickness.Get();
        constants->offset = 0.25f;
        constants->eyeFadeStart = 0.98f;
        constants->eyeFadeEnd = 0.99f;
        constants->screenEdgeFadeStart = cvSSRScreenEdgeFadeStart.Get();
        constants->screenEdgeFadeEnd = cvSSRScreenEdgeFadeEnd.Get();

        // Write camera shader data 
        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
        AssertDebug(cameraProxy != nullptr);

        CameraShaderData* cameraShaderData = RI.cbufferAllocator->Allocate<CameraShaderData>();
        *cameraShaderData = cameraProxy->bufferData;

        const uint32 frameCounter = GetFrameCounter();
        RI.cbufferAllocator->Write(&frameCounter);

        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);
    }
    
    DeferredPassData* dpd = DynamicCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    GpuImageView* hiZTextureView = RI.textureViewCache->GetOrCreate(dpd->depthPyramidRenderer->GetHZBTexture());
    
    const bool useMipChain = cvSSRConeTracing.Get();

    GpuImageView* sampleSourceTargetView = useMipChain && dpd->mipChain.IsValid()
        ? RI.textureViewCache->GetOrCreate(dpd->mipChain)
        : m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Color)->GetImageView();

    { // PASS 1 -- write UVs
        ENGINE_STAT_GPU_SCOPE(&s_statSSRTracePass);

        m_tracePass->Begin(frame, renderSetup);

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Normals)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferMaterialTexture"_sh, m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::MatData)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferVelocityTexture"_sh, m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Velocity)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "HiZTexture"_sh, hiZTextureView);
        cr << SetShaderUniform(uniformIndex++, "GBufferMipChain"_sh, sampleSourceTargetView);
        cr << SetShaderUniform(uniformIndex++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
        cr << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(uniformIndex++, "BlueNoiseBuffer"_sh, RI.blueNoiseBuffer);

        m_tracePass->RenderFullScreenQuad(frame, renderSetup);
        m_tracePass->End(frame, renderSetup);

        cr << InsertBarrier(m_uvsTexture->GetGpuImage(), RS_SHADER_RESOURCE);
    }

    { // PASS 2 -- fill color buffer using mip chain to sample based on roughness
        ENGINE_STAT_GPU_SCOPE(&s_statSSRColorPass);

        m_samplePass->Begin(frame, renderSetup);

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Normals)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferMaterialTexture"_sh, m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::MatData)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferVelocityTexture"_sh, m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Velocity)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferMipChain"_sh, sampleSourceTargetView);
        cr << SetShaderUniform(uniformIndex++, "HiZTexture"_sh, hiZTextureView);
        cr << SetShaderUniform(uniformIndex++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
        cr << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(uniformIndex++, "BlueNoiseBuffer"_sh, RI.blueNoiseBuffer);
        cr << SetShaderUniform(uniformIndex++, "UVImage"_sh, RI.textureViewCache->GetOrCreate(m_uvsTexture));

        m_samplePass->RenderFullScreenQuad(frame, renderSetup);
        m_samplePass->End(frame, renderSetup);
    }

    cr << SetStencilTest(false);

    if (UseTemporalBlending && m_temporalBlending != nullptr)
    {
        m_temporalBlending->Render(frame, renderSetup);
    }

    m_isRendered = true;
}

#pragma endregion SSRPass

} // namespace Hyperion
