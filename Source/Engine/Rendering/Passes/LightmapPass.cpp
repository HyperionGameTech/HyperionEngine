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
#include <Rendering/Mesh.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/StencilMasks.hpp>
#include <Rendering/CBufferAllocator.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/MeshBuilder.hpp>

#include <Scene/View.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Core/Math/Mat4f.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Framework/CVarManager.hpp>

namespace Hyperion {

extern CVar<bool> g_cvHBAO;

struct LightmapVolumeUniforms
{
    Mat4f transformMatrix;
    Vec4f aabbMin;
    Vec4f aabbMax;
    float irradianceWeight;
    uint32 numAtlases;
    uint32 _pad0;
    uint32 _pad1;
};

static_assert(sizeof(LightmapVolumeUniforms) % 16 == 0);

#pragma region LightmapPass

LightmapPass::LightmapPass()
    : FullScreenPass(TextureFormat::RGBA16F, nullptr, FSP_EXTERNAL_RENDERTARGET)
{
    SetPassName(NAME("Lightmap"));
    m_shaderDesc = ShaderDesc(NAME("ApplyLightmap"));
}

LightmapPass::~LightmapPass()
{
}

void LightmapPass::Create()
{
    AssertOnThread(g_renderThread);

    m_volumeMesh = MeshBuilder::Cube();
    m_volumeMesh->SetIsTransient(true);
    m_volumeMesh->SetFlags(MeshFlags::ViewIndependent);
    m_volumeMesh->SetName(NAME("LightmapVolumeMesh"));
    m_volumeMesh->UploadGpuData();

    FullScreenPass::Create();
}

void LightmapPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void LightmapPass::RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer)
{
    AssertDebug(renderSetup.world && renderSetup.view);

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
    Assert(cameraProxy != nullptr);

    DeferredPassData* dpd = DynamicCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    Framebuffer* viewFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);
    AssertDebug(viewFramebuffer != nullptr);

    CommandRecorder& cr = frame->cr;

    cr << SetCurrentShader(m_shaderDesc);

    cr << SetCurrentViewport(renderSetup.viewport);

    cr << SetInputLayout(m_volumeMesh->GetMeshAttributes().inputLayout);
    cr << SetTopology(m_volumeMesh->GetMeshAttributes().topology);
    // Cull front faces so the volume still covers the screen when the camera is inside it.
    //cr << SetFaceCullMode(FCM_FRONT);
    cr << SetFillMode(FM_FILL);

    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);

    cr << SetCurrentBlendFunction(BlendFunction::Additive());

    HYP_DEFER({
        // reset states
        cr << SetCurrentBlendFunction(BlendFunction::None());
        cr << SetDepthWrite(true);
        cr << SetDepthTest(true);
    });

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

    // Env probes
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesDepthTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesDepthTexture));

    if (renderSetup.envProbe != nullptr)
    {
        cr << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, RI.namedBuffers[NamedBuffer::EnvProbes], Resources::GetBinding(renderSetup.envProbe));
    }
    else
    {
        cr << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, RI.namedBuffers[NamedBuffer::EnvProbes], 0);
    }

    if (dpd->hbao != nullptr && g_cvHBAO.Get())
    {
        cr << SetShaderUniform(numShaderUniforms++, "SSAOResultTexture"_sh, dpd->hbao->GetFinalImageView());
    }
    else
    {
        cr << SetShaderUniform(numShaderUniforms++, "SSAOResultTexture"_sh, RI.textureViewCache->GetOrCreate(RI.placeholderData->textureSolidWhite));
    }

    for (LightmapVolume* lmv : rpl.GetLightmapVolumes())
    {
        RenderProxyLightmapVolume* proxy = static_cast<RenderProxyLightmapVolume*>(GetRenderProxy(lmv));
        Assert(proxy != nullptr);

        if (proxy->numAtlases == 0)
        {
            continue; // nothing to do
        }

        LightmapVolumePassData& data = GetLightmapVolumePassData(lmv);

        for (uint32 atlasIndex = 0; atlasIndex < proxy->numAtlases; atlasIndex++)
        {
            Texture* irradianceTexture = proxy->atlasIrradianceTextures[atlasIndex];

            LightmapVolumeUniforms uniforms {};
            uniforms.transformMatrix = proxy->transformMatrix;
            uniforms.aabbMin = Vec4f(proxy->worldAabb.min, 1.0f);
            uniforms.aabbMax = Vec4f(proxy->worldAabb.max, 1.0f);
            uniforms.numAtlases = proxy->numAtlases;
            uniforms.irradianceWeight = irradianceTexture ? 1.0f : 0.0f;

            GpuBuffer* cbuffer;
            size_t cbufferOffset;
            size_t cbufferSize;

            RI.cbufferAllocator->Write(&cameraProxy->bufferData);
            RI.cbufferAllocator->Write(&uniforms);

            RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

            uint32 localNumShaderUniforms = numShaderUniforms;

            cr << SetShaderUniform(localNumShaderUniforms++, "IrradianceTexture"_sh, RI.textureViewCache->GetOrCreate(irradianceTexture != nullptr ? irradianceTexture : RI.placeholderData->textureSolidBlack));
            cr << SetShaderUniform(localNumShaderUniforms++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

            cr << CommitDrawState();

            cr << BindVertexBuffer(m_volumeMesh->GetVertexBuffer());
            cr << BindIndexBuffer(m_volumeMesh->GetIndexBuffer());
            cr << DrawIndexed(36); // draw cube
        }
    }

    // reset stencil state back to default
    cr << SetStencilState(0, 0xFF, 0x0);

    m_isFirstFrame = false;
}

#pragma endregion LightmapPass

} // namespace Hyperion
