/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererFramebuffer.hpp>
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {

extern IRenderingAPI *g_rendering_api;

namespace renderer {

static inline VulkanRenderingAPI *GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI *>(g_rendering_api);
}

#pragma region VulkanAttachmentMap

RendererResult VulkanAttachmentMap::Create()
{
    for (KeyValuePair<uint32, VulkanAttachmentDef> &it : attachments) {
        VulkanAttachmentDef &def = it.second;

        AssertThrow(def.image.IsValid());

        if (!def.image->IsCreated()) {
            HYPERION_BUBBLE_ERRORS(def.image->Create());
        }

        AssertThrow(def.attachment.IsValid());

        if (!def.attachment->IsCreated()) {
            HYPERION_BUBBLE_ERRORS(def.attachment->Create());
        }
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanAttachmentMap::Resize(Vec2u new_size)
{
    for (KeyValuePair<uint32, VulkanAttachmentDef> &it : attachments) {
        VulkanAttachmentDef &def = it.second;

        AssertThrow(def.image.IsValid());

        TextureDesc texture_desc = def.image->GetTextureDesc();
        texture_desc.extent = Vec3u { new_size.x, new_size.y, 1 };

        VulkanImageRef new_image = MakeRenderObject<VulkanImage>(texture_desc);
        DeferCreate(new_image, ResourceState::SHADER_RESOURCE);

        VulkanAttachmentRef new_attachment = MakeRenderObject<VulkanAttachment>(
            new_image,
            def.attachment->GetRenderPassStage()
        );
        new_attachment->SetBinding(def.attachment->GetBinding());

        DeferCreate(new_attachment);

        if (def.image.IsValid()) {
            SafeRelease(std::move(def.image));
        }

        if (def.attachment.IsValid()) {
            SafeRelease(std::move(def.attachment));
        }

        def = VulkanAttachmentDef {
            std::move(new_image),
            std::move(new_attachment)
        };
    }

    HYPERION_RETURN_OK;
}

#pragma endregion VulkanAttachmentMap

#pragma region VulkanFramebuffer

VulkanFramebuffer::VulkanFramebuffer(
    Vec2u extent,
    RenderPassStage stage,
    uint32 num_multiview_layers
) : FramebufferBase(extent),
    m_handles { VK_NULL_HANDLE },
    m_render_pass(MakeRenderObject<VulkanRenderPass>(stage, RenderPassMode::RENDER_PASS_INLINE, num_multiview_layers))
{
}

VulkanFramebuffer::~VulkanFramebuffer()
{
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        AssertThrowMsg(m_handles[frame_index] == VK_NULL_HANDLE, "Expected framebuffer to have been destroyed");
    }
}

bool VulkanFramebuffer::IsCreated() const
{
    return m_handles[0] != VK_NULL_HANDLE;
}

RendererResult VulkanFramebuffer::Create()
{
    if (IsCreated()) {
        HYPERION_RETURN_OK;
    }

    HYPERION_BUBBLE_ERRORS(m_attachment_map.Create());
    
    for (const auto &it : m_attachment_map.attachments) {
        const VulkanAttachmentDef &def = it.second;

        AssertThrow(def.attachment.IsValid());
        m_render_pass->AddAttachment(def.attachment);
    }

    HYPERION_BUBBLE_ERRORS(m_render_pass->Create());

    Array<VkImageView> attachment_image_views;
    attachment_image_views.Reserve(m_attachment_map.attachments.Size());

    uint32 num_layers = 1;
    
    for (const auto &it : m_attachment_map.attachments) {
        AssertThrow(it.second.attachment != nullptr);
        AssertThrow(it.second.attachment->GetImageView() != nullptr);
        AssertThrow(it.second.attachment->GetImageView()->IsCreated());

        attachment_image_views.PushBack(static_cast<VulkanImageView *>(it.second.attachment->GetImageView().Get())->GetVulkanHandle());
    }

    VkFramebufferCreateInfo framebuffer_create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebuffer_create_info.renderPass = m_render_pass->GetVulkanHandle();
    framebuffer_create_info.attachmentCount = uint32(attachment_image_views.Size());
    framebuffer_create_info.pAttachments = attachment_image_views.Data();
    framebuffer_create_info.width = m_extent.x;
    framebuffer_create_info.height = m_extent.y;
    framebuffer_create_info.layers = num_layers;

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        HYPERION_VK_CHECK(vkCreateFramebuffer(
            GetRenderingAPI()->GetDevice()->GetDevice(),
            &framebuffer_create_info,
            nullptr,
            &m_handles[frame_index]
        ));
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanFramebuffer::Destroy()
{
    if (!IsCreated()) {
        HYPERION_RETURN_OK;
    }

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        if (m_handles[frame_index] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(GetRenderingAPI()->GetDevice()->GetDevice(), m_handles[frame_index], nullptr);
            m_handles[frame_index] = VK_NULL_HANDLE;
        }
    }

    SafeRelease(std::move(m_render_pass));

    m_attachment_map.Reset();

    HYPERION_RETURN_OK;
}

RendererResult VulkanFramebuffer::Resize(Vec2u new_size)
{
    if (m_extent == new_size) {
        HYPERION_RETURN_OK;
    }

    m_extent = new_size;

    if (!IsCreated()) {
        HYPERION_RETURN_OK;
    }

    HYPERION_BUBBLE_ERRORS(m_attachment_map.Resize(new_size));

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        if (m_handles[frame_index] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(GetRenderingAPI()->GetDevice()->GetDevice(), m_handles[frame_index], nullptr);
            m_handles[frame_index] = VK_NULL_HANDLE;
        }
    }

    Array<VkImageView> attachment_image_views;
    attachment_image_views.Reserve(m_attachment_map.attachments.Size());

    uint32 num_layers = 1;
    
    for (const auto &it : m_attachment_map.attachments) {
        AssertThrow(it.second.attachment != nullptr);
        AssertThrow(it.second.attachment->GetImageView() != nullptr);
        AssertThrow(it.second.attachment->GetImageView()->IsCreated());

        attachment_image_views.PushBack(static_cast<VulkanImageView *>(it.second.attachment->GetImageView().Get())->GetVulkanHandle());
    }

    VkFramebufferCreateInfo framebuffer_create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebuffer_create_info.renderPass = m_render_pass->GetVulkanHandle();
    framebuffer_create_info.attachmentCount = uint32(attachment_image_views.Size());
    framebuffer_create_info.pAttachments = attachment_image_views.Data();
    framebuffer_create_info.width = new_size.x;
    framebuffer_create_info.height = new_size.y;
    framebuffer_create_info.layers = num_layers;

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        HYPERION_VK_CHECK(vkCreateFramebuffer(
            GetRenderingAPI()->GetDevice()->GetDevice(),
            &framebuffer_create_info,
            nullptr,
            &m_handles[frame_index]
        ));
    }

    HYPERION_RETURN_OK;
}

