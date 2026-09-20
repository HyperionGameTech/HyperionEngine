/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/HeightFogPass.hpp>
#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderSetup.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Vertex.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/StencilMasks.hpp>

#include <Rendering/Util/MeshBuilder.hpp>

#include <Scene/View.hpp>
#include <Scene/Light.hpp>
#include <Scene/EnvProbe.hpp>

#include <Framework/EngineStats.hpp>

#include <Core/Utilities/DeferredScope.hpp>

namespace Hyperion {

static EngineStatGpuTimer s_statHeightFog("Rendering/GPU/HeightFog");

// matches the tail of HeightFogConstants in Shaders/HeightFog.hlsl
struct HeightFogConstants
{
    uint32 hasSun;
    uint32 hasSkyProbe;
    uint32 _pad0;
    uint32 _pad1;
};

HeightFogPass::HeightFogPass() = default;

HeightFogPass::~HeightFogPass() = default;

void HeightFogPass::Create()
{
    m_quadMesh = MeshBuilder::Quad();
    m_quadMesh->SetName(NAME("HeightFogQuad"));
    m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
    m_quadMesh->SetIsTransient(true);
    m_quadMesh->UploadGpuData();
}

void HeightFogPass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    AssertDebug(renderSetup.view != nullptr && renderSetup.framebuffer != nullptr);

    if (!(GetWorldBufferData()->environmentFlags & uint32(WorldEnvironmentFlags::HeightFog)))
    {
        return;
    }

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));

    if (!cameraProxy)
    {
        return;
    }

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    ENGINE_STAT_GPU_SCOPE(&s_statHeightFog);

    HeightFogConstants constants {};

    LightShaderData sunShaderData {};

    for (Light* light : rpl.GetLights())
    {
        if (light->GetLightType() != LightType::Directional)
        {
            continue;
        }

        if (RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light)))
        {
            sunShaderData = lightProxy->bufferData;
            constants.hasSun = 1;

            break;
        }
    }

    EnvProbeShaderData skyProbeShaderData {};

    const auto& skyProbes = rpl.GetEnvProbes().GetElements<SkyProbe>();

    if (skyProbes.Any())
    {
        if (RenderProxyEnvProbe* skyProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(*skyProbes.Begin())))
        {
            // the probe's spherical harmonics light the fog even while its cubemap isn't resident
            skyProbeShaderData = skyProbeProxy->bufferData;
            constants.hasSkyProbe = 1;
        }
    }

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&cameraProxy->bufferData);
    RI.cbufferAllocator->Write(&sunShaderData);
    RI.cbufferAllocator->Write(&skyProbeShaderData);
    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    const FramebufferRef& opaqueFramebuffer = renderSetup.view->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);

    CommandRecorder& cr = frame->cr;

    cr << SetCurrentFramebuffer(renderSetup.framebuffer);
    cr << SetCurrentViewport(renderSetup.viewport);

    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
    cr << SetFaceCullMode(FaceCullMode::Back);
    cr << SetFillMode(FillMode::Fill);
    cr << SetTopology(Topology::Triangles);
    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);

    // geometry only
    cr << SetStencilTest(true);
    cr << SetStencilFunction(StencilFunction { StencilOp::Keep, StencilOp::Keep, StencilOp::Keep, StencilCompareOp::Equal });
    cr << SetStencilState(0, SkyStencilMask, 0x0);

    // premultiplied: color = inscatter + scene * transmittance, destination alpha untouched
    cr << SetCurrentBlendFunction(BlendFunction(BlendModeFactor::One, BlendModeFactor::SrcAlpha, BlendModeFactor::Zero, BlendModeFactor::One));

    cr << SetCurrentShader(ShaderDesc(NAME("HeightFog")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, opaqueFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
    cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
    cr << SetShaderUniform(uniformIndex++, "HeightFogConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << CommitDrawState();

    cr << BindVertexBuffer(m_quadMesh->GetVertexBuffer(0));
    cr << BindIndexBuffer(m_quadMesh->GetIndexBuffer(0));

    cr << DrawIndexed(6);

    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);
    cr << SetStencilTest(false);
    cr << SetCurrentBlendFunction(BlendFunction::None());

    cr << SetCurrentFramebuffer(nullptr);
}

} // namespace Hyperion
