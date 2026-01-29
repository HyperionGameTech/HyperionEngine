/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanGpuImageView.hpp>
#include <rendering/vulkan/VulkanGpuImage.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#include <rendering/vulkan/VulkanResult.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/debug/Debug.hpp>

#include <VulkanGpuImageView.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface* g_renderInterface;

#pragma region VulkanGpuImageView

VulkanGpuImageView::VulkanGpuImageView(const VulkanGpuImageRef& image)
    : GpuImageViewBase(image),
      m_handle(VK_NULL_HANDLE)
{
}

VulkanGpuImageView::VulkanGpuImageView(
    const VulkanGpuImageRef& image,
    const ImageSubResource& subResource)
    : GpuImageViewBase(image, subResource),
      m_handle(VK_NULL_HANDLE)
{
}

VulkanGpuImageView::~VulkanGpuImageView()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroyImageView(g_renderInterface->GetDevice()->GetDevice(), m_handle, nullptr);

        m_handle = VK_NULL_HANDLE;
    }
}

bool VulkanGpuImageView::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

RendererResult VulkanGpuImageView::Create()
{
    if (!m_image)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create image view on uninitialized image");
    }

    if (m_subResource.baseArrayLayer >= m_image->NumArrayLayers())
    {
        return HYP_MAKE_ERROR(RendererError, "Face index out of bounds");
    }

    if (m_subResource.baseMipLevel >= m_image->NumMips())
    {
        return HYP_MAKE_ERROR(RendererError, "Mip index out of bounds");
    }

    Assert(static_cast<const VulkanGpuImage*>(m_image.Get())->GetVulkanHandle() != VK_NULL_HANDLE);

    VkImageViewCreateInfo viewInfo { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image = static_cast<const VulkanGpuImage*>(m_image.Get())->GetVulkanHandle();
    viewInfo.viewType = ToVkImageViewType(m_image->GetType());
    viewInfo.format = ToVkFormat(m_image->GetTextureFormat());

    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewInfo.subresourceRange.aspectMask = ToVkImageAspect(m_image->GetTextureFormat());
    viewInfo.subresourceRange.baseMipLevel = m_subResource.baseMipLevel;
    viewInfo.subresourceRange.levelCount = m_subResource.numLevels != 0 ? m_subResource.numLevels : m_image->NumMips();
    viewInfo.subresourceRange.baseArrayLayer = m_subResource.baseArrayLayer;
    viewInfo.subresourceRange.layerCount = m_subResource.numLayers != 0 ? m_subResource.numLayers : m_image->NumArrayLayers();

    VULKAN_CHECK_MSG(
        vkCreateImageView(g_renderInterface->GetDevice()->GetDevice(), &viewInfo, nullptr, &m_handle),
        "Failed to create image view");

    return {};
}

#ifdef HYP_DEBUG_MODE

void VulkanGpuImageView::SetDebugName(Name name)
{
    GpuImageViewBase::SetDebugName(name);

    if (!IsCreated())
    {
        return;
    }

    const char* strName = name.LookupString();

    VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
    objectNameInfo.objectHandle = (uint64)m_handle;
    objectNameInfo.pObjectName = strName;

    g_vulkanDynamicFunctions->vkSetDebugUtilsObjectNameEXT(g_renderInterface->GetDevice()->GetDevice(), &objectNameInfo);
}

#endif

#pragma endregion VulkanGpuImageView

} // namespace Hyperion