AttachmentRef VulkanFramebuffer::AddAttachment(const AttachmentRef &attachment)
{
    return m_attachment_map.AddAttachment(VulkanAttachmentRef(attachment));
}

AttachmentRef VulkanFramebuffer::AddAttachment(
    uint32 binding,
    const ImageRef &image,
    LoadOperation load_op,
    StoreOperation store_op
)
{
    VulkanAttachmentRef attachment = MakeRenderObject<VulkanAttachment>(
        VulkanImageRef(image),
        m_render_pass->GetStage(),
        load_op,
        store_op
    );

    attachment->SetBinding(binding);

    return AddAttachment(attachment);
}

AttachmentRef VulkanFramebuffer::AddAttachment(
    uint32 binding,
    InternalFormat format,
    ImageType type,
    LoadOperation load_op,
    StoreOperation store_op
)
{
    return m_attachment_map.AddAttachment(
        binding,
        m_extent,
        format,
        type,
        m_render_pass->GetStage(),
        load_op,
        store_op
    );
}

bool VulkanFramebuffer::RemoveAttachment(uint32 binding)
{
    const auto it = m_attachment_map.attachments.Find(binding);

    if (it == m_attachment_map.attachments.End()) {
        return false;
    }

    SafeRelease(std::move(it->second.attachment));

    m_attachment_map.attachments.Erase(it);

    return true;
}

AttachmentBase *VulkanFramebuffer::GetAttachment(uint32 binding) const
{
    return m_attachment_map.GetAttachment(binding).Get();
}

void VulkanFramebuffer::BeginCapture(CommandBufferBase *command_buffer, uint32 frame_index)
{
    m_render_pass->Begin(static_cast<VulkanCommandBuffer *>(command_buffer), this, frame_index);
}

void VulkanFramebuffer::EndCapture(CommandBufferBase *command_buffer, uint32 frame_index)
{
    m_render_pass->End(static_cast<VulkanCommandBuffer *>(command_buffer));
}

#pragma endregion VulkanFramebuffer

} // namespace renderer
} // namespace hyperion
