/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerIrradiancePass.hpp>
#include <Rendering/Glimmer/GlimmerPass.hpp>
#include <Rendering/Glimmer/GlimmerTechnique.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/StencilMasks.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statGlimmerIrradiance("Rendering/GPU/Glimmer/Irradiance");

// lightmapped pixels take their GI from the lightmap, which is path traced too
static constexpr uint8 SkippedStencilMask = SkyStencilMask | LightmapStencilMask;

GlimmerIrradiancePass::GlimmerIrradiancePass(Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TextureFormat::RGBA16F, extent, gbuffer),
      m_shaderData {}
{
    SetPassName(NAME("GlimmerIrradiance"));
}

GlimmerIrradiancePass::~GlimmerIrradiancePass() = default;

void GlimmerIrradiancePass::CreateFramebuffer()
{
    if (m_framebuffer.IsValid())
    {
        EnqueueDeletion(std::move(m_framebuffer));
    }

    Assert(m_gbuffer != nullptr);

    FramebufferDesc framebufferDesc {};
    framebufferDesc.extent = MathUtil::Max(m_extent, Vec2u::One());
    framebufferDesc.numLayers = 1;

    m_framebuffer = RI.MakeFramebuffer(framebufferDesc);

#ifdef HYP_RHI_DEBUG_NAMES
    m_framebuffer->SetDebugName(NAME("GlimmerIrradianceFramebuffer"));
#endif

    // cleared, so pixels the stencil keeps out read as no Glimmer
    Attachment* colorAttachment = m_framebuffer->AddAttachment(
        0,
        AttachmentDesc { TextureType::Texture2D, m_imageFormat, LoadOperation::Clear, StoreOperation::Store });

    Check(colorAttachment->Create());

    const GpuImageViewRef& depthImageView = m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Depth)->GetImageView();
    Assert(depthImageView.IsValid());

    AttachmentDesc depthAttachmentDesc {};
    depthAttachmentDesc.imageType = TextureType::Texture2D;
    depthAttachmentDesc.format = depthImageView->GetImage()->GetTextureFormat();
    depthAttachmentDesc.loadOp = LoadOperation::Load;
    depthAttachmentDesc.storeOp = StoreOperation::None;
    depthAttachmentDesc.onlyStencil = true;

    m_framebuffer->AddAttachment(1, depthAttachmentDesc, depthImageView);

    Check(m_framebuffer->Create());
}

void GlimmerIrradiancePass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;

    AssertDebug(renderSetup.world && renderSetup.view);

    CommandRecorder& cr = frame->cr;

    GlimmerPass* glimmerPass = static_cast<GlimmerPass*>(RI.GetPass(NamedPass::Glimmer));
    const GlimmerTechnique* technique = glimmerPass ? glimmerPass->GetApplyTechnique(renderSetup.world) : nullptr;

    const int debugView = g_cvGlimmerDebugView.Get();
    const bool isDebugView = debugView == int(GlimmerDebugView::Irradiance)
        || debugView == int(GlimmerDebugView::Coverage)
        || g_cvGlimmerDebugSH.Get() > 0;

    const bool isActive = technique && technique->IsReady();

    m_shaderData.params = Vec4u(isActive ? 1u : 0u, isActive && isDebugView ? 1u : 0u, 0, 0);

    const GpuImageRef& targetImage = GetAttachment(0)->GetGpuImage();

    if (!isActive)
    {
        // never read while Glimmer is off, but the lighting pass binds it all the same
        cr << InsertBarrier(targetImage, ResourceState::ShaderResource, ShaderModuleType::Pixel);

        return;
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerIrradiance);

    cr << SetCurrentFramebuffer(m_framebuffer);

    cr << SetCurrentViewport(renderSetup.viewport);

    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
    cr << SetFaceCullMode(FaceCullMode::Back);
    cr << SetFillMode(FillMode::Fill);
    cr << SetTopology(Topology::Triangles);
    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);
    cr << SetCurrentBlendFunction(BlendFunction::None());

    cr << SetStencilTest(true);
    cr << SetStencilFunction(StencilFunction { StencilOp::Keep, StencilOp::Keep, StencilOp::Keep, StencilCompareOp::Equal });
    cr << SetStencilState(0, SkippedStencilMask, 0x0);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerIrradiance")));

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RenderProxyEnvProbe* skyProbeProxy = renderSetup.envProbe != nullptr
        ? static_cast<RenderProxyEnvProbe*>(GetRenderProxy(renderSetup.envProbe))
        : nullptr;

    if (skyProbeProxy != nullptr)
    {
        RI.cbufferAllocator->Write(&skyProbeProxy->bufferData);
    }
    else
    {
        // default constructed, so textureIndices is ~0u
        static const EnvProbeShaderData s_noSkyProbeData {};
        RI.cbufferAllocator->Write(&s_noSkyProbeData);
    }

    glimmerPass->WriteApplyShaderData(*RI.cbufferAllocator, renderSetup.world);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    const GBufferTarget& opaquePass = m_gbuffer->GetPass(GBufferPass::Opaque);

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, opaquePass.GetAttachment(GBufferTarget::Normals)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, opaquePass.GetAttachment(GBufferTarget::Depth)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));
    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    uniformIndex = glimmerPass->BindApplyResources(cr, uniformIndex, renderSetup.world);

    cr << CommitDrawState();

    cr << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer(0));
    cr << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer(0));
    cr << DrawIndexed(6);

    cr << SetStencilTest(false);
    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);

    cr << SetCurrentFramebuffer(nullptr);

    cr << InsertBarrier(targetImage, ResourceState::ShaderResource, ShaderModuleType::Pixel);
}

} // namespace Hyperion
