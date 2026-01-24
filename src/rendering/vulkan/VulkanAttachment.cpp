/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanAttachment.hpp>
#include <rendering/vulkan/VulkanGpuImage.hpp>
#include <rendering/vulkan/VulkanGpuImageView.hpp>
#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>

#include <rendering/util/SafeDeleter.hpp>

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
    m_imageView = MakeHandle<VulkanGpuImageView>(image);
}

VulkanAttachment::~VulkanAttachment()
{
    SafeDelete(std::move(m_image));
    SafeDelete(std::move(m_imageView));
}

bool VulkanAttachment::IsCreated() const
{
    return m_imageView != nullptr && m_imageView->IsCreated();
}

RendererResult VulkanAttachment::Create()
{
    Assert(m_image != nullptr && m_imageView != nullptr);

    m_vkAttachmentDescription = VkAttachmentDescription {
        .format = ToVkFormat(m_image->GetTextureFormat()),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = ToVkLoadOp(m_attachmentDesc.loadOp),
        .storeOp = ToVkStoreOp(m_attachmentDesc.storeOp),
        .stencilLoadOp = IsDepthAttachment() ? ToVkLoadOp(m_attachmentDesc.loadOp) : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = IsDepthAttachment() ? ToVkStoreOp(m_attachmentDesc.storeOp) : VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = GetInitialLayout(m_attachmentDesc.loadOp, IsDepthAttachment()),
        .finalLayout = GetFinalLayout(m_renderPassMode, IsDepthAttachment())
    };

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
