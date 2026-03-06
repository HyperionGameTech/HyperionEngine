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

    CommandRecorder& cr = frame->cr;

    const uint32 acquiredImageIndex = rs.swapchain->GetAcquiredImageIndex();

    if (acquiredImageIndex == ~0u)
    {
        // invalid, skip the frame
        return;
    }

    if (m_uiLayerImageView != nullptr)
    {
        // transition ui image before we do any rendering, so we don't
        // need to end/begin the renderpass in between rendering view texture
        // (better for performance this way, plus, would break blending and leave us with a blank screen due to CLEAR load op)
        cr << InsertBarrier(m_uiLayerImageView->GetImage(), RS_SHADER_RESOURCE);
    }

    const FramebufferRef& framebuffer = rs.swapchain->GetFramebuffers()[acquiredImageIndex];
    AssertDebug(framebuffer != nullptr);

    cr << SetCurrentFramebuffer(framebuffer);

    cr << SetCurrentViewport(Viewport { rs.swapchain->GetExtent() });

    cr << SetVertexAttributes(VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::TexCoord0);

    cr << SetCurrentShader(ShaderDesc(NAME("BlitTexture")));

    // Need blending to composite passes and ui
    cr << SetCurrentBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));

    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);
    cr << SetStencilTest(false);

    cr << SetFaceCullMode(FCM_BACK);
    cr << SetFillMode(FM_FILL);
    cr << SetTopology(TOP_TRIANGLES);
    
    cr << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
    cr << SetShaderUniform(1, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));

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

        cr << SetShaderUniform(2, "InTexture"_sh, inputImageView);

        cr << CommitDrawState();

        cr << BindVertexBuffer(m_quadMesh->GetVertexBuffer());
        cr << BindIndexBuffer(m_quadMesh->GetIndexBuffer());

        cr << DrawIndexed(6);
    }

#ifdef HYP_RENDER_UI_IN_FINAL_PASS
    // Render UI onto screen, blending with the scene render pass
    if (m_uiLayerImageView != nullptr)
    {
        cr << SetShaderUniform(2, "InTexture"_sh, m_uiLayerImageView);

        cr << CommitDrawState();

        cr << BindVertexBuffer(m_quadMesh->GetVertexBuffer());
        cr << BindIndexBuffer(m_quadMesh->GetIndexBuffer());

        cr << DrawIndexed(6);
    }
#endif

    cr << SetCurrentFramebuffer(nullptr);

    // reset
    cr << SetCurrentBlendFunction(BlendFunction::None());
    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);

    
    if (m_uiLayerImageView != nullptr)
    {
        // @TODO: check if we can remove this transition.
        cr << InsertBarrier(m_uiLayerImageView->GetImage(), RS_RENDER_TARGET);
    }
}

#pragma endregion FinalPass

} // namespace Hyperion
