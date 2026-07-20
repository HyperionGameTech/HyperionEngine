/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/ReflectionsPass.hpp>
#include <Rendering/Passes/DeferredPass.hpp>
#include <Rendering/Passes/DeferredPassShared.hpp>
#include <Rendering/Passes/SSRPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Swapchain.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/View.hpp>
#include <Scene/EnvProbe.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Framework/CVarManager.hpp>

namespace Hyperion {

extern CVar<bool> g_cvSSR;
extern CVar<bool> g_cvRayTracedReflections;

static constexpr TypeId EnvProbeTypeToTypeId[EPT_MAX] = {
    TypeId::ForType<SkyProbe>(),        // EPT_SKY
    TypeId::ForType<ReflectionProbe>(), // EPT_REFLECTION
    TypeId::ForType<IrradianceProbe>()  // EPT_AMBIENT (fixme when derived class)
};

// Sky renders first
constexpr FixedArray<EnvProbeType, CMT_MAX> EnvProbeTypes {
    EPT_SKY,
    EPT_REFLECTION
};

constexpr FixedArray<CubemapType, CMT_MAX> CubemapTypes {
    CMT_DEFAULT,           // EPT_SKY
    CMT_PARALLAX_CORRECTED // EPT_REFLECTION
};

#pragma region ReflectionsPass

ReflectionsPass::ReflectionsPass(Vec2u extent, GBuffer* gbuffer, const GpuImageViewRef& mipChainImageView)
    : FullScreenPass(TextureFormat::RGBA16F, extent, gbuffer),
      m_mipChainImageView(mipChainImageView),
      m_isFirstFrame(true)
{
    SetPassName(NAME("Reflections"));
    m_shaderDesc = ShaderDesc(NAME("ApplyReflectionProbe"));

    SetBlendFunction(BlendFunction(
        BlendModeFactor::SrcAlpha, BlendModeFactor::OneMinusSrcAlpha, BlendModeFactor::One, BlendModeFactor::OneMinusSrcAlpha));
}

ReflectionsPass::~ReflectionsPass()
{
    EnqueueDeletion(std::move(m_mipChainImageView));

    ssrPass.Reset();
}

void ReflectionsPass::Create()
{
    FullScreenPass::Create();

    CreateSSRPass();
}

bool ReflectionsPass::ShouldRenderSSR() const
{
    return g_cvSSR.Get() && !g_cvRayTracedReflections.Get();
}

void ReflectionsPass::CreateSSRPass()
{
    ssrPass = MakeUnique<SSRPass>(m_gbuffer, m_mipChainImageView);
    ssrPass->Create();
}

void ReflectionsPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void ReflectionsPass::Render(Frame* frame, const RenderSetup& rs)
{
    AssertDebug(rs.world && rs.view);
    AssertDebug(rs.passData != nullptr);

    RenderProxyList& rpl = GetConsumerProxyList(rs.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    Viewport viewport = rs.viewport;

    if (ShouldRenderCheckerboarded())
    {
        const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (GetWorldBufferData()->frameCounter & 1);
        const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        viewport = Viewport { viewportExtent, viewportOffset };
    }

    CommandRecorder& cr = frame->cr;

    if (ShouldRenderSSR())
    {
        ssrPass->Render(frame, rs);
    }

    cr << SetTopology(TOP_TRIANGLES);
    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);

    cr << SetCurrentViewport(viewport);

    cr << SetCurrentShader(m_shaderDesc);

    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);
    cr << SetStencilTest(false);

    cr << SetCurrentBlendFunction(BlendFunction(
        BlendModeFactor::SrcAlpha,
        BlendModeFactor::OneMinusSrcAlpha,
        BlendModeFactor::One,
        BlendModeFactor::OneMinusSrcAlpha));

    cr << SetFillMode(FM_FILL);
    cr << SetFaceCullMode(FCM_BACK);

    HYP_DEFER({
        cr << SetCurrentBlendFunction(BlendFunction::None());
        cr << SetDepthTest(true);
        cr << SetDepthWrite(true);
    });

    FixedArray<Array<EnvProbe*, RenderTempAllocator>, CMT_MAX> probesPerCubemapType;

    for (uint32 cubemapType = 0; cubemapType < CMT_MAX; cubemapType++)
    {
        const EnvProbeType envProbeType = EnvProbeTypes[cubemapType];

        for (EnvProbe* envProbe : rpl.GetEnvProbes().GetElements(EnvProbeTypeToTypeId[envProbeType]))
        {
            probesPerCubemapType[cubemapType].PushBack(envProbe);
        }
    }

    cr << SetCurrentFramebuffer(GetFramebuffer());

