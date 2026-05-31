/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/TAAPass.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/CommandRecorder.hpp>
#include <rendering/Frame.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/CBufferAllocator.hpp>

#include <rendering/passes/DeferredPass.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/View.hpp>

#include <Framework/EngineStats.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/threading/Threads.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statTAA("Rendering/GPU/TAA");

TAAPass::TAAPass(const GpuImageViewRef& inputImageView, const Vec2u& extent, GBuffer* gbuffer)
    : m_inputImageView(inputImageView),
      m_extent(extent),
      m_gbuffer(gbuffer),
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
        IU_STORAGE | IU_SAMPLED
    });

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
        IU_STORAGE | IU_SAMPLED
    });

    m_historyTexture->SetName(NAME("TAA_HistoryTexture"));
    CheckResult(m_historyTexture->Create());
}

void TAAPass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    ENGINE_STAT_GPU_SCOPE(&s_statTAA);

    AssertDebug(renderSetup.world && renderSetup.view);

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
    AssertDebug(cameraProxy != nullptr);

    AttachmentBase* depthAttachment = m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_DEPTH);
    AssertDebug(depthAttachment != nullptr);

    const Vec3u depthTextureDimensions = depthAttachment->GetExtent();

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    { // Set constants
        struct TAAConstants
        {
            Vec2u dimensions;
            Vec2u depthTextureDimensions;
            Vec2f cameraNearFar;
        };

        TAAConstants constants {};
        constants.dimensions = m_extent;
        constants.depthTextureDimensions = depthTextureDimensions.GetXY();
        constants.cameraNearFar = Vec2f { cameraProxy->bufferData.cameraNear, cameraProxy->bufferData.cameraFar };

        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);
    }

    Texture* activeTexture = frame->GetFrameIndex() % 2 == 0 ? m_resultTexture : m_historyTexture;
    Texture* prevTexture = frame->GetFrameIndex() % 2 == 0 ? m_historyTexture : m_resultTexture;

    frame->cr << InsertBarrier(activeTexture->GetGpuImage(), RS_UNORDERED_ACCESS);

    frame->cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);

    frame->cr << SetCurrentShader(ShaderDesc(NAME("TAA")));

    frame->cr << SetShaderUniform(0, "InColorTexture"_sh, m_inputImageView);
    frame->cr << SetShaderUniform(1, "InPrevColorTexture"_sh, RI.textureViewCache->GetOrCreate(prevTexture));
    frame->cr << SetShaderUniform(2, "InVelocityTexture"_sh, m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());
    frame->cr << SetShaderUniform(3, "InDepthTexture"_sh, m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_DEPTH)->GetImageView());
    frame->cr << SetShaderUniform(4, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
    frame->cr << SetShaderUniform(5, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    frame->cr << SetShaderUniform(6, "OutColorImage"_sh, RI.textureViewCache->GetOrCreate(activeTexture));
    frame->cr << SetShaderUniform(7, "TAAConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    frame->cr << DispatchCompute(Vec3u { (m_extent.x + 7) / 8, (m_extent.y + 7) / 8, 1 });
    frame->cr << InsertBarrier(activeTexture->GetGpuImage(), RS_SHADER_RESOURCE);
}

} // namespace Hyperion
