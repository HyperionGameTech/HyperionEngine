/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/TAAPass.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/ComputePipeline.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/ShaderInstance.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/CBufferAllocator.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/View.hpp>

#include <Framework/EngineStats.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Threading/Threads.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statTAA("Rendering/GPU/TAA");

TAAPass::TAAPass(const GpuImageViewRef& inputImageView, const Vec2u& extent, GBuffer* gbuffer)
    : m_inputImageView(inputImageView),
      m_extent(extent),
      m_gbuffer(gbuffer),
      m_pingPongIndex(0),
      m_isInitialized(false)
{
}

TAAPass::~TAAPass()
{
    EnqueueDeletion(std::move(m_inputImageView));
}

void TAAPass::Create()
{
    if (m_isInitialized)
    {
        return;
    }

    Assert(m_gbuffer != nullptr);

    CreateTextures();

    m_isInitialized = true;
}

void TAAPass::CreateTextures()
{
    m_resultTexture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2D,
        TextureFormat::RGBA16F,
        Vec3u { m_extent.x, m_extent.y, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    m_resultTexture->SetName(NAME("TAA_ResultTexture"));
    CheckResult(m_resultTexture->Create());

    m_historyTexture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2D,
        TextureFormat::RGBA16F,
        Vec3u { m_extent.x, m_extent.y, 1 },
        TFM_NEAREST,
        TFM_NEAREST,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });

    m_historyTexture->SetName(NAME("TAA_HistoryTexture"));
    CheckResult(m_historyTexture->Create());
}

void TAAPass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    ENGINE_STAT_GPU_SCOPE(&s_statTAA);

    AssertDebug(renderSetup.world && renderSetup.view);

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
    AssertDebug(cameraProxy != nullptr);

    AttachmentBase* depthAttachment = m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Depth);
    AssertDebug(depthAttachment != nullptr);

    const Vec3u depthTextureDimensions = depthAttachment->GetExtent();

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    { // Set constants
        struct TAAConstants
        {
            Vec4u dimensions;
            Vec4f jitter;
            Vec2f nearFarClip;
        };

        TAAConstants constants {};
        constants.dimensions = Vec4u { m_extent, depthTextureDimensions.GetXY() };
        constants.jitter = cameraProxy->bufferData.jitter;
        constants.nearFarClip = Vec2f { cameraProxy->bufferData.cameraNear, cameraProxy->bufferData.cameraFar };

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);
    }

    Texture* textures[2] = { m_resultTexture.Get(), m_historyTexture.Get() };

    Texture* activeTexture = textures[m_pingPongIndex];
    Texture* prevTexture = textures[m_pingPongIndex ^ 1];

    frame->cr << InsertBarrier(activeTexture->GetGpuImage(), RS_UNORDERED_ACCESS);

    frame->cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);

    frame->cr << SetCurrentShader(ShaderDesc(NAME("TAA")));

    frame->cr << SetShaderUniform(0, "InColorTexture"_sh, m_inputImageView);
    frame->cr << SetShaderUniform(1, "InPrevColorTexture"_sh, RI.textureViewCache->GetOrCreate(prevTexture));
    frame->cr << SetShaderUniform(2, "InVelocityTexture"_sh, m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Velocity)->GetImageView());
    frame->cr << SetShaderUniform(3, "InDepthTexture"_sh, m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Depth)->GetImageView());
    frame->cr << SetShaderUniform(4, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
    frame->cr << SetShaderUniform(5, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    frame->cr << SetShaderUniform(6, "OutColorImage"_sh, RI.textureViewCache->GetOrCreate(activeTexture));
    frame->cr << SetShaderUniform(7, "TAAConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    frame->cr << DispatchCompute(Vec3u { (m_extent.x + 7) / 8, (m_extent.y + 7) / 8, 1 });
    frame->cr << InsertBarrier(activeTexture->GetGpuImage(), RS_SHADER_RESOURCE);

    m_pingPongIndex ^= 1;
}

} // namespace Hyperion
