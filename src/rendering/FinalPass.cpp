/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/FinalPass.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/Frame.hpp>
#include <rendering/Swapchain.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/TextureViewCache.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/SafeDeleter.hpp>

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
    m_passData.Clear();

    SafeDelete(std::move(m_quadMesh));
    SafeDelete(std::move(m_uiLayerImageView));
}

void FinalPass::SetUILayerImageView(const GpuImageViewRef& imageView)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    SafeDelete(std::move(m_uiLayerImageView));

    if (g_engineDriver->IsShuttingDown())
    {
        // Don't set if the engine is in a shutdown state,
        // pipeline may already have been deleted.
        return;
    }

    m_uiLayerImageView = imageView;

    // Update all existing pass data
    for (FinalPassData& passData : m_passData)
    {
        if (passData.renderTextureToScreenPass != nullptr)
        {
            for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
            {
                const DescriptorSetRef& descriptorSet = passData.descriptorSets[frameIndex];
                Assert(descriptorSet != nullptr);

                if (imageView != nullptr)
                {
                    descriptorSet->SetElement("InTexture"_sh, imageView);
                }
                else
                {
                    descriptorSet->SetElement("InTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->placeholderData->defaultTexture2d));
                }
            }

            passData.dirtyFrameIndices = (1u << NumFramesInFlight) - 1;
            passData.lastUiImageView = m_uiLayerImageView;
        }
    }
}

void FinalPass::Create()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    m_quadMesh = MeshBuilder::Quad();
    m_quadMesh->SetFlags(MF_VIEW_INDEPENDENT);
    InitObject(m_quadMesh);
}

FinalPassData* FinalPass::GetOrCreatePassData(Swapchain* swapchain)
{
    Assert(swapchain != nullptr);

    const ObjId<Swapchain> id = swapchain->Id();
    FinalPassData* pPassData = m_passData.TryGet(id.ToIndex());

    if (pPassData)
    {
        if (pPassData->swapchain.GetUnsafe() == swapchain
            && pPassData->renderTextureToScreenPass != nullptr
            && pPassData->renderTextureToScreenPass->GetExtent() == swapchain->GetExtent()
            && pPassData->renderTextureToScreenPass->GetFormat() == swapchain->GetImageFormat())
        {
            return pPassData;
        }

        pPassData->renderTextureToScreenPass.Reset();
    }

    // Create new
    FinalPassData passData;
    passData.swapchain = MakeWeakRef(swapchain);

    ShaderRef renderTextureToScreenShader = g_shaderManager->GetOrCreate(NAME("RenderTextureToScreen"));
    Assert(renderTextureToScreenShader.IsValid());

    const DescriptorTableDeclaration* descriptorTableDecl = renderTextureToScreenShader->GetCompiledShader()->GetDescriptorTableDeclaration();
    Assert(descriptorTableDecl != nullptr);

    const DescriptorSetDeclaration* descriptorSetDecl = descriptorTableDecl->FindDescriptorSetDeclaration("RenderTextureToScreenDescriptorSet"_sh);
    Assert(descriptorSetDecl != nullptr);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        DescriptorSetRef descriptorSet = g_renderBackend->MakeDescriptorSet(DescriptorSetLayout(descriptorSetDecl));
        Assert(descriptorSet != nullptr);

        if (m_uiLayerImageView != nullptr)
        {
            descriptorSet->SetElement("InTexture"_sh, m_uiLayerImageView);
        }
        else
        {
            descriptorSet->SetElement("InTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
        }

        Assert(descriptorSet->Create());

        passData.descriptorSets[frameIndex] = std::move(descriptorSet);
    }

    passData.renderTextureToScreenPass = MakeHandle<FullScreenPass>(
        renderTextureToScreenShader,
        swapchain->GetImageFormat(),
        swapchain->GetExtent(),
        nullptr);

    passData.renderTextureToScreenPass->SetBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));

    passData.renderTextureToScreenPass->Create();

    passData.lastUiImageView = m_uiLayerImageView;
    passData.dirtyFrameIndices = 0;

    return &*m_passData.Set(id.ToIndex(), std::move(passData));
}

void FinalPass::Render(Frame* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (!rs.swapchain)
    {
        return;
    }

    FinalPassData* passData = GetOrCreatePassData(rs.swapchain);
    if (!passData || !passData->renderTextureToScreenPass)
    {
        return;
    }

    // Check UI updates
    if (passData->lastUiImageView != m_uiLayerImageView)
    {
        for (uint32 i = 0; i < NumFramesInFlight; i++)
        {
            DescriptorSetRef& descriptorSet = passData->descriptorSets[i];
            Assert(descriptorSet != nullptr);

            if (m_uiLayerImageView != nullptr)
            {
                descriptorSet->SetElement("InTexture"_sh, m_uiLayerImageView);
            }
            else
            {
                descriptorSet->SetElement("InTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
            }
        }

        passData->dirtyFrameIndices = (1u << NumFramesInFlight) - 1;
        passData->lastUiImageView = m_uiLayerImageView;
    }

    const uint32 frameIndex = frame->GetFrameIndex();
    const uint32 acquiredImageIndex = rs.swapchain->GetAcquiredImageIndex();

    const FramebufferRef& framebuffer = rs.swapchain->GetFramebuffers()[acquiredImageIndex];
    AssertDebug(framebuffer != nullptr);

    const uint32 globalDescriptorSetIndex = passData->renderTextureToScreenPass->GetGraphicsPipeline()->GetDescriptorSetIndex("Global"_sh);

    const uint32 descriptorSetIndex = passData->renderTextureToScreenPass->GetGraphicsPipeline()->GetDescriptorSetIndex("RenderTextureToScreenDescriptorSet"_sh);
    AssertDebug(descriptorSetIndex != ~0u);

    frame->renderQueue << BeginFramebuffer(framebuffer);

    frame->renderQueue << BindGraphicsPipeline(passData->renderTextureToScreenPass->GetGraphicsPipeline(), Viewport { rs.swapchain->GetExtent() });

    frame->renderQueue << BindDescriptorSet(
        g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global"_sh, frameIndex),
        passData->renderTextureToScreenPass->GetGraphicsPipeline(),
        {},
        globalDescriptorSetIndex);

    // Render each sub-view
    DeferredRenderer* dr = static_cast<DeferredRenderer*>(g_renderInterface->globalRenderers[GRT_MAIN][0]);
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
            passData->renderTextureToScreenPass->GetGraphicsPipeline(),
            {},
            descriptorSetIndex);

        frame->renderQueue << DrawIndexed(6);
    }

#ifdef HYP_RENDER_UI_IN_FINAL_PASS
    // Render UI onto screen, blending with the scene render pass
    if (m_uiLayerImageView != nullptr)
    {
        // If the UI pass has needs to be updated for the current frame index, do it
        if (passData->dirtyFrameIndices & (1u << frameIndex))
        {
            passData->descriptorSets[frameIndex]->Update(frameIndex);

            passData->dirtyFrameIndices &= ~(1u << frameIndex);
        }

        frame->renderQueue << BindDescriptorSet(
            passData->descriptorSets[frameIndex],
            passData->renderTextureToScreenPass->GetGraphicsPipeline(),
            {},
            descriptorSetIndex);

        frame->renderQueue << DrawIndexed(6);

        // DebugLog(LogType::Debug, "Rendering UI layer to screen for frame index %u\n", frameIndex);
    }
#endif

    frame->renderQueue << EndFramebuffer(framebuffer);
}

#pragma endregion FinalPass

} // namespace Hyperion
