/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/FinalPass.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderSwapchain.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/RenderDescriptorSet.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

#include <util/MeshBuilder.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#define HYP_RENDER_UI_IN_FINAL_PASS

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region FinalPass

FinalPass::FinalPass(const SwapchainRef& swapchain)
    : m_swapchain(swapchain),
      m_dirtyFrameIndices(0)
{
}

FinalPass::~FinalPass()
{
    m_quadMesh.Reset();

    if (m_renderTextureToScreenPass != nullptr)
    {
        m_renderTextureToScreenPass.Reset();
    }

    SafeDelete(std::move(m_uiLayerImageView));
    SafeDelete(std::move(m_swapchain));
}

void FinalPass::SetUILayerImageView(const GpuImageViewRef& imageView)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    SafeDelete(std::move(m_uiLayerImageView));

    if (g_engineDriver->IsShuttingDown())
    {
        // Don't set if the engine is in a shutdown state,
        // pipeline may already have been deleted.
        return;
    }

    if (m_renderTextureToScreenPass != nullptr)
    {
        const DescriptorTableRef& descriptorTable = m_renderTextureToScreenPass->GetGraphicsPipeline()->GetDescriptorTable();
        Assert(descriptorTable.IsValid());

        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("RenderTextureToScreenDescriptorSet", frameIndex);
            Assert(descriptorSet != nullptr);

            if (imageView != nullptr)
            {
                descriptorSet->SetElement("InTexture", imageView);
            }
            else
            {
                descriptorSet->SetElement("InTexture", g_renderBackend->GetTextureImageView(g_renderGlobalState->placeholderData->defaultTexture2d));
            }
        }
    }

    // Set frames to be dirty so the descriptor sets get updated before we render the UI
    m_dirtyFrameIndices = (1u << NumFramesInFlight) - 1;
    m_uiLayerImageView = imageView;
}

void FinalPass::Create()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    Assert(m_swapchain != nullptr);

    m_extent = m_swapchain->GetExtent();
    m_imageFormat = m_swapchain->GetImageFormat();

    Assert(m_extent.Volume() != 0);

    m_quadMesh = MeshBuilder::Quad();
    m_quadMesh->SetFlags(MF_VIEW_INDEPENDENT);
    InitObject(m_quadMesh);

    ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen_UI"));
    Assert(renderTextureToScreenShader.IsValid());

    const DescriptorTableDeclaration* descriptorTableDecl = renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration();
    Assert(descriptorTableDecl != nullptr);

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("RenderTextureToScreenDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        if (m_uiLayerImageView != nullptr)
        {
            descriptorSet->SetElement("InTexture", m_uiLayerImageView);
        }
        else
        {
            descriptorSet->SetElement("InTexture", g_renderGlobalState->placeholderData->GetImageView2D1x1R8());
        }
    }

    DeferCreate(descriptorTable);

    m_renderTextureToScreenPass = CreateObject<FullScreenPass>(
        renderTextureToScreenShader,
        std::move(descriptorTable),
        m_imageFormat,
        m_extent,
        nullptr);

    m_renderTextureToScreenPass->SetStage(RenderPassStage::PRESENT);

    m_renderTextureToScreenPass->SetBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));

    m_renderTextureToScreenPass->Create();
}

void FinalPass::Render(FrameBase* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    const uint32 frameIndex = frame->GetFrameIndex();
    const uint32 acquiredImageIndex = m_swapchain->GetAcquiredImageIndex();

    const FramebufferRef& framebuffer = m_swapchain->GetFramebuffers()[acquiredImageIndex];
    AssertDebug(framebuffer != nullptr);

    frame->renderQueue << BeginFramebuffer(framebuffer);
    frame->renderQueue << BindGraphicsPipeline(m_renderTextureToScreenPass->GetGraphicsPipeline(), Viewport { m_swapchain->GetExtent() });

    frame->renderQueue << BindDescriptorTable(
        m_renderTextureToScreenPass->GetGraphicsPipeline()->GetDescriptorTable(),
        m_renderTextureToScreenPass->GetGraphicsPipeline(),
        {},
        frameIndex);

    const uint32 descriptorSetIndex = m_renderTextureToScreenPass->GetGraphicsPipeline()->GetDescriptorTable()->GetDescriptorSetIndex("RenderTextureToScreenDescriptorSet");
    AssertDebug(descriptorSetIndex != ~0u);

    // Render each sub-view
    DeferredRenderer* dr = static_cast<DeferredRenderer*>(g_renderGlobalState->mainRenderer);
    AssertDebug(dr != nullptr);

    frame->renderQueue << BindVertexBuffer(m_quadMesh->GetVertexBuffer());
    frame->renderQueue << BindIndexBuffer(m_quadMesh->GetIndexBuffer());

    // ordered by priority of the view
    for (const Pair<View*, DeferredRendererPassData*>& it : dr->GetLastFrameData().passData)
    {
        View* view = it.first;
        AssertDebug(view != nullptr);

        DeferredRendererPassData* pd = it.second;
        AssertDebug(pd != nullptr);

        AssertDebug(pd->finalPassDescriptorSet);

        frame->renderQueue << BindDescriptorSet(
            pd->finalPassDescriptorSet,
            m_renderTextureToScreenPass->GetGraphicsPipeline(),
            {},
            descriptorSetIndex);

        frame->renderQueue << DrawIndexed(6);
    }

#ifdef HYP_RENDER_UI_IN_FINAL_PASS
    // Render UI onto screen, blending with the scene render pass
    if (m_uiLayerImageView != nullptr)
    {
        // If the UI pass has needs to be updated for the current frame index, do it
        if (m_dirtyFrameIndices & (1u << frameIndex))
        {
            m_renderTextureToScreenPass->GetGraphicsPipeline()->GetDescriptorTable()->Update(frameIndex);

            m_dirtyFrameIndices &= ~(1u << frameIndex);
        }

        frame->renderQueue << BindDescriptorSet(
            m_renderTextureToScreenPass->GetGraphicsPipeline()->GetDescriptorTable()->GetDescriptorSet("RenderTextureToScreenDescriptorSet", frameIndex),
            m_renderTextureToScreenPass->GetGraphicsPipeline(),
            {},
            descriptorSetIndex);

        frame->renderQueue << DrawIndexed(6);

        // DebugLog(LogType::Debug, "Rendering UI layer to screen for frame index %u\n", frameIndex);
    }
#endif

    frame->renderQueue << EndFramebuffer(framebuffer);
}

#pragma endregion FinalPass

} // namespace hyperion
