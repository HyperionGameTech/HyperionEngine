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

#pragma region ReflectionsPass

ReflectionsPass::ReflectionsPass(Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TextureFormat::RGBA16F, extent, gbuffer),
      m_isFirstFrame(true)
{
    SetPassName(NAME("Reflections"));
}

ReflectionsPass::~ReflectionsPass()
{
    ssrPass.Reset();
}

void ReflectionsPass::Create()
{
    FullScreenPass::Create();

    CreateSSRPass();
}

void ReflectionsPass::CreateFramebuffer()
{
    if (m_flags & FSP_EXTERNAL_RENDERTARGET)
    {
        return;
    }

    if (m_framebuffer != nullptr)
    {
        if (m_framebuffer->GetExtent() == m_extent)
        {
            Check(m_framebuffer->Create());

            return;
        }

        EnqueueDeletion(std::move(m_framebuffer));
    }

    Assert(m_extent.Volume() != 0);

    FramebufferDesc framebufferDesc;
    framebufferDesc.extent = m_extent;
    framebufferDesc.numLayers = 1;

    m_framebuffer = RI.MakeFramebuffer(framebufferDesc);

#if HYP_DEBUG_MODE
    m_framebuffer->SetDebugName(NAME_FMT("{}Framebuffer", GetName()));
#endif

    Attachment* attachment = m_framebuffer->AddAttachment(
        0,
        AttachmentDesc {
            TextureType::Texture2D,
            m_imageFormat,
            LoadOperation::LOAD,
            StoreOperation::STORE
        });

    Check(attachment->Create());
    Check(m_framebuffer->Create());
}

bool ReflectionsPass::ShouldRenderSSR() const
{
    return g_cvSSR.Get() && !g_cvRayTracedReflections.Get();
}

void ReflectionsPass::CreateSSRPass()
{
    ssrPass = MakeUnique<SSRPass>(m_extent, m_gbuffer);
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

    if (!ShouldRenderSSR())
    {
        return;
    }

    CommandRecorder& cr = frame->cr;

    ssrPass->Render(frame, rs);

    Texture* ssrTexture = ssrPass->GetFinalResultTexture();

    if (ssrTexture == nullptr)
    {
        return;
    }

    cr << SetCurrentFramebuffer(GetFramebuffer());

    cr << SetTopology(TOP_TRIANGLES);
    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);

    cr << SetCurrentViewport(rs.viewport);

    cr << SetCurrentShader(ShaderDesc(NAME("BlitTexture")));

    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);
    cr << SetStencilTest(false);
    cr << SetFillMode(FM_FILL);
    cr << SetFaceCullMode(FCM_BACK);

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

    cr << SetCurrentFramebuffer(nullptr);

    m_isFirstFrame = false;
}

#pragma endregion ReflectionsPass

} // namespace Hyperion
