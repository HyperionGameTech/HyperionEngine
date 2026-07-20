/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/LightmapPass.hpp>
#include <Rendering/Passes/DeferredPass.hpp>
#include <Rendering/Passes/HBAOPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/ShaderManager.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/View.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Framework/CVarManager.hpp>

namespace Hyperion {

extern CVar<bool> g_cvHBAO;

struct LightmapVolumeUniforms
{
    float irradianceWeight;
    float radianceWeight;

    uint32 numAtlases;
};

#pragma region LightmapPass

LightmapPass::LightmapPass()
    : FullScreenPass(TextureFormat::RGBA16F, nullptr, FSP_EXTERNAL_RENDERTARGET)
{
    SetPassName(NAME("Lightmap"));
    m_shaderDesc = ShaderDesc(NAME("ApplyLightmap"));
}

LightmapPass::~LightmapPass()
{
    for (auto& data : m_lightmapVolumePassData)
    {
        EnqueueDeletion(std::move(data.uniformBuffers));
    }
}

void LightmapPass::Create()
{
    AssertOnThread(g_renderThread);

    FullScreenPass::Create();
}

void LightmapPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void LightmapPass::RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer)
{
    AssertDebug(renderSetup.world && renderSetup.volume && renderSetup.view);

    LightmapVolume* volume = DynamicCast<LightmapVolume>(renderSetup.volume);
    AssertDebug(volume != nullptr);

    RenderProxyLightmapVolume* proxy = static_cast<RenderProxyLightmapVolume*>(GetRenderProxy(volume));
    Assert(proxy != nullptr);

    if (proxy->numAtlases == 0)
    {
        return; // nothing to do
    }

    DeferredPassData* dpd = DynamicCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    Framebuffer* viewFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);
    AssertDebug(viewFramebuffer != nullptr);

    CommandRecorder& cr = frame->cr;

    cr << SetCurrentShader(m_shaderDesc);

    cr << SetCurrentViewport(renderSetup.viewport);

    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);

    cr << SetFaceCullMode(FCM_BACK);
    cr << SetFillMode(FM_FILL);
    cr << SetTopology(TOP_TRIANGLES);

    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);

    // // We store irradiance weight from the Indirect pass which samples EnvProbes.
    // // EnvProbes take priority over lightmap volumes.
    // // So we want to apply: 1.0 - irradianceWeight
    // cr << SetCurrentBlendFunction(BlendFunction(BlendModeFactor::OneMinusDstAlpha, BlendModeFactor::DstAlpha));

    cr << SetCurrentBlendFunction(BlendFunction::Additive());

    // cr << SetStencilTest(true);
    // cr << SetStencilFunction(StencilFunction {
    //     .passOp = SO_KEEP, .failOp = SO_KEEP, .depthFailOp = SO_KEEP,
    //     .compareOp = SCO_EQUAL // match values with equal atlas index when we render
    // });

    HYP_DEFER({
        // reset states
        cr << SetCurrentBlendFunction(BlendFunction::None());
        cr << SetStencilState(0, 0xFF, 0x0);
        cr << SetDepthWrite(true);
        cr << SetDepthTest(true);
        cr << SetStencilTest(false);
    });

    LightmapVolumePassData& data = GetLightmapVolumePassData(volume);

    uint32 numShaderUniforms = 0;

    // GBuffer textures
    cr << SetShaderUniform(numShaderUniforms++, "GBufferAlbedoTexture"_sh, viewFramebuffer->GetAttachment(GBufferTarget::Color)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferNormalsTexture"_sh, viewFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferMaterialTexture"_sh, viewFramebuffer->GetAttachment(GBufferTarget::MatData)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferDepthTexture"_sh, viewFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferVelocityTexture"_sh, viewFramebuffer->GetAttachment(GBufferTarget::Velocity)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, RI.textureViewCache->GetOrCreate(dpd->mipChain));

    // Samplers
    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());

    // Shadows
    cr << SetShaderUniform(numShaderUniforms++, "ShadowMapsTextureArray"_sh, RI.shadowMapCache->GetAtlasImageView());
    cr << SetShaderUniform(numShaderUniforms++, "PointLightShadowMapsTextureArray"_sh, RI.shadowMapCache->GetPointLightShadowMapImageView());

    // Cameras and Worlds buffers
    cr << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));
    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    // Env probes
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesDepthTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesDepthTexture));
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesBuffer"_sh, RI.namedBuffers[NamedBuffer::EnvProbes]);

    if (renderSetup.envProbe != nullptr)
        cr << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, RI.namedBuffers[NamedBuffer::EnvProbes], Resources::GetBinding(renderSetup.envProbe));
    else
        cr << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, RI.namedBuffers[NamedBuffer::EnvProbes], 0);

    if (dpd->hbao != nullptr && g_cvHBAO.Get())
    {
        cr << SetShaderUniform(numShaderUniforms++, "SSAOResultTexture"_sh, dpd->hbao->GetFinalImageView());
    }
    else
    {
        cr << SetShaderUniform(numShaderUniforms++, "SSAOResultTexture"_sh, RI.textureViewCache->GetOrCreate(RI.placeholderData->textureSolidWhite));
    }

    if (data.uniformBuffers.Size() < proxy->numAtlases)
    {
        data.uniformBuffers.Resize(proxy->numAtlases);
    }

    for (uint32 atlasIndex = 0; atlasIndex < proxy->numAtlases; atlasIndex++)
    {
        Texture* irradianceTexture = proxy->atlasIrradianceTextures[atlasIndex];
        Texture* radianceTexture = proxy->atlasRadianceTextures[atlasIndex];

        LightmapVolumeUniforms uniforms {};
        uniforms.numAtlases = proxy->numAtlases;
        uniforms.irradianceWeight = irradianceTexture ? 1.0f : 0.0f;
        uniforms.radianceWeight = radianceTexture ? 1.0f : 0.0f;

        GpuBufferRef& uniformBuffer = data.uniformBuffers[atlasIndex];

        if (!uniformBuffer)
        {
            uniformBuffer = RI.MakeGpuBuffer(GpuBufferType::ConstantBuffer, sizeof(LightmapVolumeUniforms));
            Check(uniformBuffer->Create());
        }

        uniformBuffer->Copy(sizeof(uniforms), &uniforms);

        // only draw elems in the volume with a stencil reference of the atlas index (+1)
        cr << SetStencilState(atlasIndex + 1, LightmapStencilMask, 0x0);

        uint32 localNumShaderUniforms = numShaderUniforms;

        cr << SetShaderUniform(localNumShaderUniforms++, "IrradianceTexture"_sh, RI.textureViewCache->GetOrCreate(irradianceTexture != nullptr ? irradianceTexture : RI.placeholderData->defaultTexture2d));
        cr << SetShaderUniform(localNumShaderUniforms++, "RadianceTexture"_sh, RI.textureViewCache->GetOrCreate(radianceTexture != nullptr ? radianceTexture : RI.placeholderData->defaultTexture2d));
        cr << SetShaderUniform(localNumShaderUniforms++, "LightmapSampler"_sh, RI.placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(localNumShaderUniforms++, "LightmapVolumeUniforms"_sh, uniformBuffer);

        RenderFullScreenQuad(frame, renderSetup);
    }

    // reset stencil state back to default
    cr << SetStencilState(0, 0xFF, 0x0);

    m_isFirstFrame = false;
}

#pragma endregion LightmapPass

} // namespace Hyperion
