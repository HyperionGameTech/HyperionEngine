/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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

#include <rendering/util/SafeDeleter.hpp>

#include <scene/View.hpp>

#include <system/AppContext.hpp>

#include <core/math/Vector2.hpp>

#include <core/config/Config.hpp>

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
        HYP_GFX_CHECK(uniformBuffer->Create());
        uniformBuffer->Copy(sizeof(uniforms), &uniforms);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

HBAO::HBAO(HBAOConfig&& config, Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TF_RGBA8, extent, gbuffer),
      m_config(std::move(config))
{
}

HBAO::~HBAO()
{
    SafeDelete(std::move(m_uniformBuffer));
    SafeDelete(std::move(m_descriptorSet));
}

void HBAO::Create()
{
    HYP_SCOPE;

    ShaderProperties shaderProperties;
    shaderProperties.Set(NAME("HBIL_ENABLED"), CoreApi::GetGlobalConfig().Get("Rendering.HBIL.Enabled").ToBool());

    if (ShouldRenderHalfRes())
    {
        shaderProperties.Set(NAME("HALFRES"));
    }

    m_shader = g_shaderManager->GetOrCreate(NAME("HBAO"), shaderProperties);

    FullScreenPass::Create();
}

void HBAO::CreatePipeline(const RenderableAttributeSet& renderableAttributes)
{
    HYP_SCOPE;

    const DescriptorTableDeclaration* descriptorTableDecl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();
    Assert(descriptorTableDecl != nullptr);

    const DescriptorSetDeclaration* descriptorSetDecl = descriptorTableDecl->FindDescriptorSetDeclaration("HBAODescriptorSet"_sh);
    Assert(descriptorSetDecl != nullptr);

    m_descriptorSet = g_renderBackend->MakeDescriptorSet(DescriptorSetLayout(descriptorSetDecl));
    Assert(m_descriptorSet != nullptr);

    m_descriptorSet->SetElement("UniformBuffer"_sh, m_uniformBuffer);

    Assert(m_descriptorSet->Create());

    m_graphicsPipelineCacheHandle = g_renderInterface->graphicsPipelineCache->GetOrCreate(
        m_shader,
        { &m_framebuffer, 1 },
        renderableAttributes);
}

void HBAO::CreateDescriptors()
{
    CreateUniformBuffers();
}

void HBAO::CreateUniformBuffers()
{
    HBAOUniforms uniforms {};
    uniforms.dimension = ShouldRenderHalfRes() ? m_extent / 2 : m_extent;
    uniforms.radius = m_config.radius;
    uniforms.power = m_config.power;

    m_uniformBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(uniforms));

    PUSH_RENDER_COMMAND(CreateHBAOUniformBuffer, uniforms, m_uniformBuffer);
}

void HBAO::Resize_Internal(Vec2u newSize)
{
    HYP_SCOPE;

    SafeDelete(std::move(m_uniformBuffer));
    SafeDelete(std::move(m_descriptorSet));

    FullScreenPass::Resize_Internal(newSize);
}

void HBAO::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    Begin(frame, renderSetup);

    const GraphicsPipelineRef& graphicsPipeline = GetGraphicsPipeline();

    const uint32 descriptorSetIndex = graphicsPipeline->GetDescriptorSetIndex("HBAODescriptorSet"_sh);
    AssertDebug(descriptorSetIndex != ~0u);

    const uint32 globalDescriptorSetIndex = graphicsPipeline->GetDescriptorSetIndex("Global"_sh);
    AssertDebug(globalDescriptorSetIndex != ~0u);

    const uint32 viewDescriptorSetIndex = graphicsPipeline->GetDescriptorSetIndex("View"_sh);

    frame->renderQueue << BindDescriptorSet(
        g_renderInterface->globalDescriptorTable->GetDescriptorSet("Global"_sh, frameIndex),
        graphicsPipeline,
        { { "CamerasBuffer"_sh, ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) } },
        globalDescriptorSetIndex);

    frame->renderQueue << BindDescriptorSet(
        m_descriptorSet,
        graphicsPipeline,
        {},
        descriptorSetIndex);

    frame->renderQueue << BindDescriptorSet(
        renderSetup.passData->descriptorSets[frame->GetFrameIndex()],
        graphicsPipeline,
        {},
        viewDescriptorSetIndex);

    frame->renderQueue << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
    frame->renderQueue << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
    frame->renderQueue << DrawIndexed(6);

    End(frame, renderSetup);
}

} // namespace Hyperion
