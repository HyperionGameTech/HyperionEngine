/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/FinalPass.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/Frame.hpp>
#include <rendering/Swapchain.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/TextureViewCache.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>

#include <util/MeshBuilder.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineDriver.hpp>

#define HYP_RENDER_UI_IN_FINAL_PASS

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region FinalPass

FinalPass::FinalPass()
{
}

FinalPass::~FinalPass()
{
    EnqueueDeletion(std::move(m_quadMesh));
    EnqueueDeletion(std::move(m_uiLayerImageView));
}

void FinalPass::SetUILayerImageView(const GpuImageViewRef& imageView)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    EnqueueDeletion(std::move(m_uiLayerImageView));

    m_uiLayerImageView = imageView;
}

void FinalPass::Create()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    m_quadMesh = MeshBuilder::Quad();
    m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
    InitObject(m_quadMesh);
}

void FinalPass::Render(Frame* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (!rs.swapchain)
    {
        return;
    }

    uint32 frameIndex = frame->GetFrameIndex();

    RenderQueue& rq = frame->renderQueue;

    const uint32 acquiredImageIndex = rs.swapchain->GetAcquiredImageIndex();

    if (acquiredImageIndex == ~0u)
    {
        // invalid, skip the frame
        return;
    }

    const FramebufferRef& framebuffer = rs.swapchain->GetFramebuffers()[acquiredImageIndex];
    AssertDebug(framebuffer != nullptr);

    rq << BeginFramebuffer(framebuffer);

    rq << SetCurrentView(framebuffer->GetRenderTargetDesc(), Viewport { rs.swapchain->GetExtent() });

    rq << SetVertexAttributes(VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::TexCoord0);

    rq << SetCurrentShader(ShaderDesc(NAME("BlitTexture")));

    // Need blending to composite passes and ui
    rq << SetCurrentBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));

    rq << SetDepthTest(false);
    rq << SetDepthWrite(false);
    rq << SetStencilTest(false);

    rq << SetFaceCullMode(FCM_BACK);
    rq << SetFillMode(FM_FILL);
    rq << SetTopology(TOP_TRIANGLES);
    
    rq << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
    rq << SetShaderUniform(1, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));

    // Render each sub-view
    DeferredRenderer* dr = static_cast<DeferredRenderer*>(g_renderInterface->globalRenderers[GRT_MAIN][0]);
    AssertDebug(dr != nullptr);

    // ordered by priority of the view
    for (const Pair<View*, DeferredRendererPassData*>& it : dr->GetLastFrameData().passData)
    {
        View* view = it.first;
        AssertDebug(view != nullptr);

        DeferredRendererPassData* pd = it.second;
        AssertDebug(pd != nullptr);

        GpuImageView* inputImageView = dr->GetRendererConfig().taaEnabled
            ? g_renderInterface->textureViewCache->GetOrCreate(pd->taaPass->GetResultTexture())
            : pd->tonemapPass->GetFinalImageView();

        rq << SetShaderUniform(2, "InTexture"_sh, inputImageView);

        rq << CommitDrawState();

        rq << BindVertexBuffer(m_quadMesh->GetVertexBuffer());
        rq << BindIndexBuffer(m_quadMesh->GetIndexBuffer());

        rq << DrawIndexed(6);
    }

#ifdef HYP_RENDER_UI_IN_FINAL_PASS
    // Render UI onto screen, blending with the scene render pass
    if (m_uiLayerImageView != nullptr)
    {
        rq << SetShaderUniform(2, "InTexture"_sh, m_uiLayerImageView);

        rq << CommitDrawState();

        rq << BindVertexBuffer(m_quadMesh->GetVertexBuffer());
        rq << BindIndexBuffer(m_quadMesh->GetIndexBuffer());

        rq << DrawIndexed(6);
    }
#endif

    rq << EndFramebuffer(framebuffer);

    // reset
    rq << SetCurrentBlendFunction(BlendFunction::None());
    rq << SetDepthTest(true);
    rq << SetDepthWrite(true);
}

#pragma endregion FinalPass

} // namespace Hyperion
