/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/passes/HBAOPass.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/Frame.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/Texture.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/View.hpp>

#include <system/AppContext.hpp>

#include <Core/math/Vector2.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/CVarManager.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

struct HBAOUniforms
{
    Vec2u dimension;
    float radius;
    float power;
};

static const ShaderPropertyId s_propHalfRes = InternShaderProperty(ShaderProperty(NAME("HALFRES")));

CVar<float> cvHBAORadius { "Rendering.HBAORadius", 1.5f, "Rendering.HBAO.Radius" };
CVar<float> cvHBAOPower { "Rendering.HBAOPower", 2.5f, "Rendering.HBAO.Power" };

HBAO::HBAO(Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TextureFormat::R16F, extent, gbuffer)
{
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
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    CommandRecorder& cr = frame->cr;

    if (!m_cbuffer)
    {
        HBAOUniforms constants {};
        constants.dimension = ShouldRenderHalfRes() ? m_extent / 2 : m_extent;
        constants.radius = cvHBAORadius.Get();
        constants.power = cvHBAOPower.Get();

        m_cbuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(constants));
        CheckResult(m_cbuffer->Create());

        m_cbuffer->Copy(sizeof(constants), &constants);
        m_cbuffer->Flush(0, sizeof(constants));
    }

    Begin(frame, renderSetup);

    DeferredRendererPassData* dpd = ObjCast<DeferredRendererPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    const FramebufferRef& inputsFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RenderBucket::Opaque);
    AssertDebug(inputsFramebuffer.IsValid());

    ShaderPropertySet shaderProperties;

    if (ShouldRenderHalfRes())
    {
        shaderProperties.Add(s_propHalfRes);
    }

    cr << SetCurrentShader(ShaderDesc(NAME("HBAO"), shaderProperties));

    uint32 numShaderUniforms = 0;

    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinearMipmap());

    cr << SetShaderUniform(numShaderUniforms++, "GBufferAlbedoTexture"_sh, inputsFramebuffer->GetAttachment(GTN_ALBEDO)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferNormalsTexture"_sh, inputsFramebuffer->GetAttachment(GTN_NORMALS)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferMaterialTexture"_sh, inputsFramebuffer->GetAttachment(GTN_MATERIAL)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferVelocityTexture"_sh, inputsFramebuffer->GetAttachment(GTN_VELOCITY)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferDepthTexture"_sh, inputsFramebuffer->GetAttachment(GTN_DEPTH)->GetImageView());

    cr << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->mipChain));

    cr << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frameIndex), TShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()));
    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));

    cr << SetShaderUniform(numShaderUniforms++, "UniformBuffer"_sh, m_cbuffer);

    RenderFullScreenQuad(frame, renderSetup);

    End(frame, renderSetup);
}

} // namespace Hyperion
