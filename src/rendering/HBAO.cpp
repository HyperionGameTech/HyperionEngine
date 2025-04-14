/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/HBAO.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderState.hpp>

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
    Vec2u   dimension;
    float   radius;
    float   power;
};

#pragma region Render commands

struct RENDER_COMMAND(AddHBAOResultToGlobalDescriptorSet) : renderer::RenderCommand
{
    ImageViewRef    image_view;

    RENDER_COMMAND(AddHBAOResultToGlobalDescriptorSet)(const ImageViewRef &image_view)
        : image_view(image_view)
    {
    }

    virtual ~RENDER_COMMAND(AddHBAOResultToGlobalDescriptorSet)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("SSAOResultTexture"), image_view);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RemoveHBAODescriptors) : renderer::RenderCommand
{
    RENDER_COMMAND(RemoveHBAODescriptors)()
    {
    }

    virtual ~RENDER_COMMAND(RemoveHBAODescriptors)() override = default;

    virtual RendererResult operator()() override
    {
        RendererResult result;

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("SSAOResultTexture"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        return result;
    }
};

struct RENDER_COMMAND(CreateHBAOUniformBuffer) : renderer::RenderCommand
{
    HBAOUniforms    uniforms;
    GPUBufferRef    uniform_buffer;

    RENDER_COMMAND(CreateHBAOUniformBuffer)(
        const HBAOUniforms &uniforms,
        const GPUBufferRef &uniform_buffer
    ) : uniforms(uniforms),
        uniform_buffer(uniform_buffer)
    {
        AssertThrow(uniforms.dimension.x * uniforms.dimension.y != 0);

        AssertThrow(this->uniform_buffer != nullptr);
    }

    virtual ~RENDER_COMMAND(CreateHBAOUniformBuffer)() override = default;

    virtual RendererResult operator()() override
    {
        HYPERION_BUBBLE_ERRORS(uniform_buffer->Create(
            g_engine->GetGPUDevice(),
            sizeof(uniforms)
        ));

        uniform_buffer->Copy(
            g_engine->GetGPUDevice(),
            sizeof(uniforms),
            &uniforms
        );

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

HBAO::HBAO(HBAOConfig &&config)
    : FullScreenPass(InternalFormat::RGBA8),
      m_config(std::move(config))
{
}

HBAO::~HBAO()
{
    PUSH_RENDER_COMMAND(RemoveHBAODescriptors);
}

void HBAO::Create()
{
    HYP_SCOPE;
    
    ShaderProperties shader_properties;
    shader_properties.Set("HBIL_ENABLED", g_engine->GetAppContext()->GetConfiguration().Get("rendering.hbil.enabled").ToBool());

    if (ShouldRenderHalfRes()) {
        shader_properties.Set("HALFRES");
    }

    m_shader = g_shader_manager->GetOrCreate(NAME("HBAO"), shader_properties);

    FullScreenPass::Create();

    PUSH_RENDER_COMMAND(AddHBAOResultToGlobalDescriptorSet, GetFinalImageView());
}

void HBAO::CreatePipeline(const RenderableAttributeSet &renderable_attributes)
{
    HYP_SCOPE;

    renderer::DescriptorTableDeclaration descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);
    
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("HBAODescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("UniformBuffer"), m_uniform_buffer);
    }
    
    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

    m_descriptor_table = descriptor_table;

    m_render_group = CreateObject<RenderGroup>(
        m_shader,
        renderable_attributes,
        *m_descriptor_table,
        RenderGroupFlags::NONE
    );

    m_render_group->AddFramebuffer(m_framebuffer);

    InitObject(m_render_group);
}

void HBAO::CreateDescriptors()
{
    CreateUniformBuffers();
}

void HBAO::CreateUniformBuffers()
{
    HBAOUniforms uniforms { };
    uniforms.dimension = ShouldRenderHalfRes() ? m_extent / 2 : m_extent;
    uniforms.radius = m_config.radius;
    uniforms.power = m_config.power;

    m_uniform_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::CONSTANT_BUFFER);

    PUSH_RENDER_COMMAND(CreateHBAOUniformBuffer, uniforms, m_uniform_buffer);
}

void HBAO::Resize_Internal(Vec2u new_size)
{
    HYP_SCOPE;

    SafeRelease(std::move(m_uniform_buffer));

    FullScreenPass::Resize_Internal(new_size);

    PUSH_RENDER_COMMAND(AddHBAOResultToGlobalDescriptorSet, GetFinalImageView());
}

void HBAO::Render(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    const SceneRenderResource *scene_render_resource = g_engine->GetRenderState()->GetActiveScene();
    const TResourceHandle<CameraRenderResource> &camera_resource_handle = g_engine->GetRenderState()->GetActiveCamera();

    {
        Begin(frame);

        frame->GetCommandList().Add<BindDescriptorTable>(
            GetRenderGroup()->GetPipeline()->GetDescriptorTable(),
            GetRenderGroup()->GetPipeline(),
            ArrayMap<Name, ArrayMap<Name, uint32>> {
                {
                    NAME("Scene"),
                    {
                        { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource) },
                        { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*camera_resource_handle) }
                    }
                }
            },
            frame_index
        );
        
        GetQuadMesh()->GetRenderResource().Render(frame->GetCommandList());
        End(frame);
    }
}

} // namespace hyperion