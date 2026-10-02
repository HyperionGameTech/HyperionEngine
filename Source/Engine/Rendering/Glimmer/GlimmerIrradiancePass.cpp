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
static EngineStatGpuTimer s_statGlimmerIrradianceHalf("Rendering/GPU/Glimmer/IrradianceHalf");
static EngineStatGpuTimer s_statGlimmerIrradianceFull("Rendering/GPU/Glimmer/IrradianceFull");

static constexpr uint8 SkippedStencilMask = SkyStencilMask | LightmapStencilMask;

static constexpr TextureFormat SpecularFormat = TextureFormat::RG8;
static constexpr TextureFormat ReflectionFormat = TextureFormat::R11G11B10F;

static StaticShaderPropertyId s_propModeFull { ShaderProperty(NAME("MODE"), NAME("FULL")) };
static StaticShaderPropertyId s_propModeHalf { ShaderProperty(NAME("MODE"), NAME("HALF")) };
static StaticShaderPropertyId s_propModeUpsample { ShaderProperty(NAME("MODE"), NAME("UPSAMPLE")) };

// the pixel of each 2x2 quad the half-res pass shades, cycled so TAA sees all four
static const Vec2u s_halfResPicks[4] = { Vec2u(0, 0), Vec2u(1, 1), Vec2u(1, 0), Vec2u(0, 1) };

#pragma region GlimmerIrradiancePass

GlimmerIrradiancePass::GlimmerIrradiancePass(Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TextureFormat::RGBA16F, extent, gbuffer),
      m_shaderData {},
      m_frameIndex(0)
{
    SetPassName(NAME("GlimmerIrradiance"));
}

GlimmerIrradiancePass::~GlimmerIrradiancePass()
{
    if (m_halfFramebuffer.IsValid())
    {
        EnqueueDeletion(std::move(m_halfFramebuffer));
    }
}

const GpuImageViewRef& GlimmerIrradiancePass::GetSpecularImageView() const
{
    return m_framebuffer->GetAttachment(1)->GetImageView();
}

const GpuImageViewRef& GlimmerIrradiancePass::GetReflectionImageView() const
{
    return m_framebuffer->GetAttachment(2)->GetImageView();
}

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

    Attachment* specularAttachment = m_framebuffer->AddAttachment(
        1,
        AttachmentDesc { TextureType::Texture2D, SpecularFormat, LoadOperation::Clear, StoreOperation::Store });

    Check(specularAttachment->Create());

    Attachment* reflectionAttachment = m_framebuffer->AddAttachment(
        2,
        AttachmentDesc { TextureType::Texture2D, ReflectionFormat, LoadOperation::Clear, StoreOperation::Store });

    Check(reflectionAttachment->Create());

    const GpuImageViewRef& depthImageView = m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Depth)->GetImageView();
    Assert(depthImageView.IsValid());

    AttachmentDesc depthAttachmentDesc {};
    depthAttachmentDesc.imageType = TextureType::Texture2D;
    depthAttachmentDesc.format = depthImageView->GetImage()->GetTextureFormat();
    depthAttachmentDesc.loadOp = LoadOperation::Load;
    depthAttachmentDesc.storeOp = StoreOperation::None;
    depthAttachmentDesc.onlyStencil = true;

    m_framebuffer->AddAttachment(3, depthAttachmentDesc, depthImageView);

    Check(m_framebuffer->Create());

    if (m_halfFramebuffer.IsValid())
    {
        EnqueueDeletion(std::move(m_halfFramebuffer));
    }

    FramebufferDesc halfFramebufferDesc {};
    halfFramebufferDesc.extent = MathUtil::Max(Vec2u((m_extent.x + 1) / 2, (m_extent.y + 1) / 2), Vec2u::One());
    halfFramebufferDesc.numLayers = 1;

    m_halfFramebuffer = RI.MakeFramebuffer(halfFramebufferDesc);

#ifdef HYP_RHI_DEBUG_NAMES
    m_halfFramebuffer->SetDebugName(NAME("GlimmerHalfIrradianceFramebuffer"));
