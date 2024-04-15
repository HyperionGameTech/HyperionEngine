/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererRenderPass.hpp>

#include <math/MathUtil.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
namespace platform {

FramebufferObject<Platform::VULKAN>::FramebufferObject(Extent2D extent)
    : FramebufferObject(Extent3D(extent))
{
}

FramebufferObject<Platform::VULKAN>::FramebufferObject(Extent3D extent)
    : m_extent(extent),
      m_handle(VK_NULL_HANDLE)
{
}

FramebufferObject<Platform::VULKAN>::~FramebufferObject()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE, "handle should have been destroyed");
}

Result FramebufferObject<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, RenderPass<Platform::VULKAN> *render_pass)
{
    Array<VkImageView> attachment_image_views;
    attachment_image_views.Reserve(m_attachment_usages.Size());

    uint num_layers = 1;
    
    for (const AttachmentUsageRef<Platform::VULKAN> &attachment_usage : m_attachment_usages) {
        AssertThrow(attachment_usage != nullptr);
        AssertThrow(attachment_usage->GetImageView() != nullptr);
        AssertThrow(attachment_usage->GetImageView()->GetImageView() != nullptr);

        //num_layers = MathUtil::Max(num_layers, attachment_usage->GetImageView()->NumFaces());

        attachment_image_views.PushBack(attachment_usage->GetImageView()->GetImageView());
    }

    VkFramebufferCreateInfo framebuffer_create_info { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebuffer_create_info.renderPass      = render_pass->GetHandle();
    framebuffer_create_info.attachmentCount = uint32(attachment_image_views.Size());
    framebuffer_create_info.pAttachments    = attachment_image_views.Data();
    framebuffer_create_info.width           = m_extent.width;
    framebuffer_create_info.height          = m_extent.height;
    framebuffer_create_info.layers          = num_layers;

    HYPERION_VK_CHECK(vkCreateFramebuffer(device->GetDevice(), &framebuffer_create_info, nullptr, &m_handle));

    HYPERION_RETURN_OK;
}

Result FramebufferObject<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    Result result;

    vkDestroyFramebuffer(device->GetDevice(), m_handle, nullptr);
    m_handle = VK_NULL_HANDLE;

    SafeRelease(std::move(m_attachment_usages));

    return result;
}

void FramebufferObject<Platform::VULKAN>::AddAttachmentUsage(AttachmentUsageRef<Platform::VULKAN> attachment_usage)
{
    m_attachment_usages.PushBack(std::move(attachment_usage));
}

bool FramebufferObject<Platform::VULKAN>::RemoveAttachmentUsage(const AttachmentRef<Platform::VULKAN> &attachment)
{
    const auto it = m_attachment_usages.FindIf([&attachment](const AttachmentUsageRef<Platform::VULKAN> &item)
    {
        return item->GetAttachment() == attachment;
    });

    if (it == m_attachment_usages.End()) {
        return false;
    }

    SafeRelease(std::move(*it));

    m_attachment_usages.Erase(it);

    return true;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
