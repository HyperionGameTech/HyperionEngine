#include "Framebuffer.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

#pragma region Render commands

struct RENDER_COMMAND(CreateRenderPass) : renderer::RenderCommand
{
    RenderPassRef   render_pass;
    AttachmentMap   *attachment_map;

    RENDER_COMMAND(CreateRenderPass)(RenderPassRef render_pass, AttachmentMap *attachment_map)
        : render_pass(std::move(render_pass)),
          attachment_map(attachment_map)
    {
        AssertThrow(this->render_pass.IsValid());
        AssertThrow(attachment_map != nullptr);
    }

    virtual Result operator()()
    {
        for (const auto &it : attachment_map->attachments) {
            const AttachmentDef &def = it.second;

            AssertThrow(def.attachment_usage.IsValid());
            render_pass->AddAttachmentUsage(def.attachment_usage.Get());
        }

        return render_pass->Create(g_engine->GetGPUDevice());
    }
};

struct RENDER_COMMAND(CreateFramebuffer) : renderer::RenderCommand
{
    FramebufferObjectRef    framebuffer;
    RenderPassRef           render_pass;
    AttachmentMap           *attachment_map;

    RENDER_COMMAND(CreateFramebuffer)(FramebufferObjectRef framebuffer, RenderPassRef render_pass, AttachmentMap *attachment_map)
        : framebuffer(std::move(framebuffer)),
          render_pass(std::move(render_pass)),
          attachment_map(attachment_map)
    {
        AssertThrow(this->framebuffer.IsValid());
        AssertThrow(this->render_pass.IsValid());
        AssertThrow(attachment_map != nullptr);
    }

    virtual Result operator()()
    {
        AssertThrow(framebuffer.IsValid());

        for (const auto &it : attachment_map->attachments) {
            const AttachmentDef &def = it.second;

            AssertThrow(def.attachment_usage.IsValid());
            framebuffer->AddAttachmentUsage(def.attachment_usage.Get());
        }

        return framebuffer->Create(g_engine->GetGPUDevice(), render_pass);
    }
};

struct RENDER_COMMAND(CreateAttachmentMap) : renderer::RenderCommand
{
    AttachmentMap *attachment_map;

    RENDER_COMMAND(CreateAttachmentMap)(AttachmentMap *attachment_map)
        : attachment_map(attachment_map)
    {
        AssertThrow(attachment_map != nullptr);
    }

    virtual Result operator()()
    {
        for (auto &it : attachment_map->attachments) {
            const UInt binding = it.first;
            AttachmentDef &def = it.second;

            AssertThrow(def.attachment.IsValid());
            HYPERION_BUBBLE_ERRORS(def.attachment->Create(g_engine->GetGPUDevice()));

            def.attachment_usage = MakeRenderObject<AttachmentUsage>(
                def.attachment.Get(),
                def.load_op,
                def.store_op
            );

            def.attachment->AddAttachmentUsage(def.attachment_usage);

            HYPERION_BUBBLE_ERRORS(def.attachment_usage->Create(g_engine->GetGPUDevice()));
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

AttachmentMap::~AttachmentMap()
{
    for (auto &it : attachments) {
        SafeRelease(std::move(it.second.attachment));
        SafeRelease(std::move(it.second.attachment_usage));
    }
}

Framebuffer::Framebuffer(
    Extent2D extent,
    RenderPassStage stage,
    RenderPassMode render_pass_mode,
    UInt num_multiview_layers
) : Framebuffer(Extent3D(extent), stage, render_pass_mode, num_multiview_layers)
{
}

Framebuffer::Framebuffer(
    Extent3D extent,
    RenderPassStage stage,
    RenderPassMode render_pass_mode,
    UInt num_multiview_layers
) : BasicObject(),
    m_extent(extent)
{
    m_render_pass = MakeRenderObject<renderer::RenderPass>(stage, render_pass_mode, num_multiview_layers);

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_framebuffers[frame_index] = MakeRenderObject<renderer::FramebufferObject>(m_extent);
    }
}

Framebuffer::~Framebuffer()
{
    SafeRelease(std::move(m_framebuffers));
    SafeRelease(std::move(m_render_pass));
}

void Framebuffer::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();
    
    PUSH_RENDER_COMMAND(CreateAttachmentMap, &m_attachment_map);
    PUSH_RENDER_COMMAND(CreateRenderPass, m_render_pass, &m_attachment_map);

    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        AssertThrow(m_framebuffers[frame_index].IsValid());

        PUSH_RENDER_COMMAND(CreateFramebuffer, m_framebuffers[frame_index], m_render_pass, &m_attachment_map);
    }

    HYP_SYNC_RENDER();

    SetReady(true);
}

void Framebuffer::AddAttachmentUsage(AttachmentUsage *attachment_usage)
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_framebuffers[frame_index]->AddAttachmentUsage(attachment_usage);
    }

    m_render_pass->AddAttachmentUsage(attachment_usage);
}

void Framebuffer::RemoveAttachmentUsage(const Attachment *attachment)
{
    for (UInt frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_framebuffers[frame_index]->RemoveAttachmentUsage(attachment);
    }

    m_render_pass->RemoveAttachmentUsage(attachment);
}

void Framebuffer::BeginCapture(UInt frame_index, CommandBuffer *command_buffer)
{
    AssertThrow(frame_index < max_frames_in_flight);

    m_render_pass->Begin(command_buffer, m_framebuffers[frame_index]);
}

void Framebuffer::EndCapture(UInt frame_index, CommandBuffer *command_buffer)
{
    AssertThrow(frame_index < max_frames_in_flight);

    m_render_pass->End(command_buffer);
}

} // namespace hyperion::v2
