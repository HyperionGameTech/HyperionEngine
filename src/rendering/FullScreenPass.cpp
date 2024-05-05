/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderGroup.hpp>

#include <Engine.hpp>
#include <Types.hpp>

#include <util/MeshBuilder.hpp>

namespace hyperion {

using renderer::CommandBufferType;
using renderer::FillMode;

#pragma region Render commands

struct RENDER_COMMAND(CreateCommandBuffers) : renderer::RenderCommand
{
    FixedArray<CommandBufferRef, max_frames_in_flight> command_buffers;

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

#pragma endregion Render commands

FullScreenPass::FullScreenPass()
    : FullScreenPass(InternalFormat::NONE, Extent2D { 0, 0 })
{
}

FullScreenPass::FullScreenPass(InternalFormat image_format, Extent2D extent)
    : FullScreenPass(nullptr, image_format, extent)
{
}

FullScreenPass::FullScreenPass(
    const ShaderRef &shader,
    const DescriptorTableRef &descriptor_table,
    InternalFormat image_format,
    Extent2D extent
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
    Extent2D extent
) : m_shader(shader),
    m_image_format(image_format),
    m_extent(extent),
    m_blend_function(BlendFunction::None())
{
}

FullScreenPass::~FullScreenPass() = default;

void FullScreenPass::SetBlendFunction(const BlendFunction &blend_function)
{
    m_blend_function = blend_function;
}

void FullScreenPass::Create()
{
    AssertThrowMsg(
        m_image_format != InternalFormat::NONE,
        "Image format must be set before creating the full screen pass"
    );

    CreateQuad();
    CreateCommandBuffers();
    CreateFramebuffer();
    CreatePipeline();
    CreateDescriptors();
}

void FullScreenPass::SetShader(const ShaderRef &shader)
{
    if (m_shader == shader) {
        return;
    }

    m_shader = shader;
}

void FullScreenPass::CreateQuad()
{
    m_full_screen_quad = MeshBuilder::Quad();
    InitObject(m_full_screen_quad);
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
    if (m_extent.Size() == 0) {
        // TODO: Make non render-thread dependent
        m_extent = g_engine->GetGPUInstance()->GetSwapchain()->extent;
    }

    m_framebuffer = MakeRenderObject<renderer::Framebuffer>(
        m_extent,
        renderer::RenderPassStage::SHADER,
        renderer::RenderPassMode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    ImageRef attachment_image = MakeRenderObject<Image>(renderer::FramebufferImage2D(
        m_extent,
        m_image_format,
        nullptr
    ));

    attachment_image->SetIsAttachmentTexture(true);

    DeferCreate(attachment_image, g_engine->GetGPUDevice());

    AttachmentRef attachment = MakeRenderObject<renderer::Attachment>(
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
            .flags          = MaterialAttributes::RAF_NONE
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

    g_engine->AddRenderGroup(m_render_group);
    InitObject(m_render_group);
}

void FullScreenPass::CreateDescriptors()
{
}

void FullScreenPass::Destroy()
{
    if (m_framebuffer.IsValid()) {
        if (m_render_group.IsValid()) {
            m_render_group->RemoveFramebuffer(m_framebuffer);
        }
    }

    m_render_group.Reset();
    m_full_screen_quad.Reset();

    SafeRelease(std::move(m_framebuffer));    
    SafeRelease(std::move(m_command_buffers));

    HYP_SYNC_RENDER();
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
                        HYP_NAME(Scene),
                        {
                            { HYP_NAME(ScenesBuffer), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                            { HYP_NAME(CamerasBuffer), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                            { HYP_NAME(LightsBuffer), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                            { HYP_NAME(EnvGridsBuffer), HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()) },
                            { HYP_NAME(CurrentEnvProbe), HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()) }
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