    // render previous frame's result to screen if doing temporal blending (and not checkerboarded)
    if (!m_isFirstFrame && UsesTemporalBlending() && !ShouldRenderCheckerboarded())
    {
        DrawHistoryTexture(frame, rs);
    }

    cr << SetShaderUniform(0, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(1, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());

    DeferredPassData* dpd = DynamicCast<DeferredPassData>(rs.passData);
    AssertDebug(dpd != nullptr);

    const FramebufferRef& opaquePassFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);

    for (uint32 attachmentIndex = 0; attachmentIndex < NumGBufferTargets; attachmentIndex++)
    {
        cr << SetShaderUniform(2 + attachmentIndex, GBufferTextureNames[attachmentIndex], opaquePassFramebuffer->GetAttachment(attachmentIndex)->GetImageView());
    }

    cr << SetShaderUniform(2 + NumGBufferTargets, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(rs.view->GetCamera()));
    cr << SetShaderUniform(3 + NumGBufferTargets, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    cr << SetShaderUniform(4 + NumGBufferTargets, "EnvProbesBuffer"_sh, RI.namedBuffers[NamedBuffer::EnvProbes]);

    cr << SetShaderUniform(10 + NumGBufferTargets, "BlueNoiseBuffer"_sh, RI.blueNoiseBuffer.gpuBuffer);
    cr << SetShaderUniform(11 + NumGBufferTargets, "SphereSamplesBuffer"_sh, RI.sphereSamplesBuffer.gpuBuffer);

    cr << SetShaderUniform(12 + NumGBufferTargets, "GBufferMipChain"_sh, RI.textureViewCache->GetOrCreate(dpd->mipChain));

    cr << SetShaderUniform(13 + NumGBufferTargets, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
    cr << SetShaderUniform(14 + NumGBufferTargets, "EnvProbesDepthTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesDepthTexture));

    uint32 numRenderedEnvProbes = 0;

    for (uint32 envProbeTypeIndex = 0; envProbeTypeIndex < ArraySize(EnvProbeTypes); envProbeTypeIndex++)
    {
        const CubemapType cubemapType = CubemapTypes[envProbeTypeIndex];

        const Array<EnvProbe*, RenderTempAllocator>& probes = probesPerCubemapType[cubemapType];

        if (probes.Empty())
        {
            continue;
        }

        for (EnvProbe* envProbe : probes)
        {
            if (numRenderedEnvProbes >= MaxBoundReflectionProbes)
            {
                HYP_LOG(Rendering, Warning, "Attempting to render too many reflection probes.");

                break;
            }

            cr << SetShaderUniform(5 + NumGBufferTargets, "CurrentEnvProbe"_sh, RI.namedBuffers[NamedBuffer::EnvProbes], Resources::GetBinding(envProbe));

            RenderFullScreenQuad(frame, rs);

            ++numRenderedEnvProbes;
        }
    }

    if (ShouldRenderSSR())
    {
        Texture* ssrTexture = ssrPass->GetFinalResultTexture();

        // render SSR to screen
        FramebufferDesc framebufferDesc = rs.view->GetOutputTarget().GetFramebuffer()->GetFramebufferDesc();
        framebufferDesc.attachments[0].loadOp = LoadOperation::LOAD;
        framebufferDesc.attachments[0].blendFunction = BlendFunction(BlendModeFactor::SrcAlpha, BlendModeFactor::OneMinusSrcAlpha, BlendModeFactor::One, BlendModeFactor::OneMinusSrcAlpha);

        cr << SetCurrentViewport(rs.viewport);

        cr << SetCurrentShader(ShaderDesc(NAME("BlitTexture")));

        // reset
        cr << SetDepthTest(false);
        cr << SetDepthWrite(false);

        cr << SetCurrentBlendFunction(BlendFunction(
            BlendModeFactor::SrcAlpha,
            BlendModeFactor::OneMinusSrcAlpha,
            BlendModeFactor::One,
            BlendModeFactor::OneMinusSrcAlpha));

        cr << SetShaderUniform(0, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(1, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(2, "InTexture"_sh, RI.textureViewCache->GetOrCreate(ssrTexture));

        RenderFullScreenQuad(frame, rs);

        cr << SetDepthTest(true);
        cr << SetDepthWrite(true);
        cr << SetCurrentBlendFunction(BlendFunction::None());
    }

    cr << SetCurrentFramebuffer(nullptr);

    if (ShouldRenderCheckerboarded())
    {
        MergeCheckerboard(frame, rs);
    }

    if (UsesTemporalBlending())
    {
        if (!ShouldRenderCheckerboarded())
        {
            CopyResultToPreviousTexture(frame, rs);
        }

        m_temporalBlending->Render(frame, rs);
    }

    m_isFirstFrame = false;
}

#pragma endregion ReflectionsPass

} // namespace Hyperion
