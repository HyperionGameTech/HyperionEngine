/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/FullScreenPass.hpp>
#include <Engine.hpp>
#include <Types.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>
#include <util/MeshBuilder.hpp>

namespace hyperion::v2 {

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
            HYPERION_BUBBLE_ERRORS(command_buffers[i]->Create(
                g_engine->GetGPUDevice(),
                g_engine->GetGPUDevice()->GetGraphicsQueue().command_pools[0]
            ));
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

FullScreenPass::FullScreenPass(InternalFormat image_format, Extent2D extent)
    : FullScreenPass(Handle<Shader>(), image_format, extent)
{
}

FullScreenPass::FullScreenPass(
    const Handle<Shader> &shader,
    DescriptorTableRef descriptor_table,
    InternalFormat image_format,
    Extent2D extent
) : FullScreenPass(
        shader,
        image_format,
        extent
    )
{
    m_descriptor_table.Set(std::move(descriptor_table));
}

FullScreenPass::FullScreenPass(
    const Handle<Shader> &shader,
    InternalFormat image_format,
    Extent2D extent
) : m_shader(shader),
    m_image_format(image_format),
    m_extent(extent)
{
}

FullScreenPass::~FullScreenPass() = default;

void FullScreenPass::Create()
{
    InitObject(m_shader);

    CreateQuad();
    CreateCommandBuffers();
    CreateFramebuffer();
    CreatePipeline();
    CreateDescriptors();

    // HYP_SYNC_RENDER();
}

void FullScreenPass::SetShader(const Handle<Shader> &shader)
{
    if (m_shader == shader) {
        return;
    }

    m_shader = std::move(shader);
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

    m_framebuffer = CreateObject<Framebuffer>(
        m_extent,
        renderer::RenderPassStage::SHADER,
        renderer::RenderPassMode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    AttachmentRef attachment = MakeRenderObject<renderer::Attachment>(
        MakeRenderObject<Image>(renderer::FramebufferImage2D(
            m_extent,
            m_image_format,
            nullptr
        )),
        renderer::RenderPassStage::SHADER
    );

    DeferCreate(attachment, g_engine->GetGPUInstance()->GetDevice());
    m_attachments.PushBack(attachment);

    auto attachment_usage = MakeRenderObject<renderer::AttachmentUsage>(
        attachment,
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    DeferCreate(attachment_usage, g_engine->GetGPUInstance()->GetDevice());
    m_framebuffer->AddAttachmentUsage(attachment_usage);

    InitObject(m_framebuffer);
}

void FullScreenPass::CreatePipeline()
{
    CreatePipeline(RenderableAttributeSet(
        MeshAttributes {
            .vertex_attributes = static_mesh_vertex_attributes
        },
        MaterialAttributes {
            .fill_mode  = FillMode::FILL,
            .flags      = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
        }
    ));
}

void FullScreenPass::CreatePipeline(const RenderableAttributeSet &renderable_attributes)
{
    if (m_descriptor_table.HasValue()) {
        m_render_group = CreateObject<RenderGroup>(
            Handle<Shader>(m_shader),
            renderable_attributes,
            m_descriptor_table.Get()
        );
    } else {
        m_render_group = CreateObject<RenderGroup>(
            Handle<Shader>(m_shader),
            renderable_attributes
        );
    }

    m_render_group->AddFramebuffer(Handle<Framebuffer>(m_framebuffer));

    g_engine->AddRenderGroup(m_render_group);
    InitObject(m_render_group);
}

void FullScreenPass::CreateDescriptors()
{
}

void FullScreenPass::Destroy()
{
    for (auto &attachment : m_attachments) {
        m_framebuffer->RemoveAttachmentUsage(attachment);
    }

    SafeRelease(std::move(m_attachments));

    if (m_render_group) {
        m_render_group->RemoveFramebuffer(m_framebuffer->GetID());
    }

    m_framebuffer.Reset();
    m_render_group.Reset();
    m_full_screen_quad.Reset();

    SafeRelease(std::move(m_command_buffers));

    HYP_SYNC_RENDER();
}

void FullScreenPass::Record(uint frame_index)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const CommandBufferRef &command_buffer = m_command_buffers[frame_index];

    auto record_result = command_buffer->Record(
        g_engine->GetGPUInstance()->GetDevice(),
        m_render_group->GetPipeline()->GetConstructionInfo().render_pass,
        [this, frame_index](CommandBuffer *cmd)
        {
            m_render_group->GetPipeline()->push_constants = m_push_constant_data;
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
    Threads::AssertOnThread(THREAD_RENDER);

    const auto frame_index = frame->GetFrameIndex();

    m_framebuffer->BeginCapture(frame_index, frame->GetCommandBuffer());

    auto *secondary_command_buffer = m_command_buffers[frame_index].Get();
    HYPERION_ASSERT_RESULT(secondary_command_buffer->SubmitSecondary(frame->GetCommandBuffer()));

    m_framebuffer->EndCapture(frame_index, frame->GetCommandBuffer());
}

void FullScreenPass::Begin(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const uint frame_index = frame->GetFrameIndex();

    auto *command_buffer = m_command_buffers[frame_index].Get();

    command_buffer->Begin(g_engine->GetGPUDevice(), m_render_group->GetPipeline()->GetConstructionInfo().render_pass);

    m_render_group->GetPipeline()->Bind(command_buffer);
}

void FullScreenPass::End(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const uint frame_index = frame->GetFrameIndex();

    auto *command_buffer = m_command_buffers[frame_index].Get();
    command_buffer->End(g_engine->GetGPUDevice());

    m_framebuffer->BeginCapture(frame_index, frame->GetCommandBuffer());
    HYPERION_ASSERT_RESULT(command_buffer->SubmitSecondary(frame->GetCommandBuffer()));
    m_framebuffer->EndCapture(frame_index, frame->GetCommandBuffer());
}

} // namespace hyperion::v2
