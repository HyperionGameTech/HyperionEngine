/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/FinalPass.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/Frame.hpp>
#include <rendering/Swapchain.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/CBufferAllocator.hpp>
#include <rendering/RawBufferAllocator.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/passes/DeferredPass.hpp>
#include <rendering/passes/UIPass.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>

#include <rendering/util/MeshBuilder.hpp>

#include <system/AppContext.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/CVarManager.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

#if HYP_DEBUG_MODE || HYP_EDITOR
CVar<bool> cvShowDebugUI("ShowDebugUI", true);
#else // HYP_DEBUG_MODE || HYP_EDITOR
CVar<bool> cvShowDebugUI("ShowDebugUI", false);
#endif // HYP_DEBUG_MODE || HYP_EDITOR

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
        m_quadMesh->UploadGpuData();
    }

    const FramebufferRef& framebuffer = rs.swapchain->GetFramebuffers()[acquiredImageIndex];
    AssertDebug(framebuffer != nullptr);

    cr << SetCurrentFramebuffer(framebuffer);

    cr << SetCurrentViewport(Viewport { rs.swapchain->GetExtent() });

    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);

    cr << SetCurrentShader(ShaderDesc(NAME("FinalPass")));

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

    cr << SetShaderUniform(0, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
    cr << SetShaderUniform(1, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    DeferredPass* dr = static_cast<DeferredPass*>(RI.namedPasses[NamedPass::Deferred][0]);
    AssertDebug(dr != nullptr);

    for (const DeferredPass::RenderedViewOutput& output : dr->GetRenderedViewOutputs().items)
    {
        cr << SetShaderUniform(3, "InTexture"_sh, output.finalImageView);

        cr << CommitDrawState();

        cr << BindVertexBuffer(m_quadMesh->GetVertexBuffer());
        cr << BindIndexBuffer(m_quadMesh->GetIndexBuffer());

        cr << DrawIndexed(6);
    }

    if (cvShowDebugUI.Get())
    {
        // draw ui
        UIPass* uiPass = static_cast<UIPass*>(RI.namedPasses[NamedPass::UI][0]);

        if (uiPass != nullptr)
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

                    Camera* camera = view->GetCamera();
                    AssertDebug(camera != nullptr);

                    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(camera));
                    AssertDebug(cameraProxy != nullptr);

                    Viewport viewport {};
                    viewport.extent = MathUtil::Max(cameraProxy->bufferData.dimensions.GetXY(), Vec2u::One());

                    currentViewSetup.viewport = viewport;

                    uiPass->RenderFrame(frame, currentViewSetup);
                }
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
