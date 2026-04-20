/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/passes/SSRPass.hpp>
#include <rendering/RendererBase.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/CommandRecorder.hpp>
#include <rendering/Frame.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/CBufferAllocator.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <engine/CVarManager.hpp>

#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>

#include <scene/camera/Camera.hpp>

#include <Core/threading/Threads.hpp>

namespace Hyperion {

static constexpr bool UseTemporalBlending = true;
static constexpr TextureFormat SSRColorFormat = TextureFormat::R10G10B10A2;
static constexpr TextureFormat SSRTraceFormat = TextureFormat::RGBA16F; // store hit UVs in RG, and mask / alpha in B
static constexpr double TraceResolutionScale = 0.65;

CVar<bool> cvSSRConeTracing { "Rendering.SSR.ConeTracing", true };
CVar<bool> cvSSRRoughnessScattering { "Rendering.SSR.RoughnessScattering", false };

CVar<float> cvSSRResolutionScale { "Rendering.SSR.ResolutionScale", 1.0f };

CVar<float> cvSSRRayStep { "Rendering.SSR.RayStep", 2.0f };
CVar<float> cvSSRDistanceBias { "Rendering.SSR.DistanceBias", 0.01f };
CVar<float> cvSSRMaxDistance { "Rendering.SSR.MaxDistance", 1000.0f };
CVar<uint32> cvSSRMaxIterations { "Rendering.SSR.MaxIterations", 256 };

struct SSRConstants
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

#pragma region SSRPass

SSRPass::SSRPass(GBuffer* gbuffer, const GpuImageViewRef& mipChainImageView)
    : m_gbuffer(gbuffer),
      m_mipChainImageView(mipChainImageView),
      m_writeUvs(nullptr),
      m_sampleGbuffer(nullptr),
      m_isRendered(false)
{
}

SSRPass::~SSRPass()
{
    delete m_writeUvs;
    delete m_sampleGbuffer;

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

const Handle<Texture>& SSRPass::GetFinalResultTexture() const
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

        FramebufferRef writeUVsFramebuffer = g_renderInterface->MakeFramebuffer(framebufferDesc);

#if HYP_DEBUG_MODE
        writeUVsFramebuffer->SetDebugName(NAME("SSRWriteUVsFramebuffer"));
#endif

        AttachmentDesc attachmentDesc {};
        attachmentDesc.imageType = TextureType::Texture2D;
        attachmentDesc.format = m_uvsTexture->GetFormat();
        attachmentDesc.loadOp = LoadOperation::CLEAR;
        attachmentDesc.storeOp = StoreOperation::STORE;

        Attachment* attachment = writeUVsFramebuffer->AddAttachment(
            0,
            attachmentDesc,
            g_renderInterface->MakeImageView(m_uvsTexture->GetGpuImage()));

        CheckResult(writeUVsFramebuffer->Create());

        delete m_writeUvs;

        m_writeUvs = new FullScreenPass(
            ShaderDesc(NAME("SSRWriteUVs"), shaderProperties),
            writeUVsFramebuffer,
            m_uvsTexture->GetFormat(),
            m_uvsTexture->GetExtent().GetXY(),
            nullptr);

        m_writeUvs->Create();
    }

    // Sample pass - renders to m_sampledResultTexture
    {
        // Create framebuffer for sampled result texture
        FramebufferDesc framebufferDesc {};
        framebufferDesc.extent = m_sampledResultTexture->GetExtent().GetXY();
        framebufferDesc.numLayers = 1;

        FramebufferRef sampleGBufferFramebuffer = g_renderInterface->MakeFramebuffer(framebufferDesc);

#if HYP_DEBUG_MODE
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
            g_renderInterface->MakeImageView(m_sampledResultTexture->GetGpuImage()));

        CheckResult(sampleGBufferFramebuffer->Create());

        delete m_sampleGbuffer;

        m_sampleGbuffer = new FullScreenPass(
            ShaderDesc(NAME("SSRSampleGBuffer"), shaderProperties),
            sampleGBufferFramebuffer,
            SSRColorFormat,
            m_sampledResultTexture->GetExtent().GetXY(),
            nullptr);

        m_sampleGbuffer->Create();
    }
}

