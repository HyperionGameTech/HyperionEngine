/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Camera.hpp>
#include <rendering/Scene.hpp>
#include <rendering/EnvGrid.hpp>

#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <Engine.hpp>
#include <Types.hpp>

#include <util/MeshBuilder.hpp>

namespace hyperion {

using renderer::CommandBufferType;
using renderer::FillMode;

#pragma region Render commands

struct RENDER_COMMAND(CreateCommandBuffers) : renderer::RenderCommand
{
    FixedArray<CommandBufferRef, max_frames_in_flight>  command_buffers;

    RENDER_COMMAND(CreateCommandBuffers)(const FixedArray<CommandBufferRef, max_frames_in_flight> &command_buffers)
        : command_buffers(command_buffers)
    {
    }

    virtual ~RENDER_COMMAND(CreateCommandBuffers)() override = default;

    virtual Result operator()() override
    {
        for (uint i = 0; i < max_frames_in_flight; i++) {
#ifdef HYP_VULKAN
            command_buffers[i]->GetPlatformImpl().command_pool = g_engine->GetGPUDevice()->GetGraphicsQueue().command_pools[0];
#endif

            HYPERION_BUBBLE_ERRORS(command_buffers[i]->Create(g_engine->GetGPUDevice()));
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RecreateFullScreenPassFramebuffer) : renderer::RenderCommand
{
    FullScreenPass  &full_screen_pass;
    Vec2u           new_size;

    RENDER_COMMAND(RecreateFullScreenPassFramebuffer)(FullScreenPass &full_screen_pass, Vec2u new_size)
        : full_screen_pass(full_screen_pass),
          new_size(new_size)
    {
    }

    virtual ~RENDER_COMMAND(RecreateFullScreenPassFramebuffer)() override = default;

    virtual Result operator()() override
    {
        if (full_screen_pass.m_is_initialized) {
            full_screen_pass.Resize_Internal(new_size);
        } else {
            full_screen_pass.m_extent = new_size;
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

FullScreenPass::FullScreenPass()
    : FullScreenPass(InternalFormat::NONE, Vec2u::Zero())
{
}

FullScreenPass::FullScreenPass(InternalFormat image_format, Vec2u extent)
    : FullScreenPass(nullptr, image_format, extent)
{
}

FullScreenPass::FullScreenPass(
    const ShaderRef &shader,
    const DescriptorTableRef &descriptor_table,
    InternalFormat image_format,
    Vec2u extent
) : FullScreenPass(
        shader,
        image_format,
        extent
    )
{
    m_descriptor_table.Set(descriptor_table);
}

FullScreenPass::FullScreenPass(
    const ShaderRef &shader,
    InternalFormat image_format,
    Vec2u extent
) : m_shader(shader),
    m_image_format(image_format),
    m_extent(extent),
    m_blend_function(BlendFunction::None()),
    m_is_initialized(false)
{
}

FullScreenPass::~FullScreenPass()
{
    if (m_framebuffer.IsValid() && m_render_group.IsValid()) {
        m_render_group->RemoveFramebuffer(m_framebuffer);
    }

    m_render_group.Reset();
    m_full_screen_quad.Reset();

    SafeRelease(std::move(m_framebuffer));
    SafeRelease(std::move(m_command_buffers));

    if (m_is_initialized) {
        // Prevent dangling reference from render commands
        HYP_SYNC_RENDER();
    }
}

void FullScreenPass::Create()
{
    AssertThrow(!m_is_initialized);

    AssertThrowMsg(
        m_image_format != InternalFormat::NONE,
        "Image format must be set before creating the full screen pass"
    );

    CreateQuad();
    CreateCommandBuffers();
    CreateFramebuffer();
    CreatePipeline();
    CreateDescriptors();

    m_is_initialized = true;
}

void FullScreenPass::SetShader(const ShaderRef &shader)
{
    if (m_shader == shader) {
        return;
    }

    m_shader = shader;
}

const AttachmentRef &FullScreenPass::GetAttachment(uint attachment_index) const
{
    AssertThrow(m_framebuffer.IsValid());

    return m_framebuffer->GetAttachment(attachment_index);
}

void FullScreenPass::SetBlendFunction(const BlendFunction &blend_function)
{
    m_blend_function = blend_function;
}

void FullScreenPass::Resize(Vec2u new_size)
{
    PUSH_RENDER_COMMAND(RecreateFullScreenPassFramebuffer, *this, new_size);
}

void FullScreenPass::Resize_Internal(Vec2u new_size)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    if (m_extent == new_size) {
        return;
    }

    if (!m_framebuffer.IsValid()) {
        // Not created yet; skip
        return;
    }

    // if (m_render_group.IsValid()) {
    //     m_render_group->RemoveFramebuffer(m_framebuffer);
    // }

    SafeRelease(std::move(m_framebuffer));

    if (new_size.x * new_size.y == 0) {
        new_size = g_engine->GetGPUInstance()->GetSwapchain()->extent;
    }

    m_extent = new_size;

    CreateFramebuffer();
}

void FullScreenPass::CreateQuad()
{
    m_full_screen_quad = MeshBuilder::Quad();
    InitObject(m_full_screen_quad);

    // Allow Render() to be called directly without a RenderGroup
    m_full_screen_quad->SetPersistentRenderResourcesEnabled(true);
}

void FullScreenPass::CreateCommandBuffers()
{
    for (uint i = 0; i < max_frames_in_flight; i++) {
        m_command_buffers[i] = MakeRenderObject<CommandBuffer>(CommandBufferType::COMMAND_BUFFER_SECONDARY);
    }

    // create command buffers in render thread
    PUSH_RENDER_COMMAND(CreateCommandBuffers, m_command_buffers);
}

void FullScreenPass::CreateFramebuffer()
{
    if (m_extent.x * m_extent.y == 0) {
        // TODO: Make non render-thread dependent
        m_extent = g_engine->GetGPUInstance()->GetSwapchain()->extent;
    }

    m_framebuffer = MakeRenderObject<Framebuffer>(
        m_extent,
        renderer::RenderPassStage::SHADER,
        renderer::RenderPassMode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    ImageRef attachment_image = MakeRenderObject<Image>(renderer::FramebufferImage2D(
        m_extent,
        m_image_format
    ));

    attachment_image->SetIsAttachmentTexture(true);

    DeferCreate(attachment_image, g_engine->GetGPUDevice());

    AttachmentRef attachment = MakeRenderObject<Attachment>(
        attachment_image,
        renderer::RenderPassStage::SHADER,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    attachment->SetBinding(0);

    if (m_blend_function != BlendFunction::None()) {
        attachment->SetAllowBlending(true);
    }

    DeferCreate(attachment, g_engine->GetGPUDevice());
    
    m_framebuffer->AddAttachment(attachment);

    DeferCreate(m_framebuffer, g_engine->GetGPUDevice());
}

void FullScreenPass::CreatePipeline()
{
    CreatePipeline(RenderableAttributeSet(
        MeshAttributes {
            .vertex_attributes = static_mesh_vertex_attributes
        },
        MaterialAttributes {
            .fill_mode      = FillMode::FILL,
            .blend_function = m_blend_function,
            .flags          = MaterialAttributeFlags::NONE
        }
    ));
}

void FullScreenPass::CreatePipeline(const RenderableAttributeSet &renderable_attributes)
{
    if (m_descriptor_table.HasValue()) {
        m_render_group = CreateObject<RenderGroup>(
            m_shader,
            renderable_attributes,
            *m_descriptor_table,
            RenderGroupFlags::NONE
        );
    } else {
        m_render_group = CreateObject<RenderGroup>(
            m_shader,
            renderable_attributes,
            RenderGroupFlags::NONE
        );
    }

    m_render_group->AddFramebuffer(m_framebuffer);

    InitObject(m_render_group);
}

void FullScreenPass::CreateDescriptors()
{
}

void FullScreenPass::Record(uint frame_index)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const CommandBufferRef &command_buffer = m_command_buffers[frame_index];

    auto record_result = command_buffer->Record(
        g_engine->GetGPUInstance()->GetDevice(),
        m_render_group->GetPipeline()->GetRenderPass(),
        [this, frame_index](CommandBuffer *cmd)
        {
            m_render_group->GetPipeline()->SetPushConstants(m_push_constant_data.Data(), m_push_constant_data.Size());
            m_render_group->GetPipeline()->Bind(cmd);

            m_render_group->GetPipeline()->GetDescriptorTable()->Bind<GraphicsPipelineRef>(
                cmd,
                frame_index,
                m_render_group->GetPipeline(),
                {
                    {
                        NAME("Scene"),
                        {
                            { NAME("ScenesBuffer"), HYP_SHADER_DATA_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                            { NAME("CamerasBuffer"), HYP_SHADER_DATA_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                            { NAME("EnvGridsBuffer"), HYP_SHADER_DATA_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                            { NAME("CurrentEnvProbe"), HYP_SHADER_DATA_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
                        }
                    }
                }
            );

            m_full_screen_quad->Render(cmd);

            HYPERION_RETURN_OK;
        });

    HYPERION_ASSERT_RESULT(record_result);
}

void FullScreenPass::Render(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const auto frame_index = frame->GetFrameIndex();

    m_framebuffer->BeginCapture(frame->GetCommandBuffer(), frame_index);

    const CommandBufferRef &secondary_command_buffer = m_command_buffers[frame_index];
    HYPERION_ASSERT_RESULT(secondary_command_buffer->SubmitSecondary(frame->GetCommandBuffer()));

    m_framebuffer->EndCapture(frame->GetCommandBuffer(), frame_index);
}

void FullScreenPass::Begin(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const uint frame_index = frame->GetFrameIndex();

    auto *command_buffer = m_command_buffers[frame_index].Get();

    command_buffer->Begin(g_engine->GetGPUDevice(), m_render_group->GetPipeline()->GetRenderPass());

    m_render_group->GetPipeline()->Bind(command_buffer);
}

void FullScreenPass::End(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    const uint frame_index = frame->GetFrameIndex();

    auto *command_buffer = m_command_buffers[frame_index].Get();
    command_buffer->End(g_engine->GetGPUDevice());

    m_framebuffer->BeginCapture(frame->GetCommandBuffer(), frame_index);
    HYPERION_ASSERT_RESULT(command_buffer->SubmitSecondary(frame->GetCommandBuffer()));
    m_framebuffer->EndCapture(frame->GetCommandBuffer(), frame_index);
}

} // namespace hyperion
