/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/HbaoPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/View.hpp>

#include <system/AppContext.hpp>

#include <core/math/Vector2.hpp>

#include <core/config/Config.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

extern const GlobalConfig& CoreApi_GetGlobalConfig();

struct HBAOUniforms
{
    Vec2u dimension;
    float radius;
    float power;
};

#pragma region Render commands

struct RENDER_COMMAND(CreateHBAOUniformBuffer)
    : RenderCommand
{
    HBAOUniforms uniforms;
    GpuBufferRef uniformBuffer;

    RENDER_COMMAND(CreateHBAOUniformBuffer)(
        const HBAOUniforms& uniforms,
        const GpuBufferRef& uniformBuffer)
        : uniforms(uniforms),
          uniformBuffer(uniformBuffer)
    {
        Assert(uniforms.dimension.x * uniforms.dimension.y != 0);

        Assert(this->uniformBuffer != nullptr);
    }

    virtual ~RENDER_COMMAND(CreateHBAOUniformBuffer)() override = default;

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
}

void HBAO::Create()
{
    HYP_SCOPE;

    ShaderProperties shaderProperties;
    shaderProperties.Set(NAME("HBIL_ENABLED"), CoreApi_GetGlobalConfig().Get("Rendering.HBIL.Enabled").ToBool());

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

    const DescriptorTableDeclaration& descriptorTableDecl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("HBAODescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("UniformBuffer", m_uniformBuffer);
    }

    DeferCreate(descriptorTable);

    m_descriptorTable = descriptorTable;

    m_graphicsPipelineCacheHandle = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
        m_shader,
        descriptorTable,
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

    FullScreenPass::Resize_Internal(newSize);
}

void HBAO::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    const uint32 frameIndex = frame->GetFrameIndex();

    Begin(frame, renderSetup);

    const GraphicsPipelineRef& graphicsPipeline = GetGraphicsPipeline();

    frame->renderQueue << BindDescriptorTable(
        graphicsPipeline->GetDescriptorTable(),
        graphicsPipeline,
        { { "Global", { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) } } } },
        frameIndex);

    const uint32 viewDescriptorSetIndex = graphicsPipeline->GetDescriptorTable()->GetDescriptorSetIndex("View");

    if (viewDescriptorSetIndex != ~0u)
    {
        Assert(renderSetup.HasView());
        Assert(renderSetup.passData != nullptr);

        frame->renderQueue << BindDescriptorSet(
            renderSetup.passData->descriptorSets[frame->GetFrameIndex()],
            graphicsPipeline,
            {},
            viewDescriptorSetIndex);
    }

    frame->renderQueue << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
    frame->renderQueue << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
    frame->renderQueue << DrawIndexed(6);

    End(frame, renderSetup);
}

} // namespace hyperion
