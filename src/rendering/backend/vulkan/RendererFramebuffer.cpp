/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererRenderPass.hpp>

#include <math/MathUtil.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

#pragma region AttachmentMap

template <>
Result AttachmentMap<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    for (auto &it : attachments) {
        AttachmentDef<Platform::VULKAN> &def = it.second;

        AssertThrow(def.image.IsValid());

        if (!def.image->IsCreated()) {
            HYPERION_BUBBLE_ERRORS(def.image->Create(device));
        }

        AssertThrow(def.attachment.IsValid());

        if (!def.attachment->IsCreated()) {
            HYPERION_BUBBLE_ERRORS(def.attachment->Create(device));
        }
    }

    HYPERION_RETURN_OK;
}

#pragma endregion AttachmentMap

#pragma region Framebuffer

template <>
Framebuffer<Platform::VULKAN>::Framebuffer(
    Extent2D extent,
    RenderPassStage stage,
    RenderPassMode render_pass_mode,
    uint num_multiview_layers
) : m_platform_impl { this, VK_NULL_HANDLE },
    m_extent(extent),
    m_render_pass(MakeRenderObject<RenderPass<Platform::VULKAN>>(stage, render_pass_mode, num_multiview_layers))
{
}

template <>
Framebuffer<Platform::VULKAN>::~Framebuffer()
{
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        AssertThrowMsg(m_platform_impl.handles[frame_index] == VK_NULL_HANDLE, "Expected framebuffer to have been destroyed");
    }
}

template <>
bool Framebuffer<Platform::VULKAN>::IsCreated() const
{
    return m_platform_impl.handles[0] != VK_NULL_HANDLE;
}

template <>
Result Framebuffer<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    if (IsCreated()) {
        HYPERION_RETURN_OK;
    }

    HYPERION_BUBBLE_ERRORS(m_attachment_map.Create(device));
    
    for (const auto &it : m_attachment_map.attachments) {
        const AttachmentDef<Platform::VULKAN> &def = it.second;

        AssertThrow(def.attachment.IsValid());
        m_render_pass->AddAttachment(def.attachment);
    }

    HYPERION_BUBBLE_ERRORS(m_render_pass->Create(device));

    Array<VkImageView> attachment_image_views;
    attachment_image_views.Reserve(m_attachment_map.attachments.Size());

    uint num_layers = 1;
    
    for (const auto &it : m_attachment_map.attachments) {
        AssertThrow(it.second.attachment != nullptr);
        AssertThrow(it.second.attachment->GetImageView() != nullptr);
        AssertThrow(it.second.attachment->GetImageView()->IsCreated());

        attachment_image_views.PushBack(it.second.attachment->GetImageView()->GetPlatformImpl().handle);
    }

    VkFramebufferCreateInfo framebuffer_create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebuffer_create_info.renderPass = m_render_pass->GetHandle();
    framebuffer_create_info.attachmentCount = uint32(attachment_image_views.Size());
    framebuffer_create_info.pAttachments = attachment_image_views.Data();
    framebuffer_create_info.width = m_extent.width;
    framebuffer_create_info.height = m_extent.height;
    framebuffer_create_info.layers = num_layers;

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        HYPERION_VK_CHECK(vkCreateFramebuffer(
            device->GetDevice(),
            &framebuffer_create_info,
            nullptr,
            &m_platform_impl.handles[frame_index]
        ));
    }

    HYPERION_RETURN_OK;
}

template <>
Result Framebuffer<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    if (!IsCreated()) {
        HYPERION_RETURN_OK;
    }

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        if (m_platform_impl.handles[frame_index] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device->GetDevice(), m_platform_impl.handles[frame_index], nullptr);
            m_platform_impl.handles[frame_index] = VK_NULL_HANDLE;
        }
    }

    SafeRelease(std::move(m_render_pass));

    m_attachment_map.Reset();

    HYPERION_RETURN_OK;
}

template <>
bool Framebuffer<Platform::VULKAN>::RemoveAttachment(uint binding)
{
    const auto it = m_attachment_map.attachments.Find(binding);

    if (it == m_attachment_map.attachments.End()) {
        return false;
    }

    SafeRelease(std::move(it->second.attachment));

    m_attachment_map.attachments.Erase(it);

    return true;
}

template <>
void Framebuffer<Platform::VULKAN>::BeginCapture(CommandBuffer<Platform::VULKAN> *command_buffer, uint frame_index)
{
    m_render_pass->Begin(command_buffer, this, frame_index);
}

template <>
void Framebuffer<Platform::VULKAN>::EndCapture(CommandBuffer<Platform::VULKAN> *command_buffer, uint frame_index)
{
    m_render_pass->End(command_buffer);
}

#pragma endregion Framebuffer

} // namespace platform
} // namespace renderer
} // namespace hyperion