void SSRPass::UpdatePipelineState(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;

    const Vec2u renderTargetExtent = Vec2u(Vec2f(renderSetup.view->GetOutputTarget().GetFramebuffer()->GetExtent()) * cvSSRResolutionScale.Get());

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

        if (m_temporalBlending)
        {
            m_temporalBlending.Reset();
        }

        // Create textures
        m_uvsTexture = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            SSRTraceFormat,
            Vec3u { uint32(MathUtil::Ceil(m_currentExtent.x * TraceResolutionScale)), uint32(MathUtil::Ceil(m_currentExtent.y * TraceResolutionScale)), 1 },
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_ATTACHMENT | IU_SAMPLED
        });

        m_uvsTexture->SetName(NAME("SSRTexture_UVs"));
        CheckResult(m_uvsTexture->Create());

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
        CheckResult(m_sampledResultTexture->Create());

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

void SSRPass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_NAMED_SCOPE("Screen Space Reflections");

    AssertDebug(renderSetup.world && renderSetup.view);

    UpdatePipelineState(frame, renderSetup);

    const uint32 frameIndex = frame->GetFrameIndex();
    CommandRecorder& cr = frame->cr;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    { // Update cbuffer
        SSRConstants constants {};
        constants.dimensions = Vec4u(m_currentExtent, 0, 0);
        constants.rayStep = cvSSRRayStep.Get();
        constants.numIterations = cvSSRMaxIterations.Get();
        constants.maxRayDistance = cvSSRMaxDistance.Get();
        constants.distanceBias = cvSSRDistanceBias.Get();
        constants.offset = 0.25f;
        constants.eyeFadeStart = 0.98f;
        constants.eyeFadeEnd = 0.99f;
        constants.screenEdgeFadeStart = 0.98f;
        constants.screenEdgeFadeEnd = 0.99f;

        g_renderInterface->cbufferAllocator->Write(&constants);
        g_renderInterface->cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);
    }

    { // PASS 1 -- write UVs
        m_writeUvs->Begin(frame, renderSetup);

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_NORMALS)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferMaterialTexture"_sh, m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_MATERIAL)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferVelocityTexture"_sh, m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferMipChain"_sh, m_mipChainImageView ? m_mipChainImageView : g_renderInterface->placeholderData->GetImageView2D1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_DEPTH)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "DeferredResult"_sh, m_mipChainImageView ? m_mipChainImageView : g_renderInterface->placeholderData->GetImageView2D1x1R8());
        cr << SetShaderUniform(uniformIndex++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
        cr << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(uniformIndex++, "BlueNoiseBuffer"_sh, g_renderInterface->blueNoiseBuffer);
        cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Worlds].gpuBuffer);
        cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Cameras].gpuBuffer, TShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()));

        m_writeUvs->RenderFullScreenQuad(frame, renderSetup);
        m_writeUvs->End(frame, renderSetup);

        cr << InsertBarrier(m_uvsTexture->GetGpuImage(), RS_SHADER_RESOURCE);
    }

    { // PASS 2 -- fill color buffer using mip chain to sample based on roughness
        m_sampleGbuffer->Begin(frame, renderSetup);

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_NORMALS)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferMaterialTexture"_sh, m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_MATERIAL)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferVelocityTexture"_sh, m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferMipChain"_sh, m_mipChainImageView ? m_mipChainImageView : g_renderInterface->placeholderData->GetImageView2D1x1R8());
        cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_DEPTH)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
        cr << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(uniformIndex++, "BlueNoiseBuffer"_sh, g_renderInterface->blueNoiseBuffer);
        cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Worlds].gpuBuffer);
        cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Cameras].gpuBuffer, TShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()));
        cr << SetShaderUniform(uniformIndex++, "UVImage"_sh, g_renderInterface->textureViewCache->GetOrCreate(m_uvsTexture));

        m_sampleGbuffer->RenderFullScreenQuad(frame, renderSetup);
        m_sampleGbuffer->End(frame, renderSetup);
    }

    if (UseTemporalBlending && m_temporalBlending != nullptr)
    {
        m_temporalBlending->Render(frame, renderSetup);
    }

    m_isRendered = true;
}

#pragma endregion SSRPass

} // namespace Hyperion
