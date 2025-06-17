/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/HBAO.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/Deferred.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/Mesh.hpp>

#include <system/AppContext.hpp>

#include <core/math/Vector2.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

struct HBAOUniforms
{
    Vec2u dimension;
    float radius;
    float power;
};

#pragma region Render commands

struct RENDER_COMMAND(CreateHBAOUniformBuffer)
    : renderer::RenderCommand
{
    HBAOUniforms uniforms;
    GPUBufferRef uniform_buffer;

    RENDER_COMMAND(CreateHBAOUniformBuffer)(
        const HBAOUniforms& uniforms,
        const GPUBufferRef& uniform_buffer)
        : uniforms(uniforms),
          uniform_buffer(uniform_buffer)
    {
        AssertThrow(uniforms.dimension.x * uniforms.dimension.y != 0);

        AssertThrow(this->uniform_buffer != nullptr);
    }

    virtual ~RENDER_COMMAND(CreateHBAOUniformBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create());
        uniform_buffer->Copy(sizeof(uniforms), &uniforms);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

HBAO::HBAO(HBAOConfig&& config, GBuffer* gbuffer)
    : FullScreenPass(InternalFormat::RGBA8, gbuffer),
      m_config(std::move(config))
{
}

HBAO::~HBAO()
{
}

void HBAO::Create()
{
    HYP_SCOPE;

    ShaderProperties shader_properties;
    shader_properties.Set("HBIL_ENABLED", g_engine->GetAppContext()->GetConfiguration().Get("rendering.hbil.enabled").ToBool());

    if (ShouldRenderHalfRes())
    {
        shader_properties.Set("HALFRES");
    }

    m_shader = g_shader_manager->GetOrCreate(NAME("HBAO"), shader_properties);

    FullScreenPass::Create();
}

void HBAO::CreatePipeline(const RenderableAttributeSet& renderable_attributes)
{
    HYP_SCOPE;

    const renderer::DescriptorTableDeclaration& descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("HBAODescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("UniformBuffer"), m_uniform_buffer);
    }

    DeferCreate(descriptor_table);

    m_descriptor_table = descriptor_table;

    m_render_group = CreateObject<RenderGroup>(
        m_shader,
        renderable_attributes,
        *m_descriptor_table,
        RenderGroupFlags::NONE);

    m_render_group->AddFramebuffer(m_framebuffer);

    InitObject(m_render_group);
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

    m_uniform_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::CONSTANT_BUFFER, sizeof(uniforms));

    PUSH_RENDER_COMMAND(CreateHBAOUniformBuffer, uniforms, m_uniform_buffer);
}

void HBAO::Resize_Internal(Vec2u new_size)
{
    HYP_SCOPE;

    SafeRelease(std::move(m_uniform_buffer));

    FullScreenPass::Resize_Internal(new_size);
}

void HBAO::Render(FrameBase* frame, const RenderSetup& render_setup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertDebug(render_setup.IsValid());
    AssertDebug(render_setup.HasView());

    const uint32 frame_index = frame->GetFrameIndex();

    Begin(frame, render_setup);

    frame->GetCommandList().Add<BindDescriptorTable>(
        GetRenderGroup()->GetPipeline()->GetDescriptorTable(),
        GetRenderGroup()->GetPipeline(),
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("WorldsBuffer"), ShaderDataOffset<WorldShaderData>(*render_setup.world) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*render_setup.view->GetCamera()) } } } },
        frame_index);

    const uint32 view_descriptor_set_index = GetRenderGroup()->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("View"));

    if (view_descriptor_set_index != ~0u)
    {
        AssertThrow(render_setup.HasView());

        frame->GetCommandList().Add<BindDescriptorSet>(
            render_setup.view->GetDescriptorSets()[frame->GetFrameIndex()],
            m_render_group->GetPipeline(),
            ArrayMap<Name, uint32> {},
            view_descriptor_set_index);
    }

    GetQuadMesh()->GetRenderResource().Render(frame->GetCommandList());
    End(frame, render_setup);
}

} // namespace hyperion