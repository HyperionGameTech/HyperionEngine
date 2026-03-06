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

#pragma region VulkanAttachment

VulkanAttachment::VulkanAttachment(
    const VulkanGpuImageRef& image,
    const VulkanGpuImageViewRef& imageView,
    const VulkanFramebufferWeakRef& framebuffer,
    RenderPassMode renderPassMode,
    const AttachmentDesc& attachmentDesc)
    : AttachmentBase(image, imageView, framebuffer, attachmentDesc),
      m_renderPassMode(renderPassMode),
      m_vkAttachmentReference {},
      m_vkAttachmentDescription {}
{
    Assert(m_gpuImage.IsValid());

    if (!m_imageView.IsValid())
    {
        m_imageView = MakeHandle<VulkanGpuImageView>(image);
#if HYP_DEBUG_MODE
        m_imageView->SetDebugName(NAME_FMT("{}_IV", image->GetDebugName()));
#endif
    }
}

VulkanAttachment::~VulkanAttachment()
{
    m_imageView.Reset();
}

bool VulkanAttachment::IsCreated() const
{
    return m_imageView != nullptr && m_imageView->IsCreated();
}

RendererResult VulkanAttachment::Create()
{
    Assert(m_gpuImage != nullptr && m_imageView != nullptr);

    m_vkAttachmentDescription = ToVkAttachmentDescription(m_attachmentDesc, m_renderPassMode);

    AssertDebug(HasBinding());

    m_vkAttachmentReference = VkAttachmentReference {};
    m_vkAttachmentReference.attachment = m_binding;
    m_vkAttachmentReference.layout = GetIntermediateLayout(
        IsDepthAttachment(),
        TextureUtils::HasStencilComponent(m_attachmentDesc.format),
        m_attachmentDesc.onlyDepth,
        m_attachmentDesc.onlyStencil);

    if (!m_gpuImage->IsCreated())
    {
        CheckResultOrReturn(m_gpuImage->Create());
    }

    return m_imageView->Create();
}

#pragma endregion VulkanAttachment

} // namespace Hyperion
