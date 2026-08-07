/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/TonemapPass.hpp>
#include <Rendering/Passes/DeferredPass.hpp>
#include <Rendering/Passes/BloomPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/StencilMasks.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Scene/View.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Framework/CVarManager.hpp>

namespace Hyperion {

extern CVar<bool> g_cvBloom;
extern CVar<float> g_cvTonemapExposure;

#pragma region TonemapPass

TonemapPass::TonemapPass(Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TextureFormat::RGBA16F, extent, gbuffer)
{
    SetPassName(NAME("Tonemap"));
}

TonemapPass::~TonemapPass()
{
}

void TonemapPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void TonemapPass::Render(Frame* frame, const RenderSetup& rs)
{
    ShaderPropertySet shaderProperties;
    shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("EXPOSURE"), g_cvTonemapExposure.Get())));
    m_shaderDesc = ShaderDesc(NAME("Tonemap"), shaderProperties);

    Begin(frame, rs);

    CommandRecorder& cr = frame->cr;

    // Filter out Debug draws! We don't want them tonemapped or with bloom applied.
    cr << SetStencilTest(true);
    cr << SetStencilFunction(StencilFunction { SO_KEEP, SO_KEEP, SO_KEEP, SCO_EQUAL });
    cr << SetStencilState(0, DebugStencilMask, 0x0);

    DeferredPassData* dpd = DynamicCast<DeferredPassData>(rs.passData);
    AssertDebug(dpd != nullptr);

    const FramebufferRef& inputsFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);

    uint32 numShaderUniforms = 0;

    cr << SetShaderUniform(numShaderUniforms++, "GBufferAlbedoTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Color)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferNormalsTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferMaterialTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::MatData)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferVelocityTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Velocity)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferDepthTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());

    Framebuffer* translucentPassFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(GBufferPass::Translucent);
    AssertDebug(translucentPassFramebuffer != nullptr);

    cr << SetShaderUniform(numShaderUniforms++, "DeferredResult"_sh, translucentPassFramebuffer->GetAttachment(GBufferTarget::Color)->GetImageView());

    cr << SetShaderUniform(numShaderUniforms++, "ShadowMapsTextureArray"_sh, RI.shadowMapCache->GetAtlasImageView());

    cr << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, RI.textureViewCache->GetOrCreate(dpd->mipChain));

    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());

    if (g_cvBloom.Get() && dpd->bloomPass)
    {
        cr << SetShaderUniform(numShaderUniforms++, "BloomResultTexture"_sh, RI.textureViewCache->GetOrCreate(dpd->bloomPass->GetBloomResult()));
    }
    else
    {
        cr << SetShaderUniform(numShaderUniforms++, "BloomResultTexture"_sh, RI.placeholderData->GetImageView2D1x1R8());
    }

    if (dpd->taaPass)
    {
        cr << SetShaderUniform(numShaderUniforms++, "TAAResultTexture"_sh, RI.textureViewCache->GetOrCreate(dpd->taaPass->GetResultTexture()));
    }
    else
    {
        cr << SetShaderUniform(numShaderUniforms++, "TAAResultTexture"_sh, RI.placeholderData->GetImageView2D1x1R8());
    }

    cr << SetShaderUniform(numShaderUniforms++, "DeferredIndirectResultTexture"_sh, dpd->lightingFramebuffer->GetAttachment(0)->GetImageView());

    cr << SetShaderUniform(numShaderUniforms++, "PostProcessingUniforms"_sh, dpd->postProcessing->GetUniformBuffer());

    cr << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(rs.view->GetCamera()));
    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    RenderFullScreenQuad(frame, rs);

    cr << SetStencilTest(false);

    End(frame, rs);
}

#pragma endregion TonemapPass

} // namespace Hyperion
