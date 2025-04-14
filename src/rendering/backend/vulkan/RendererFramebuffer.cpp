/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererRenderPass.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

#pragma region AttachmentMap

template <>
RendererResult AttachmentMap<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    for (KeyValuePair<uint32, AttachmentDef<Platform::VULKAN>> &it : attachments) {
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

template <>
RendererResult AttachmentMap<Platform::VULKAN>::Resize(Device<Platform::VULKAN> *device, Vec2u new_size)
{
    for (KeyValuePair<uint32, AttachmentDef<Platform::VULKAN>> &it : attachments) {
        AttachmentDef<Platform::VULKAN> &def = it.second;

        AssertThrow(def.image.IsValid());

        ImageRef<Platform::VULKAN> new_image = MakeRenderObject<Image<Platform::VULKAN>>(
            TextureDesc {
                def.image->GetType(),
                def.image->GetTextureFormat(),
                Vec3u { new_size.x, new_size.y, 1 }
            }
        );

        new_image->SetIsAttachmentTexture(true);

        DeferCreate(new_image, device, ResourceState::SHADER_RESOURCE);

        AttachmentRef<Platform::VULKAN> new_attachment = MakeRenderObject<Attachment<Platform::VULKAN>>(
            new_image,
            def.attachment->GetRenderPassStage()
        );

        DeferCreate(new_attachment, device);

        if (def.image.IsValid()) {
            SafeRelease(std::move(def.image));
        }

        if (def.attachment.IsValid()) {
            SafeRelease(std::move(def.attachment));
        }

        def = AttachmentDef<Platform::VULKAN> {
            std::move(new_image),
            std::move(new_attachment)
        };
    }

    HYPERION_RETURN_OK;
}

#pragma endregion AttachmentMap

#pragma region Framebuffer

template <>
Framebuffer<Platform::VULKAN>::Framebuffer(
    Vec2u extent,
    RenderPassStage stage,
    RenderPassMode render_pass_mode,
    uint32 num_multiview_layers
) : m_platform_impl { this, VK_NULL_HANDLE },
    m_extent(extent),
    m_render_pass(MakeRenderObject<RenderPass<Platform::VULKAN>>(stage, render_pass_mode, num_multiview_layers))
{
}

template <>
Framebuffer<Platform::VULKAN>::~Framebuffer()
{
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        AssertThrowMsg(m_platform_impl.handles[frame_index] == VK_NULL_HANDLE, "Expected framebuffer to have been destroyed");
    }
}

template <>
bool Framebuffer<Platform::VULKAN>::IsCreated() const
{
    return m_platform_impl.handles[0] != VK_NULL_HANDLE;
}

template <>
RendererResult Framebuffer<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
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

    uint32 num_layers = 1;
    
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
    framebuffer_create_info.width = m_extent.x;
    framebuffer_create_info.height = m_extent.y;
    framebuffer_create_info.layers = num_layers;

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
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
RendererResult Framebuffer<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    if (!IsCreated()) {
        HYPERION_RETURN_OK;
    }

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
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
RendererResult Framebuffer<Platform::VULKAN>::Resize(Device<Platform::VULKAN> *device, Vec2u new_size)
{
    if (m_extent == new_size) {
        HYPERION_RETURN_OK;
    }

    m_extent = new_size;

    if (!IsCreated()) {
        HYPERION_RETURN_OK;
    }

    HYPERION_BUBBLE_ERRORS(m_attachment_map.Resize(device, new_size));

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        if (m_platform_impl.handles[frame_index] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device->GetDevice(), m_platform_impl.handles[frame_index], nullptr);
            m_platform_impl.handles[frame_index] = VK_NULL_HANDLE;
        }
    }

    Array<VkImageView> attachment_image_views;
    attachment_image_views.Reserve(m_attachment_map.attachments.Size());

    uint32 num_layers = 1;
    
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
    framebuffer_create_info.width = new_size.x;
    framebuffer_create_info.height = new_size.y;
    framebuffer_create_info.layers = num_layers;

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
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
bool Framebuffer<Platform::VULKAN>::RemoveAttachment(uint32 binding)
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
void Framebuffer<Platform::VULKAN>::BeginCapture(CommandBuffer<Platform::VULKAN> *command_buffer, uint32 frame_index)
{
    m_render_pass->Begin(command_buffer, this, frame_index);
}

template <>
void Framebuffer<Platform::VULKAN>::EndCapture(CommandBuffer<Platform::VULKAN> *command_buffer, uint32 frame_index)
{
    m_render_pass->End(command_buffer);
}

#pragma endregion Framebuffer

} // namespace platform
} // namespace renderer
} // namespace hyperion
