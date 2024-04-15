/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include "Framebuffer.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

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
    uint num_multiview_layers
) : Framebuffer(Extent3D(extent), stage, render_pass_mode, num_multiview_layers)
{
}

Framebuffer::Framebuffer(
    Extent3D extent,
    RenderPassStage stage,
    RenderPassMode render_pass_mode,
    uint num_multiview_layers
) : BasicObject(),
    m_extent(extent),
    m_attachment_map(new AttachmentMap)
{
    m_render_pass = MakeRenderObject<renderer::RenderPass>(stage, render_pass_mode, num_multiview_layers);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
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
    
    for (auto &it : m_attachment_map->attachments) {
        const uint binding = it.first;
        AttachmentDef &def = it.second;

        AssertThrow(def.attachment.IsValid());
        DeferCreate(def.attachment, g_engine->GetGPUDevice());

        def.attachment_usage = MakeRenderObject<AttachmentUsage>(
            def.attachment,
            def.load_op,
            def.store_op
        );

        DeferCreate(def.attachment_usage, g_engine->GetGPUDevice());
    }
    
    for (const auto &it : m_attachment_map->attachments) {
        const AttachmentDef &def = it.second;

        AssertThrow(def.attachment_usage.IsValid());
        m_render_pass->AddAttachmentUsage(def.attachment_usage);

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            AssertThrow(m_framebuffers[frame_index].IsValid());
            m_framebuffers[frame_index]->AddAttachmentUsage(def.attachment_usage);
        }
    }

    DeferCreate(m_render_pass, g_engine->GetGPUDevice());

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        AssertThrow(m_framebuffers[frame_index].IsValid());

        DeferCreate(m_framebuffers[frame_index], g_engine->GetGPUDevice(), m_render_pass);
    }

    SetReady(true);
}

void Framebuffer::AddAttachmentUsage(AttachmentUsageRef attachment_usage)
{
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_framebuffers[frame_index]->AddAttachmentUsage(attachment_usage);
    }

    m_render_pass->AddAttachmentUsage(attachment_usage);
}

void Framebuffer::RemoveAttachmentUsage(const AttachmentRef &attachment)
{
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_framebuffers[frame_index]->RemoveAttachmentUsage(attachment);
    }

    m_render_pass->RemoveAttachmentUsage(attachment);
}

void Framebuffer::BeginCapture(uint frame_index, CommandBuffer *command_buffer)
{
    AssertThrow(frame_index < max_frames_in_flight);

    m_render_pass->Begin(command_buffer, m_framebuffers[frame_index]);
}

void Framebuffer::EndCapture(uint frame_index, CommandBuffer *command_buffer)
{
    AssertThrow(frame_index < max_frames_in_flight);

    m_render_pass->End(command_buffer);
}

} // namespace hyperion::v2
