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
#include <Rendering/CBufferAllocator.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/View.hpp>

#include <System/AppContext.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/MathUtil.hpp>

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

static EngineStatGpuTimer s_statHBAOPass("Rendering/GPU/HBAO");

CVar<float> cvHBAORadius { "Rendering.HBAORadius", 5.0f, "Rendering.HBAO.Radius" };
CVar<float> cvHBAOPower { "Rendering.HBAOPower", 2.0f, "Rendering.HBAO.Power" };

HBAO::HBAO(Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TextureFormat::R16F, extent, gbuffer)
{
    SetPassName(NAME("HBAO"));

    m_shaderDesc = ShaderDesc(NAME("HBAO"));
}

HBAO::~HBAO()
{
}

void HBAO::Create()
{
    FullScreenPass::Create();

    m_upsamplePass = MakeUnique<FullScreenPass>(
        TextureFormat::R16F,
        m_extent,
        nullptr,
        FSP_NONE);

    m_upsamplePass->SetShaderDesc(ShaderDesc(NAME("Upsample"), ShaderPropertySet {}));
    m_upsamplePass->Create();
}

void HBAO::Resize_Internal(Vec2u newSize)
{
    HYP_SCOPE;

    FullScreenPass::Resize_Internal(newSize);

    if (m_upsamplePass != nullptr)
    {
        m_upsamplePass->Resize(newSize);
    }
}

const GpuImageViewRef& HBAO::GetFinalImageView() const
{
    if (m_upsamplePass != nullptr)
    {
        return m_upsamplePass->GetFinalImageView();
    }

    return FullScreenPass::GetFinalImageView();
}

void HBAO::Render(Frame* frame, const RenderSetup& renderSetup)
{
    AssertOnThread(g_renderThread);

    ENGINE_STAT_GPU_SCOPE(&s_statHBAOPass);

    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr);

    CommandRecorder& cr = frame->cr;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    HBAOUniforms constants {};
    constants.dimension = m_extent / 2;
    constants.radius = cvHBAORadius.Get();
    constants.power = cvHBAOPower.Get();

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    Begin(frame, renderSetup);

    DeferredPassData* dpd = DynamicCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    const FramebufferRef& inputsFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);
    AssertDebug(inputsFramebuffer.IsValid());

    uint32 numShaderUniforms = 0;

    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());

    cr << SetShaderUniform(numShaderUniforms++, "GBufferAlbedoTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Color)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferNormalsTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferMaterialTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::MatData)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferVelocityTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Velocity)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferDepthTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());

    cr << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, RI.textureViewCache->GetOrCreate(dpd->mipChain));

    cr << SetShaderUniform(numShaderUniforms++, "BlueNoiseBuffer"_sh, RI.blueNoiseBuffer);

    cr << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));
    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    cr << SetShaderUniform(numShaderUniforms++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    RenderFullScreenQuad(frame, renderSetup);

    End(frame, renderSetup);

    AssertDebug(m_upsamplePass != nullptr);

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
    AssertDebug(cameraProxy != nullptr);

    GpuBuffer* upsampleCbuffer = nullptr;
    size_t upsampleCbufferOffset = 0;
    size_t upsampleCbufferSize = 0;

    {
        struct UpsampleConstants
        {
            CameraShaderData camera;

            Vec2f texelSize;
            Vec2f uvScale;
            float depthThreshold;
            float normalThreshold;
        };

        const Vec2f sourceResolution = Vec2f(MathUtil::Max(m_extent / 2, Vec2u::One()));

        UpsampleConstants upsampleConstants {};
        upsampleConstants.camera = cameraProxy->bufferData;
        upsampleConstants.texelSize = Vec2f::One() / sourceResolution;
        upsampleConstants.uvScale = Vec2f::One();
        upsampleConstants.depthThreshold = 0.1f;
        upsampleConstants.normalThreshold = 1.0f;

        RI.cbufferAllocator->Write(&upsampleConstants);
        RI.cbufferAllocator->Commit(upsampleCbuffer, upsampleCbufferOffset, upsampleCbufferSize);
    }

    cr << SetCurrentShader(m_upsamplePass->GetShaderDesc());
    cr << SetCurrentFramebuffer(m_upsamplePass->GetFramebuffer());
    cr << SetCurrentViewport(Viewport { m_upsamplePass->GetExtent() });

    uint32 numUpsampleUniforms = 0;

    cr << SetShaderUniform(numUpsampleUniforms++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
    cr << SetShaderUniform(numUpsampleUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());

    cr << SetShaderUniform(numUpsampleUniforms++, "NormalsTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
    cr << SetShaderUniform(numUpsampleUniforms++, "DepthTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());

    cr << SetShaderUniform(numUpsampleUniforms++, "PrevPassTexture"_sh, GetAttachment(0)->GetImageView());

    cr << SetShaderUniform(numUpsampleUniforms++, "CBuffer"_sh, upsampleCbuffer, ShaderDataOffset(upsampleCbufferOffset, upsampleCbufferSize));

    cr << CommitDrawState();

    m_upsamplePass->RenderFullScreenQuad(frame, renderSetup);
}

} // namespace Hyperion
