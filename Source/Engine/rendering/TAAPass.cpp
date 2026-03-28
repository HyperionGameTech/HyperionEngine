/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

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

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/View.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/threading/Threads.hpp>

namespace Hyperion {

struct alignas(16) TAAConstants
{
    Vec2u dimensions;
    Vec2u depthTextureDimensions;
    Vec2f cameraNearFar;
};

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
    EnqueueDeletion(std::move(m_cbuffers));
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
    HYP_NAMED_SCOPE("Temporal AA");

    AssertDebug(renderSetup.world && renderSetup.view);

    const uint32 frameIndex = frame->GetFrameIndex();

    if (!m_cbuffers[frameIndex])
    {
        m_cbuffers[frameIndex] = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(TAAConstants));
#if HYP_DEBUG_MODE
        m_cbuffers[frameIndex]->SetDebugName(NAME("TAAConstants"));
#endif

        CheckResult(m_cbuffers[frameIndex]->Create());
    }

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
    Assert(cameraProxy != nullptr);

    const Vec3u depthTextureDimensions = m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_DEPTH)->GetGpuImage()->GetExtent();

    TAAConstants constants {};
    constants.dimensions = m_extent;
    constants.depthTextureDimensions = depthTextureDimensions.GetXY();
    constants.cameraNearFar = Vec2f { cameraProxy->bufferData.cameraNear, cameraProxy->bufferData.cameraFar };

    m_cbuffers[frameIndex]->Copy(sizeof(constants), &constants);

    Texture* activeTexture = frame->GetFrameIndex() % 2 == 0 ? m_resultTexture : m_historyTexture;
    Texture* prevTexture = frame->GetFrameIndex() % 2 == 0 ? m_historyTexture : m_resultTexture;

    frame->cr << InsertBarrier(activeTexture->GetGpuImage(), RS_UNORDERED_ACCESS);

    frame->cr << SetVertexAttributes(VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::TexCoord0);

    frame->cr << SetCurrentShader(ShaderDesc(NAME("TAA")));

    frame->cr << SetShaderUniform(0, "InColorTexture"_sh, m_inputImageView);
    frame->cr << SetShaderUniform(1, "InPrevColorTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(prevTexture));
    frame->cr << SetShaderUniform(2, "InVelocityTexture"_sh, m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());
    frame->cr << SetShaderUniform(3, "InDepthTexture"_sh, m_gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_DEPTH)->GetImageView());
    frame->cr << SetShaderUniform(4, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
    frame->cr << SetShaderUniform(5, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    frame->cr << SetShaderUniform(6, "OutColorImage"_sh, g_renderInterface->textureViewCache->GetOrCreate(activeTexture));
    frame->cr << SetShaderUniform(7, "TAAConstants"_sh, m_cbuffers[frameIndex]);

    frame->cr << DispatchCompute(Vec3u { (m_extent.x + 7) / 8, (m_extent.y + 7) / 8, 1 });
    frame->cr << InsertBarrier(activeTexture->GetGpuImage(), RS_SHADER_RESOURCE);
}

} // namespace Hyperion
