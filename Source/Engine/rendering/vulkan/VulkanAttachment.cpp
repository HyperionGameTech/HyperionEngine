/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanAttachment.hpp>
#include <rendering/vulkan/VulkanGpuImage.hpp>
#include <rendering/vulkan/VulkanGpuImageView.hpp>
#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <rendering/RenderInterface.hpp>

#include <VulkanAttachment.generated.inl>

namespace Hyperion {

#pragma region Helpers

extern VkImageLayout GetVkImageLayout(ResourceState state);

#pragma endregion Helpers

#pragma region VulkanAttachment

VulkanAttachment::VulkanAttachment(
    const VulkanGpuImageRef& image,
    const VulkanFramebufferWeakRef& framebuffer,
    VulkanRenderPassMode renderPassMode,
    const AttachmentDesc& attachmentDesc)
    : AttachmentBase(image, framebuffer, attachmentDesc),
      m_renderPassMode(renderPassMode),
      m_vkAttachmentReference {},
      m_vkAttachmentDescription {}
{
    if (image.IsValid())
    {
        m_imageView = MakeHandle<VulkanGpuImageView>(image);
#if HYP_DEBUG_MODE
        m_imageView->SetDebugName(NAME_FMT("{}_IV", image->GetDebugName()));
#endif
    }
}

VulkanAttachment::~VulkanAttachment()
{
    m_image.Reset();
    m_imageView.Reset();
}

bool VulkanAttachment::IsCreated() const
{
    return m_imageView != nullptr && m_imageView->IsCreated();
}

RendererResult VulkanAttachment::Create()
{
    Assert(m_image != nullptr && m_imageView != nullptr);

    m_vkAttachmentDescription = ToVkAttachmentDescription(m_attachmentDesc, m_renderPassMode);

    AssertDebug(HasBinding());

    m_vkAttachmentReference = VkAttachmentReference {
        .attachment = m_binding,
        .layout = GetIntermediateLayout(IsDepthAttachment())
    };

    if (!m_image->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Image is expected to be initialized before initializing attachment");
    }

    return m_imageView->Create();
}

#pragma endregion VulkanAttachment

} // namespace Hyperion
