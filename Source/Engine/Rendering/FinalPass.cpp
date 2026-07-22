/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/FinalPass.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/FullScreenPass.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Swapchain.hpp>
#include <Rendering/GraphicsPipeline.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/RawBufferAllocator.hpp>
#include <Rendering/RenderProxy.hpp>

#include <Rendering/Passes/DeferredPass.hpp>
#include <Rendering/Passes/UIPass.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Texture.hpp>

#include <Scene/World.hpp>
#include <Scene/View.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <System/AppContext.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/CVarManager.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

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

    if (acquiredImageIndex >= rs.swapchain->GetFramebuffers().Size())
    {
        // invalid, skip the frame
        return;
    }

    if (!m_quadMesh)
    {
        m_quadMesh = MeshBuilder::Quad();
        m_quadMesh->SetName(NAME("FinalPassQuad"));
        m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
        m_quadMesh->SetIsTransient(true);
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
        BlendModeFactor::SrcAlpha, BlendModeFactor::OneMinusSrcAlpha,
        BlendModeFactor::One, BlendModeFactor::OneMinusSrcAlpha));

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

    { // draw ui
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
                    if (!cameraProxy)
                    {
                        // Camera has been expired, will be removed from proxy list on next frame.
                        // GetActiveWorlds() returns previous frame's worlds when UseRingBuffer is false,
                        // so these can be stale
                        continue;
                    }

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
