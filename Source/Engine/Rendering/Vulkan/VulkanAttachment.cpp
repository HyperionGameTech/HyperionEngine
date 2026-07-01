/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <VulkanPch.hpp>

#include <Rendering/Vulkan/VulkanAttachment.hpp>
#include <Rendering/Vulkan/VulkanGpuImage.hpp>
#include <Rendering/Vulkan/VulkanGpuImageView.hpp>
#include <Rendering/Vulkan/VulkanFramebuffer.hpp>
#include <Rendering/Vulkan/VulkanHelpers.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Rendering/RenderInterface.hpp>

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
        m_imageView = MakeHandle<VulkanGpuImageView>(m_gpuImage);
#ifdef HYP_RHI_DEBUG_NAMES
        m_imageView->SetDebugName(NAME_FMT("{}_IV", m_gpuImage->GetDebugName()));
#endif
    }
}

VulkanAttachment::VulkanAttachment(
    const TextureDesc& textureDesc,
    const VulkanFramebufferWeakRef& framebuffer,
    RenderPassMode renderPassMode,
    const AttachmentDesc& attachmentDesc)
    : AttachmentBase(textureDesc, framebuffer, attachmentDesc),
      m_renderPassMode(renderPassMode),
      m_vkAttachmentReference {},
      m_vkAttachmentDescription {}
{
    Assert(m_gpuImage.IsValid());

    if (!m_imageView.IsValid())
    {
        m_imageView = MakeHandle<VulkanGpuImageView>(m_gpuImage);
#ifdef HYP_RHI_DEBUG_NAMES
        m_imageView->SetDebugName(NAME_FMT("{}_IV", m_gpuImage->GetDebugName()));
#endif
    }
}

VulkanAttachment::~VulkanAttachment()
{
    m_imageView.Reset();
}

bool VulkanAttachment::IsCreated() const
{
    return Texture::IsCreated() && m_imageView != nullptr && m_imageView->IsCreated();
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

    CheckResultOrReturn(m_imageView->Create());

    return Texture::Create();
}

#pragma endregion VulkanAttachment

} // namespace Hyperion
