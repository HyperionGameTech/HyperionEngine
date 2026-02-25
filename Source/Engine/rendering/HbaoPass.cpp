/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/HbaoPass.hpp>
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

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/View.hpp>

#include <system/AppContext.hpp>

#include <Core/math/Vector2.hpp>

#include <Core/config/Config.hpp>

#include <engine/EngineDriver.hpp>

#include <HbaoPass.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

namespace CoreApi {
extern const GlobalConfig& GetGlobalConfig();
} // namespace CoreApi

struct HBAOUniforms
{
    Vec2u dimension;
    float radius;
    float power;
};

static const ShaderPropertyId s_propHbilEnabled = InternShaderProperty(ShaderProperty(NAME("HBIL_ENABLED")));
static const ShaderPropertyId s_propHalfRes = InternShaderProperty(ShaderProperty(NAME("HALFRES")));

#pragma region Render commands

struct CreateHBAOUniformBuffer : RenderCommand
{
    HBAOUniforms uniforms;
    GpuBufferRef uniformBuffer;

    CreateHBAOUniformBuffer(
        const HBAOUniforms& uniforms,
        const GpuBufferRef& uniformBuffer)
        : uniforms(uniforms),
          uniformBuffer(uniformBuffer)
    {
        Assert(uniforms.dimension.x * uniforms.dimension.y != 0);

        Assert(this->uniformBuffer != nullptr);
    }

    virtual ~CreateHBAOUniformBuffer() override = default;

    virtual RendererResult operator()() override
    {
        CheckResultOrReturn(uniformBuffer->Create());
        uniformBuffer->Copy(sizeof(uniforms), &uniforms);

        return {};
    }
};

#pragma endregion Render commands

HBAO::HBAO(HBAOConfig&& config, Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TextureFormat::RGBA8, extent, gbuffer),
      m_config(std::move(config))
{
}

HBAO::~HBAO()
{
    EnqueueDeletion(std::move(m_uniformBuffer));
    EnqueueDeletion(std::move(m_descriptorSet));
}

void HBAO::CreateUniformBuffers()
{
    HBAOUniforms uniforms {};
    uniforms.dimension = ShouldRenderHalfRes() ? m_extent / 2 : m_extent;
    uniforms.radius = m_config.radius;
    uniforms.power = m_config.power;

    m_uniformBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(uniforms));

    PUSH_RENDER_COMMAND(CreateHBAOUniformBuffer, uniforms, m_uniformBuffer);
}

void HBAO::Resize_Internal(Vec2u newSize)
{
    HYP_SCOPE;

    EnqueueDeletion(std::move(m_uniformBuffer));
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

    RenderQueue& rq = frame->renderQueue;

    Begin(frame, renderSetup);
    
    ShaderPropertySet shaderProperties;
    shaderProperties.Set(s_propHbilEnabled, CoreApi::GetGlobalConfig().Get("Rendering.HBIL.Enabled").ToBool());

    if (ShouldRenderHalfRes())
    {
        shaderProperties.Add(s_propHalfRes);
    }

    rq << SetCurrentShader(ShaderDesc(NAME("HBAO"), shaderProperties));

    rq << SetShaderUniform(6, "UniformBuffer"_sh, m_uniformBuffer);
    
    RenderFullScreenQuad(frame, renderSetup);

    End(frame, renderSetup);
}

} // namespace Hyperion
