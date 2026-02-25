/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/TemporalAA.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderQueue.hpp>
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

TemporalAA::TemporalAA(const GpuImageViewRef& inputImageView, const Vec2u& extent, GBuffer* gbuffer)
    : m_inputImageView(inputImageView),
      m_extent(extent),
      m_gbuffer(gbuffer),
      m_isInitialized(false)
{
}

TemporalAA::~TemporalAA()
{
    EnqueueDeletion(std::move(m_inputImageView));
    EnqueueDeletion(std::move(m_cBuffers));
}

void TemporalAA::Create()
{
    if (m_isInitialized)
    {
        return;
    }

    Assert(m_gbuffer != nullptr);

    CreateTextures();

    m_isInitialized = true;
}

void TemporalAA::CreateTextures()
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
    InitObject(m_resultTexture);

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
    InitObject(m_historyTexture);
}

void TemporalAA::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_NAMED_SCOPE("Temporal AA");

    AssertDebug(renderSetup.world && renderSetup.view);

    const uint32 frameIndex = frame->GetFrameIndex();

    if (!m_cBuffers[frameIndex])
    {
        m_cBuffers[frameIndex] = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(TAAConstants));
#ifdef HYP_DEBUG_MODE
        m_cBuffers[frameIndex]->SetDebugName(NAME("TAAConstants"));
#endif

        CheckResult(m_cBuffers[frameIndex]->Create());
    }

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
    Assert(cameraProxy != nullptr);

    const Vec3u depthTextureDimensions = m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_DEPTH)->GetImage()->GetExtent();

    TAAConstants constants {};
    constants.dimensions = m_extent;
    constants.depthTextureDimensions = depthTextureDimensions.GetXY();
    constants.cameraNearFar = Vec2f { cameraProxy->bufferData.cameraNear, cameraProxy->bufferData.cameraFar };

    m_cBuffers[frameIndex]->Copy(sizeof(constants), &constants);

    Texture* activeTexture = frame->GetFrameIndex() % 2 == 0 ? m_resultTexture : m_historyTexture;
    Texture* prevTexture = frame->GetFrameIndex() % 2 == 0 ? m_historyTexture : m_resultTexture;

    frame->renderQueue << InsertBarrier(activeTexture->GetGpuImage(), RS_UNORDERED_ACCESS);

    frame->renderQueue << SetVertexAttributes(VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::TexCoord0);

    frame->renderQueue << SetCurrentShader(ShaderDesc(NAME("TemporalAA")));

    frame->renderQueue << SetShaderUniform(0, "InColorTexture"_sh, m_inputImageView);
    frame->renderQueue << SetShaderUniform(1, "InPrevColorTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(prevTexture));
    frame->renderQueue << SetShaderUniform(2, "InVelocityTexture"_sh, m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_VELOCITY)->GetImageView());
    frame->renderQueue << SetShaderUniform(3, "InDepthTexture"_sh, m_gbuffer->GetBucket(RB_OPAQUE).GetGBufferAttachment(GTN_DEPTH)->GetImageView());
    frame->renderQueue << SetShaderUniform(4, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
    frame->renderQueue << SetShaderUniform(5, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    frame->renderQueue << SetShaderUniform(6, "OutColorImage"_sh, g_renderInterface->textureViewCache->GetOrCreate(activeTexture));
    frame->renderQueue << SetShaderUniform(7, "TAAConstants"_sh, m_cBuffers[frameIndex]);

    frame->renderQueue << DispatchCompute(Vec3u { (m_extent.x + 7) / 8, (m_extent.y + 7) / 8, 1 });
    frame->renderQueue << InsertBarrier(activeTexture->GetGpuImage(), RS_SHADER_RESOURCE);
}

} // namespace Hyperion