#endif

    Check(m_halfFramebuffer->AddAttachment(0, AttachmentDesc { TextureType::Texture2D, m_imageFormat, LoadOperation::Clear, StoreOperation::Store })->Create());
    Check(m_halfFramebuffer->AddAttachment(1, AttachmentDesc { TextureType::Texture2D, SpecularFormat, LoadOperation::Clear, StoreOperation::Store })->Create());
    Check(m_halfFramebuffer->AddAttachment(2, AttachmentDesc { TextureType::Texture2D, ReflectionFormat, LoadOperation::Clear, StoreOperation::Store })->Create());

    Check(m_halfFramebuffer->Create());
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

    m_shaderData.params = Vec4u(isActive ? 1u : 0u, isActive && isDebugView ? 1u : 0u, g_cvGlimmerSpecularOcclusion.Get() ? 1u : 0u, 0);

    const GpuImageRef& targetImage = GetAttachment(0)->GetGpuImage();
    const GpuImageRef& specularImage = GetAttachment(1)->GetGpuImage();
    const GpuImageRef& reflectionImage = GetAttachment(2)->GetGpuImage();

    if (!isActive)
    {
        cr << InsertBarrier(targetImage, ResourceState::ShaderResource, ShaderModuleType::Pixel);
        cr << InsertBarrier(specularImage, ResourceState::ShaderResource, ShaderModuleType::Pixel);
        cr << InsertBarrier(reflectionImage, ResourceState::ShaderResource, ShaderModuleType::Pixel);

        return;
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerIrradiance);

    const bool isHalfRes = g_cvGlimmerHalfRes.Get() && !isDebugView;
    const Vec2u halfResPick = s_halfResPicks[m_frameIndex++ % 4];

    const GBufferTarget& opaquePass = m_gbuffer->GetPass(GBufferPass::Opaque);

    const auto draw = [&](const StaticShaderPropertyId& modeProperty)
    {
        ShaderPropertySet shaderProperties;
        shaderProperties.Add(modeProperty);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerIrradiance"), shaderProperties));

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        glimmerPass->WriteApplyShaderData(*RI.cbufferAllocator, renderSetup.world);

        const Vec4u irradianceParams(halfResPick.x, halfResPick.y, 0, 0);
        RI.cbufferAllocator->Write(&irradianceParams);

        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, opaquePass.GetAttachment(GBufferTarget::Normals)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, opaquePass.GetAttachment(GBufferTarget::Depth)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GBufferMaterialTexture"_sh, opaquePass.GetAttachment(GBufferTarget::MatData)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerHalfIrradianceTexture"_sh, m_halfFramebuffer->GetAttachment(0)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerHalfSpecularTexture"_sh, m_halfFramebuffer->GetAttachment(1)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "GlimmerHalfReflectionTexture"_sh, m_halfFramebuffer->GetAttachment(2)->GetImageView());
        cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));
        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        uniformIndex = glimmerPass->BindApplyResources(cr, uniformIndex, renderSetup.world);

        cr << CommitDrawState();

        cr << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer(0));
        cr << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer(0));
        cr << DrawIndexed(6);
    };

    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
    cr << SetFaceCullMode(FaceCullMode::Back);
    cr << SetFillMode(FillMode::Fill);
    cr << SetTopology(Topology::Triangles);
    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);
    cr << SetCurrentBlendFunction(BlendFunction::None());

    if (isHalfRes)
    {
        ENGINE_STAT_GPU_SCOPE(&s_statGlimmerIrradianceHalf);

        cr << SetCurrentFramebuffer(m_halfFramebuffer);
        cr << SetCurrentViewport(Viewport { m_halfFramebuffer->GetExtent() });
        cr << SetStencilTest(false);

        draw(s_propModeHalf);

        cr << SetCurrentFramebuffer(nullptr);

        cr << InsertBarrier(m_halfFramebuffer->GetAttachment(0)->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Pixel);
        cr << InsertBarrier(m_halfFramebuffer->GetAttachment(1)->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Pixel);
        cr << InsertBarrier(m_halfFramebuffer->GetAttachment(2)->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Pixel);
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerIrradianceFull);

    cr << SetCurrentFramebuffer(m_framebuffer);
    cr << SetCurrentViewport(renderSetup.viewport);

    cr << SetStencilTest(true);
    cr << SetStencilFunction(StencilFunction { StencilOp::Keep, StencilOp::Keep, StencilOp::Keep, StencilCompareOp::Equal });
    cr << SetStencilState(0, SkippedStencilMask, 0x0);

    draw(isHalfRes ? s_propModeUpsample : s_propModeFull);

    cr << SetStencilTest(false);
    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);

    cr << SetCurrentFramebuffer(nullptr);

    cr << InsertBarrier(targetImage, ResourceState::ShaderResource, ShaderModuleType::Pixel);
    cr << InsertBarrier(specularImage, ResourceState::ShaderResource, ShaderModuleType::Pixel);
    cr << InsertBarrier(reflectionImage, ResourceState::ShaderResource, ShaderModuleType::Pixel);
}

#pragma endregion GlimmerIrradiancePass

} // namespace Hyperion
