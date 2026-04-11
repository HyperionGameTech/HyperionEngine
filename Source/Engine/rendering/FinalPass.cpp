/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

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
#include <rendering/renderers/UIRenderer.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>

#include <util/MeshBuilder.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/CVarManager.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

extern CVar<bool> cvTAA;

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

    if (!m_quadMesh)
    {
        m_quadMesh = MeshBuilder::Quad();
        m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
        InitObject(m_quadMesh);
    }

    const FramebufferRef& framebuffer = rs.swapchain->GetFramebuffers()[acquiredImageIndex];
    AssertDebug(framebuffer != nullptr);

    cr << SetCurrentFramebuffer(framebuffer);

    cr << SetCurrentViewport(Viewport { rs.swapchain->GetExtent() });

    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);

    cr << SetCurrentShader(ShaderDesc(NAME("BlitTexture")));

    // Need blending to composite passes and ui
    cr << SetCurrentBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));

    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);
    cr << SetStencilTest(false);

    cr << SetFillMode(FM_FILL);
    cr << SetTopology(TOP_TRIANGLES);
    cr << SetFaceCullMode(FCM_NONE);
    
    cr << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
    cr << SetShaderUniform(1, "WorldsBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Worlds].gpuBuffer);

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

        GpuImageView* inputImageView = cvTAA.Get()
            ? g_renderInterface->textureViewCache->GetOrCreate(pd->taaPass->GetResultTexture())
            : pd->tonemapPass->GetFinalImageView();

        cr << SetShaderUniform(2, "InTexture"_sh, inputImageView);

        cr << CommitDrawState();

        cr << BindVertexBuffer(m_quadMesh->GetVertexBuffer());
        cr << BindIndexBuffer(m_quadMesh->GetIndexBuffer());

        cr << DrawIndexed(6);
    }

    // draw ui
    UIRenderer* uiRenderer = static_cast<UIRenderer*>(g_renderInterface->globalRenderers[GRT_UI][0]);

    if (uiRenderer != nullptr)
    {
        for (World* world : GetActiveWorlds())
        {
            for (View* view : world->GetViews())
            {
                if (!(view->GetFlags() & ViewFlags::UI_VIEW))
                {
                    continue;
                }

                RenderSetup currentViewSetup = rs.Fork();
                currentViewSetup.world = world;
                currentViewSetup.view = view;

                uiRenderer->RenderFrame(frame, currentViewSetup);
            }
        }
    }

    cr << SetCurrentFramebuffer(nullptr);

    // reset
    cr << SetCurrentBlendFunction(BlendFunction::None());
    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);
    cr << SetFaceCullMode(FCM_BACK);
}

#pragma endregion FinalPass

} // namespace Hyperion
