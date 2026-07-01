/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/HBAOPass.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/GraphicsPipelineCache.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/GraphicsPipeline.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/Texture.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/View.hpp>

#include <System/AppContext.hpp>

#include <Core/Math/Vector2.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/CVarManager.hpp>
#include <Framework/EngineStats.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

struct HBAOUniforms
{
    Vec2u dimension;
    float radius;
    float power;
};

static const ShaderPropertyId s_propHalfRes = InternShaderProperty(ShaderProperty(NAME("HALFRES")));

static EngineStatGpuTimer s_statHBAOPass("Rendering/GPU/HBAO");

CVar<float> cvHBAORadius { "Rendering.HBAORadius", 1.5f, "Rendering.HBAO.Radius" };
CVar<float> cvHBAOPower { "Rendering.HBAOPower", 2.5f, "Rendering.HBAO.Power" };

HBAO::HBAO(Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TextureFormat::R16F, extent, gbuffer)
{
    SetPassName(NAME("HBAO"));
}

HBAO::~HBAO()
{
    EnqueueDeletion(std::move(m_cbuffer));
    EnqueueDeletion(std::move(m_descriptorSet));
}

void HBAO::Resize_Internal(Vec2u newSize)
{
    HYP_SCOPE;

    EnqueueDeletion(std::move(m_cbuffer));
    EnqueueDeletion(std::move(m_descriptorSet));

    FullScreenPass::Resize_Internal(newSize);
}

void HBAO::Render(Frame* frame, const RenderSetup& renderSetup)
{
    AssertOnThread(g_renderThread);

    ENGINE_STAT_GPU_SCOPE(&s_statHBAOPass);

    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr);

    CommandRecorder& cr = frame->cr;

    if (!m_cbuffer)
    {
        HBAOUniforms constants {};
        constants.dimension = ShouldRenderHalfRes() ? m_extent / 2 : m_extent;
        constants.radius = cvHBAORadius.Get();
        constants.power = cvHBAOPower.Get();

        m_cbuffer = RI.MakeGpuBuffer(GpuBufferType::ConstantBuffer, sizeof(constants));
        CheckResult(m_cbuffer->Create());

        m_cbuffer->Copy(sizeof(constants), &constants);
        m_cbuffer->Flush(0, sizeof(constants));
    }

    ShaderPropertySet shaderProperties;

    if (ShouldRenderHalfRes())
    {
        shaderProperties.Add(s_propHalfRes);
    }

    m_shaderDesc = ShaderDesc(NAME("HBAO"), shaderProperties);

    Begin(frame, renderSetup);

    DeferredPassData* dpd = DynamicCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    const FramebufferRef& inputsFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);
    AssertDebug(inputsFramebuffer.IsValid());

    uint32 numShaderUniforms = 0;

    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());

    cr << SetShaderUniform(numShaderUniforms++, "GBufferAlbedoTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Albedo)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferNormalsTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferMaterialTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::MatData)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferVelocityTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Velocity)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferDepthTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());

    cr << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, RI.textureViewCache->GetOrCreate(dpd->mipChain));

    cr << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));
    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    cr << SetShaderUniform(numShaderUniforms++, "UniformBuffer"_sh, m_cbuffer);

    RenderFullScreenQuad(frame, renderSetup);

    End(frame, renderSetup);
}

} // namespace Hyperion
